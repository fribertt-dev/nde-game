// No Despiertes Eso

#include "Networking/NDExpeditionGameMode.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "Networking/NDExpeditionGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Players/NDPlayerController.h"
#include "Players/NDPlayerState.h"
#include "TimerManager.h"

namespace NDExpeditionGameMode
{
	constexpr float LoadCheckInterval = 0.5f;
	/** Pausa breve en Settlement para que los clientes vean el resultado antes de viajar. */
	constexpr float SettlementToTravelSeconds = 3.f;
}

ANDExpeditionGameMode::ANDExpeditionGameMode()
{
	GameStateClass = ANDExpeditionGameState::StaticClass();
	PlayerStateClass = ANDPlayerState::StaticClass();
	PlayerControllerClass = ANDPlayerController::StaticClass();
	bUseSeamlessTravel = true;
}

void ANDExpeditionGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	// TODO(fase 4): entrar a mitad de expedición como espectador y rejoin (GDD 21.6).
	// En el editor se permite para probar el mapa de expedición directamente con varios clientes PIE.
	if (ErrorMessage.IsEmpty() && !GIsEditor)
	{
		ErrorMessage = TEXT("NDE_IN_PROGRESS: la banda esta en plena expedicion.");
	}
}

void ANDExpeditionGameMode::StartPlay()
{
	Super::StartPlay();

	if (ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>())
	{
		ExpState->SetRunGuid(FGuid::NewGuid());
		UE_LOG(LogNDNet, Log, TEXT("Expedicion %s: esperando jugadores"), *ExpState->GetRunGuid().ToString());
	}

	LoadingStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(LoadCheckTimer, this, &ThisClass::CheckAllPlayersLoaded, NDExpeditionGameMode::LoadCheckInterval, true);
}

void ANDExpeditionGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	CheckAllPlayersLoaded();
}

void ANDExpeditionGameMode::CheckAllPlayersLoaded()
{
	const ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	if (!ExpState || ExpState->GetSessionPhase() != ENDExpeditionSessionPhase::Loading)
	{
		return;
	}

	const UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	// Sin dato del lobby (p. ej. abrir el mapa directamente en PIE) basta con un jugador.
	const int32 Expected = FMath::Max(1, GI ? GI->GetExpectedExpeditionPlayers() : 1);
	const bool bEveryoneArrived = GetNumPlayers() >= Expected;
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - LoadingStartTime >= UNDNetworkSettings::Get()->InsertionLoadTimeoutSeconds;

	if (bEveryoneArrived || bTimedOut)
	{
		if (bTimedOut && !bEveryoneArrived)
		{
			UE_LOG(LogNDNet, Warning, TEXT("Tiempo de carga agotado: %d de %d jugadores"), GetNumPlayers(), Expected);
		}
		BeginExpedition();
	}
}

void ANDExpeditionGameMode::BeginExpedition()
{
	GetWorldTimerManager().ClearTimer(LoadCheckTimer);
	if (ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>())
	{
		ExpState->SetSessionPhase(ENDExpeditionSessionPhase::InProgress);
		UE_LOG(LogNDNet, Log, TEXT("Expedicion en marcha con %d jugadores"), GetNumPlayers());
	}
}

void ANDExpeditionGameMode::EndExpedition()
{
	ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	if (bSettlementStarted || !ExpState)
	{
		return;
	}
	bSettlementStarted = true;
	GetWorldTimerManager().ClearTimer(LoadCheckTimer);

	// TODO(integración): guardar en UNDProfileSubsystem el FNDRunSummary de OnExpeditionSettled.
	ExpState->SetSessionPhase(ENDExpeditionSessionPhase::Settlement);
	GetWorldTimerManager().SetTimer(ReturnTimer, this, &ThisClass::TravelToLobby, NDExpeditionGameMode::SettlementToTravelSeconds, false);
}

void ANDExpeditionGameMode::TravelToLobby()
{
	const FString LobbyPackage = UNDNetworkSettings::Get()->LobbyMap.ToSoftObjectPath().GetLongPackageName();
	if (LobbyPackage.IsEmpty())
	{
		UE_LOG(LogNDNet, Error, TEXT("LobbyMap no configurado en Project Settings > NDE Red."));
		return;
	}

	if (ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>())
	{
		ExpState->SetSessionPhase(ENDExpeditionSessionPhase::ReturningToBase);
	}
	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		GI->SetExpectedExpeditionPlayers(0);
	}

	UE_LOG(LogNDNet, Log, TEXT("Volviendo a La Madriguera"));
	GetWorld()->ServerTravel(LobbyPackage + TEXT("?game=Lobby"));
}
