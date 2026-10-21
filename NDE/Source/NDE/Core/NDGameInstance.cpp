// No Despiertes Eso

#include "Core/NDGameInstance.h"

#include "Core/NDLog.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "Networking/NDExpeditionGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Networking/NDSessionSubsystem.h"
#include "Players/NDPlayerController.h"
#include "TimerManager.h"

namespace
{
	/** Margen para que el aviso de cierre llegue a los clientes antes de cortar la conexión. */
	constexpr float HostCloseDelaySeconds = 0.5f;
}

void UNDGameInstance::Init()
{
	Super::Init();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UNDGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Shutdown();
}

void UNDGameInstance::OnStart()
{
	Super::OnStart();

	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->BindPlatformDelegates();
	}
}

void UNDGameInstance::ReturnToMainMenuWithReason(ENDDisconnectReason Reason)
{
	if (bReturningToMenu)
	{
		return;
	}
	bReturningToMenu = true;
	LastDisconnectReason = Reason;
	ExpectedExpeditionPlayers = 0;
	RememberExpeditionForRejoin(Reason);

	// Como host, avisamos a los clientes antes de cerrar para que sepan que la partida terminó.
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer && World->GetNumPlayerControllers() > 1)
	{
		NotifyClientsHostClosing();
		GetTimerManager().SetTimer(HostCloseTimer, this, &ThisClass::OpenMainMenuMap, HostCloseDelaySeconds, false);
		return;
	}
	OpenMainMenuMap();
}

void UNDGameInstance::NotifyClientsHostClosing()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ANDPlayerController* PC = Cast<ANDPlayerController>(It->Get());
		if (PC && !PC->IsLocalController())
		{
			PC->ClientHostClosedSession();
		}
	}
}

void UNDGameInstance::OpenMainMenuMap()
{
	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->DestroySession();
	}

	const FString MenuPackage = UNDNetworkSettings::Get()->MainMenuMap.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogNDNet, Log, TEXT("Volviendo al menu (%s)"), *UEnum::GetValueAsString(LastDisconnectReason));
	if (!MenuPackage.IsEmpty())
	{
		// Abrir un mapa sin ?listen cierra el servidor o la conexión al host.
		UGameplayStatics::OpenLevel(this, FName(*MenuPackage), true);
	}
	else
	{
		UE_LOG(LogNDNet, Error, TEXT("MainMenuMap no configurado en Project Settings > NDE Red."));
		bReturningToMenu = false;
	}
}

void UNDGameInstance::RememberExpeditionForRejoin(ENDDisconnectReason Reason)
{
	RejoinAddress.Reset();
	RejoinDeadline = 0.0;

	// Solo un cliente que estaba en una expedición en marcha puede volver a ella.
	const UWorld* World = GetWorld();
	const ANDExpeditionGameState* ExpState = World ? World->GetGameState<ANDExpeditionGameState>() : nullptr;
	const bool bWasInExpedition = World && World->GetNetMode() == NM_Client
		&& ExpState && ExpState->GetSessionPhase() == ENDExpeditionSessionPhase::InProgress;
	// HostLeft llega solo cuando el host avisa que cierra: ya no hay partida a la que volver.
	const bool bReasonAllowsRejoin = Reason == ENDDisconnectReason::LeftVoluntarily
		|| Reason == ENDDisconnectReason::ConnectionLost;
	const UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>();
	if (!bWasInExpedition || !bReasonAllowsRejoin || !Sessions || Sessions->GetLastConnectString().IsEmpty())
	{
		return;
	}

	RejoinAddress = Sessions->GetLastConnectString();
	RejoinDeadline = FPlatformTime::Seconds() + UNDNetworkSettings::Get()->RejoinWindowSeconds;
	UE_LOG(LogNDNet, Log, TEXT("Se puede volver a la expedicion en %s durante %.0f s"), *RejoinAddress, UNDNetworkSettings::Get()->RejoinWindowSeconds);
}

