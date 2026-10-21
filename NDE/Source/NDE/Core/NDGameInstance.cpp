// No Despiertes Eso

#include "Core/NDGameInstance.h"

#include "Core/NDLog.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "Networking/NDNetworkSettings.h"
#include "Networking/NDSessionSubsystem.h"

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

	if (UNDSessionSubsystem* Sessions = GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->DestroySession();
	}

	const FString MenuPackage = UNDNetworkSettings::Get()->MainMenuMap.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogNDNet, Log, TEXT("Volviendo al menu (%s)"), *UEnum::GetValueAsString(Reason));
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
	ENDDisconnectReason Reason = FailureType == ENetworkFailure::ConnectionLost
		? ENDDisconnectReason::HostLeft
		: ENDDisconnectReason::ConnectionLost;
	if (ErrorString.Contains(TEXT("NDE_VERSION")))
	{
		Reason = ENDDisconnectReason::VersionMismatch;
	}
	else if (ErrorString.Contains(TEXT("NDE_FULL")))
	{
		Reason = ENDDisconnectReason::SessionFull;
	}

	// TODO(fase 4): si cae el host durante una expedición, restaurar checkpoint (GDD 21.6).
	ReturnToMainMenuWithReason(Reason);
}

void UNDGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNDNet, Warning, TEXT("Fallo de viaje %s: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	ReturnToMainMenuWithReason(ENDDisconnectReason::TravelFailed);
}
