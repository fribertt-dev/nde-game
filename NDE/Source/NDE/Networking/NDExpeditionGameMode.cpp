// No Despiertes Eso

#include "Networking/NDExpeditionGameMode.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Networking/NDExpeditionGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Players/NDCharacter.h"
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
	DefaultPawnClass = ANDCharacter::StaticClass();
	bUseSeamlessTravel = true;
}

void ANDExpeditionGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	DefaultPawnClass = ANDCharacter::GetDesveladoClass();
}

void ANDExpeditionGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	// En el editor se permite para probar el mapa de expedición directamente con varios clientes PIE.
	if (!ErrorMessage.IsEmpty() || GIsEditor)
	{
		return;
	}

	// Solo pueden entrar quienes ya estaban en esta expedición y se cayeron hace poco.
	// TODO: entrar como espectador si el lobby lo permite (GDD 21.6).
	if (IsWithinRejoinWindow(UniqueId))
	{
		return;
	}
	ErrorMessage = DisconnectedPlayers.Contains(UniqueId.ToString())
		? TEXT("NDE_REJOIN_EXPIRED: paso el tiempo para volver a la partida.")
		: TEXT("NDE_IN_PROGRESS: la banda esta en plena expedicion.");
}

bool ANDExpeditionGameMode::IsWithinRejoinWindow(const FUniqueNetIdRepl& UniqueId) const
{
	const double* DroppedAt = UniqueId.IsValid() ? DisconnectedPlayers.Find(UniqueId.ToString()) : nullptr;
	return DroppedAt && GetWorld()->GetTimeSeconds() - *DroppedAt <= UNDNetworkSettings::Get()->RejoinWindowSeconds;
}

void ANDExpeditionGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	APlayerState* PS = NewPlayer ? NewPlayer->PlayerState : nullptr;
	if (PS && DisconnectedPlayers.Remove(PS->GetUniqueId().ToString()) > 0)
	{
		UE_LOG(LogNDNet, Log, TEXT("%s volvio a la expedicion"), *PS->GetPlayerName());
		OnPlayerRejoined.Broadcast(PS);
	}
}

void ANDExpeditionGameMode::Logout(AController* Exiting)
{
	const ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	APlayerState* PS = Exiting ? Exiting->PlayerState : nullptr;
	const bool bInProgress = ExpState && ExpState->GetSessionPhase() == ENDExpeditionSessionPhase::InProgress;

	// El host no se cuenta: si se va él, se cierra la partida entera.
	const APlayerController* PC = Cast<APlayerController>(Exiting);
	if (bInProgress && PS && PC && !PC->IsLocalController())
	{
		UE_LOG(LogNDNet, Log, TEXT("%s se desconecto en plena expedicion; tiene %.0f s para volver"),
			*PS->GetPlayerName(), UNDNetworkSettings::Get()->RejoinWindowSeconds);
		if (PS->GetUniqueId().IsValid())
		{
			DisconnectedPlayers.Add(PS->GetUniqueId().ToString(), GetWorld()->GetTimeSeconds());
		}
		OnPlayerDisconnected.Broadcast(PS);
	}

	Super::Logout(Exiting);
}

void ANDExpeditionGameMode::StartPlay()
{
	Super::StartPlay();

	if (ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>())
	{
		ExpState->SetRunGuid(FGuid::NewGuid());
		ExpState->SetContractId(FName(*UGameplayStatics::ParseOption(OptionsString, TEXT("Contract"))));
		UE_LOG(LogNDNet, Log, TEXT("Expedicion %s, contrato %s: esperando jugadores"),
			*ExpState->GetRunGuid().ToString(), *ExpState->GetContractId().ToString());
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

void ANDExpeditionGameMode::GrantRunReward(APlayerController* Player, int32 Coins, int32 Reputation)
{
	const ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	ANDPlayerController* PC = Cast<ANDPlayerController>(Player);
	if (!PC || !ExpState)
	{
		return;
	}
	// El perfil es local (GDD 21.8 opción C): cada jugador guarda lo suyo en su PC.
	PC->ClientReceiveRunReward(ExpState->GetRunGuid(), Coins, Reputation);
}

void ANDExpeditionGameMode::GrantRunRewardToAll(int32 Coins, int32 Reputation)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		GrantRunReward(It->Get(), Coins, Reputation);
	}
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