bool UNDGameInstance::CanRejoinExpedition() const
{
	return !RejoinAddress.IsEmpty() && FPlatformTime::Seconds() < RejoinDeadline;
}

float UNDGameInstance::GetRejoinSecondsLeft() const
{
	return CanRejoinExpedition() ? static_cast<float>(RejoinDeadline - FPlatformTime::Seconds()) : 0.f;
}

bool UNDGameInstance::RejoinExpedition()
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!CanRejoinExpedition() || !PC)
	{
		LastDisconnectReason = ENDDisconnectReason::RejoinExpired;
		return false;
	}

	// El servidor reconoce al jugador por su id de plataforma (ANDExpeditionGameMode::PreLogin).
	const FString URL = FString::Printf(TEXT("%s?NDEVersion=%s"), *RejoinAddress, *UNDNetworkSettings::Get()->CompatibilityVersion);
	RejoinAddress.Reset();
	RejoinDeadline = 0.0;
	UE_LOG(LogNDNet, Log, TEXT("Volviendo a la expedicion: %s"), *URL);
	PC->ClientTravel(URL, TRAVEL_Absolute);
	return true;
}

void UNDGameInstance::NDNetSim(const FString& Preset)
{
#if !UE_BUILD_SHIPPING
	// Retardo por sentido (el ida y vuelta es el doble), variación y pérdida de paquetes.
	int32 Lag = 0;
	int32 Variance = 0;
	int32 Loss = 0;
	if (Preset.Equals(TEXT("good"), ESearchCase::IgnoreCase))
	{
		Lag = 15;
		Variance = 5;
	}
	else if (Preset.Equals(TEXT("medium"), ESearchCase::IgnoreCase))
	{
		Lag = 75;
		Variance = 15;
		Loss = 2;
	}
	else if (Preset.Equals(TEXT("bad"), ESearchCase::IgnoreCase))
	{
		Lag = 110;
		Variance = 40;
		Loss = 5;
	}
	else if (!Preset.Equals(TEXT("off"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogNDNet, Display, TEXT("Uso: NDNetSim off | good | medium | bad"));
		return;
	}

	UWorld* World = GetWorld();
	GEngine->Exec(World, *FString::Printf(TEXT("NetEmulation.PktLag %d"), Lag));
	GEngine->Exec(World, *FString::Printf(TEXT("NetEmulation.PktLagVariance %d"), Variance));
	GEngine->Exec(World, *FString::Printf(TEXT("NetEmulation.PktLoss %d"), Loss));

	const FString Message = FString::Printf(TEXT("Red simulada: %s (ida y vuelta ~%d ms, perdida %d%%)"), *Preset, Lag * 2, Loss);
	UE_LOG(LogNDNet, Display, TEXT("%s"), *Message);
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Yellow, Message);
#endif
}

void UNDGameInstance::NDRejoin()
{
	RejoinExpedition();
}

void UNDGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// El viaje al menú ya terminó; se aceptan nuevas salidas.
	if (LoadedWorld == GetWorld())
	{
		bReturningToMenu = false;
	}
}

void UNDGameInstance::NDHost(bool bFriendsOnly)
{
	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->HostSession(bFriendsOnly);
	}
}

void UNDGameInstance::NDFind()
{
	UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>();
	if (!Sessions)
	{
		return;
	}
	if (!Sessions->OnFindSessionsComplete.IsAlreadyBound(this, &ThisClass::LogFoundSessions))
	{
		Sessions->OnFindSessionsComplete.AddDynamic(this, &ThisClass::LogFoundSessions);
	}
	Sessions->FindSessions();
}

void UNDGameInstance::LogFoundSessions(const TArray<FNDSessionInfo>& Results, bool bWasSuccessful)
{
	UE_LOG(LogNDNet, Display, TEXT("Salas encontradas: %d (ok=%d)"), Results.Num(), bWasSuccessful);
	for (const FNDSessionInfo& Info : Results)
	{
		UE_LOG(LogNDNet, Display, TEXT("  [%d] %s  %d/%d  %d ms  version %s%s"),
			Info.SearchIndex, *Info.HostName, Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs,
			*Info.CompatibilityVersion, Info.bIsCompatible ? TEXT("") : TEXT("  (INCOMPATIBLE)"));
	}
}

