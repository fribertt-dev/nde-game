// No Despiertes Eso

#include "Networking/NDLobbyGameMode.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Networking/NDLobbyGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Networking/NDReadyZone.h"
#include "Networking/NDSessionSubsystem.h"
#include "Players/NDCharacter.h"
#include "Players/NDPlayerController.h"
#include "Players/NDPlayerState.h"
#include "TimerManager.h"

ANDLobbyGameMode::ANDLobbyGameMode()
{
	GameStateClass = ANDLobbyGameState::StaticClass();
	PlayerStateClass = ANDPlayerState::StaticClass();
	PlayerControllerClass = ANDPlayerController::StaticClass();
	DefaultPawnClass = ANDCharacter::StaticClass();
	bUseSeamlessTravel = true;
}

void ANDLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	DefaultPawnClass = ANDCharacter::GetDesveladoClass();
}

void ANDLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	// Los prefijos NDE_* los interpreta UNDGameInstance::HandleNetworkFailure en el cliente.
	const UNDNetworkSettings* NetSettings = UNDNetworkSettings::Get();
	const FString ClientVersion = UGameplayStatics::ParseOption(Options, TEXT("NDEVersion"));
	// En el editor los clientes de PIE conectan sin opciones de URL.
	const bool bEditorClientWithoutVersion = GIsEditor && ClientVersion.IsEmpty();
	if (!bEditorClientWithoutVersion && ClientVersion != NetSettings->CompatibilityVersion)
	{
		ErrorMessage = FString::Printf(TEXT("NDE_VERSION: la sala usa %s y tu juego %s. Actualiza el juego."), *NetSettings->CompatibilityVersion, *ClientVersion);
		return;
	}

	if (GetNumPlayers() >= NetSettings->MaxPlayers)
	{
		ErrorMessage = TEXT("NDE_FULL: la sala esta llena.");
		return;
	}

	const ANDLobbyGameState* LobbyState = GetGameState<ANDLobbyGameState>();
	if (LobbyState && LobbyState->GetPhase() == ENDLobbyPhase::Traveling)
	{
		ErrorMessage = TEXT("NDE_TRAVELING: la banda ya esta saliendo de expedicion.");
	}
}

void ANDLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Al volver de una expedición, la sala vuelve a aceptar gente.
	if (UNDSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->SetSessionJoinable(true);
	}
	SpawnReadyZoneIfMissing();
}

void ANDLobbyGameMode::SpawnReadyZoneIfMissing()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ANDReadyZone>(World))
	{
		return;
	}
	// Mapas sin zona colocada a mano: una delante del primer PlayerStart.
	TActorIterator<APlayerStart> Start(World);
	if (!Start)
	{
		return;
	}
	const FVector Location = Start->GetActorLocation() + Start->GetActorForwardVector() * 600.f;
	World->SpawnActor<ANDReadyZone>(Location, Start->GetActorRotation());
	UE_LOG(LogNDNet, Log, TEXT("Zona de salida creada en %s"), *Location.ToString());
}

void ANDLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	EvaluateReadyState();
}

void ANDLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// El PlayerState de quien sale sigue en PlayerArray hasta que se destruye.
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::EvaluateReadyState);
}

void ANDLobbyGameMode::HandleReadyRequest(ANDPlayerController* Requester, bool bReady)
{
	ANDLobbyGameState* LobbyState = GetGameState<ANDLobbyGameState>();
	ANDPlayerState* PS = Requester ? Requester->GetPlayerState<ANDPlayerState>() : nullptr;
	if (!LobbyState || !PS || LobbyState->GetPhase() == ENDLobbyPhase::Traveling)
	{
		return;
	}
	if (bReady && !IsInsideReadyZone(Requester))
	{
		UE_LOG(LogNDNet, Log, TEXT("Listo ignorado: %s esta fuera de la zona de salida"), *PS->GetPlayerName());
		return;
	}
	PS->SetReady(bReady);
	EvaluateReadyState();
}

bool ANDLobbyGameMode::IsInsideReadyZone(const ANDPlayerController* Requester) const
{
	bool bAnyZone = false;
	for (TActorIterator<ANDReadyZone> It(GetWorld()); It; ++It)
	{
		bAnyZone = true;
		if (It->ContainsController(Requester))
		{
			return true;
		}
	}
	return !bAnyZone;
}

void ANDLobbyGameMode::EvaluateReadyState()
{
	ANDLobbyGameState* LobbyState = GetGameState<ANDLobbyGameState>();
	if (!LobbyState || LobbyState->GetPhase() == ENDLobbyPhase::Traveling)
	{
		return;
	}

	const int32 NumPlayers = LobbyState->PlayerArray.Num();
	const bool bAllReady = NumPlayers >= UNDNetworkSettings::Get()->MinPlayersToStart
		&& LobbyState->GetNumReadyPlayers() == NumPlayers;

	if (bAllReady && LobbyState->GetPhase() != ENDLobbyPhase::ReadyCheck)
	{
		StartTravelCountdown();
	}
	else if (!bAllReady && LobbyState->GetPhase() == ENDLobbyPhase::ReadyCheck)
	{
		CancelTravelCountdown();
	}
}

void ANDLobbyGameMode::StartTravelCountdown()
{
	ANDLobbyGameState* LobbyState = GetGameState<ANDLobbyGameState>();
	const float Countdown = UNDNetworkSettings::Get()->ReadyCountdownSeconds;
	LobbyState->SetPhase(ENDLobbyPhase::ReadyCheck, LobbyState->GetServerWorldTimeSeconds() + Countdown);

	if (Countdown <= 0.f)
	{
		TravelToExpedition();
		return;
	}
	GetWorldTimerManager().SetTimer(TravelTimer, this, &ThisClass::TravelToExpedition, Countdown, false);
}

void ANDLobbyGameMode::CancelTravelCountdown()
{
	GetWorldTimerManager().ClearTimer(TravelTimer);
	GetGameState<ANDLobbyGameState>()->SetPhase(ENDLobbyPhase::Waiting);
}

void ANDLobbyGameMode::TravelToExpedition()
{
	ANDLobbyGameState* LobbyState = GetGameState<ANDLobbyGameState>();
	const FString MapPackage = UNDNetworkSettings::Get()->ExpeditionTestMap.ToSoftObjectPath().GetLongPackageName();
	if (!LobbyState || MapPackage.IsEmpty())
	{
		UE_LOG(LogNDNet, Error, TEXT("ExpeditionTestMap no configurado en Project Settings > NDE Red."));
		return;
	}

	LobbyState->SetPhase(ENDLobbyPhase::Traveling);

	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		GI->SetExpectedExpeditionPlayers(LobbyState->PlayerArray.Num());
	}
	// TODO(fase 4): entrada a mitad de expedición como espectador (GDD 21.6).
	if (UNDSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNDSessionSubsystem>())
	{
		Sessions->SetSessionJoinable(false);
	}

	UE_LOG(LogNDNet, Log, TEXT("Saliendo de expedicion con %d jugadores a %s"), LobbyState->PlayerArray.Num(), *MapPackage);
	GetWorld()->ServerTravel(MapPackage + TEXT("?game=Expedition"));
}