void UNDGameInstance::NDJoin(int32 SearchIndex)
{
	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->JoinSessionByIndex(SearchIndex);
	}
}

void UNDGameInstance::NDInvite()
{
	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->ShowInviteUI();
	}
}

void UNDGameInstance::NDLeave()
{
	ReturnToMainMenuWithReason(ENDDisconnectReason::LeftVoluntarily);
}

FText UNDGameInstance::GetDisconnectReasonText(ENDDisconnectReason Reason)
{
	switch (Reason)
	{
	case ENDDisconnectReason::LeftVoluntarily:		return NSLOCTEXT("NDE", "ReasonLeft", "Saliste de la sala.");
	case ENDDisconnectReason::HostLeft:				return NSLOCTEXT("NDE", "ReasonHostLeft", "El host cerro la partida.");
	case ENDDisconnectReason::ConnectionLost:		return NSLOCTEXT("NDE", "ReasonConnection", "Se perdio la conexion con la partida.");
	case ENDDisconnectReason::TravelFailed:			return NSLOCTEXT("NDE", "ReasonTravel", "No se pudo cargar el mapa.");
	case ENDDisconnectReason::VersionMismatch:		return NSLOCTEXT("NDE", "ReasonVersion", "La sala usa otra version del juego.");
	case ENDDisconnectReason::SessionFull:			return NSLOCTEXT("NDE", "ReasonFull", "La sala esta llena.");
	case ENDDisconnectReason::JoinFailed:			return NSLOCTEXT("NDE", "ReasonJoin", "No se pudo entrar en la sala.");
	case ENDDisconnectReason::ExpeditionInProgress:	return NSLOCTEXT("NDE", "ReasonInProgress", "La banda esta en plena expedicion.");
	case ENDDisconnectReason::RejoinExpired:		return NSLOCTEXT("NDE", "ReasonRejoinExpired", "Paso el tiempo para volver a la partida.");
	default:										return FText::GetEmpty();
	}
}

ENDDisconnectReason UNDGameInstance::ConsumeLastDisconnectReason()
{
	const ENDDisconnectReason Reason = LastDisconnectReason;
	LastDisconnectReason = ENDDisconnectReason::None;
	return Reason;
}

void UNDGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (World != GetWorld())
	{
		return;
	}
	UE_LOG(LogNDNet, Warning, TEXT("Fallo de red %s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString);

	// Si el servidor rechazó por versión o sala llena, PreLogin ya dejó el motivo en el texto.
	// Si el host cerró a propósito, ClientHostClosedSession ya nos devolvió al menú y este aviso se ignora.
	ENDDisconnectReason Reason = ENDDisconnectReason::ConnectionLost;
	if (ErrorString.Contains(TEXT("NDE_VERSION")))
	{
		Reason = ENDDisconnectReason::VersionMismatch;
	}
	else if (ErrorString.Contains(TEXT("NDE_FULL")))
	{
		Reason = ENDDisconnectReason::SessionFull;
	}
	else if (ErrorString.Contains(TEXT("NDE_IN_PROGRESS")) || ErrorString.Contains(TEXT("NDE_TRAVELING")))
	{
		Reason = ENDDisconnectReason::ExpeditionInProgress;
	}
	else if (ErrorString.Contains(TEXT("NDE_REJOIN_EXPIRED")))
	{
		Reason = ENDDisconnectReason::RejoinExpired;
	}

	// Sin migración de host en listen server (GDD 21.6): si se va el host, se vuelve al menú.
	// TODO: restaurar checkpoint o compensación verificada cuando exista el servicio de perfil.
	ReturnToMainMenuWithReason(Reason);
}

void UNDGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNDNet, Warning, TEXT("Fallo de viaje %s: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	ReturnToMainMenuWithReason(ENDDisconnectReason::TravelFailed);
}
