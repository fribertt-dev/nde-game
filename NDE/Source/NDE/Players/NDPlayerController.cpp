// No Despiertes Eso

#include "Players/NDPlayerController.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "Networking/NDExpeditionGameMode.h"
#include "Networking/NDExpeditionGameState.h"
#include "Networking/NDLobbyGameMode.h"
#include "Networking/NDLobbyGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Networking/NDVoiceComponent.h"
#include "Progression/NDProfileSubsystem.h"
#include "Players/NDPlayerState.h"
#include "UI/NDLobbyStatusWidget.h"

namespace NDPlayerController
{
	constexpr double MinSecondsBetweenReadyRequests = 0.25;
}

ANDPlayerController::ANDPlayerController()
{
	VoiceComponent = CreateDefaultSubobject<UNDVoiceComponent>(TEXT("Voice"));
}

void ANDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Teclas fijas provisionales hasta que la UI tenga sus acciones de Enhanced Input.
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ThisClass::HandlePushToTalkPressed);
	InputComponent->BindKey(EKeys::V, IE_Released, this, &ThisClass::HandlePushToTalkReleased);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::HandleVoteKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::HandleVoteKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::HandleVoteKey3);
}

void ANDPlayerController::HandlePushToTalkPressed()
{
	VoiceComponent->SetTransmitting(true);
}

void ANDPlayerController::HandlePushToTalkReleased()
{
	VoiceComponent->SetTransmitting(false);
}

void ANDPlayerController::RequestVote(int32 ContractIndex)
{
	// Las teclas de voto solo hacen algo en La Madriguera.
	if (GetWorld()->GetGameState<ANDLobbyGameState>())
	{
		ServerSubmitVote(ContractIndex);
	}
}

void ANDPlayerController::NDVote(int32 OptionNumber)
{
	RequestVote(OptionNumber - 1);
}

void ANDPlayerController::ServerSubmitVote_Implementation(int32 ContractIndex)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastReadyRequestTime >= 0.0 && Now - LastReadyRequestTime < NDPlayerController::MinSecondsBetweenReadyRequests)
	{
		return;
	}
	LastReadyRequestTime = Now;

	if (ANDLobbyGameMode* GM = GetWorld()->GetAuthGameMode<ANDLobbyGameMode>())
	{
		GM->HandleVoteRequest(this, ContractIndex);
	}
}

void ANDPlayerController::ClientReceiveRunReward_Implementation(const FGuid& RunGuid, int32 Coins, int32 Reputation)
{
	if (UNDProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UNDProfileSubsystem>())
	{
		Profile->ApplyRunReward(RunGuid, Coins, Reputation);
	}
}

void ANDPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		// Al venir del menú el ratón y el foco siguen en la interfaz.
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void ANDPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateLobbyStatusWidget();
}

void ANDPlayerController::UpdateLobbyStatusWidget()
{
	const bool bInLobby = GetWorld()->GetGameState<ANDLobbyGameState>() != nullptr;
	if (!bInLobby)
	{
		if (LobbyStatusWidget)
		{
			LobbyStatusWidget->RemoveFromParent();
			LobbyStatusWidget = nullptr;
		}
		return;
	}
	// Tras un viaje sin costuras el widget anterior ya no está en pantalla.
	if (LobbyStatusWidget && LobbyStatusWidget->IsInViewport())
	{
		return;
	}

	const TSubclassOf<UNDLobbyStatusWidget> WidgetClass = UNDNetworkSettings::Get()->LobbyStatusWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		if (!bWarnedMissingLobbyWidget)
		{
			UE_LOG(LogNDNet, Warning, TEXT("LobbyStatusWidgetClass no configurado en Project Settings > NDE Red."));
			bWarnedMissingLobbyWidget = true;
		}
		return;
	}
	LobbyStatusWidget = CreateWidget<UNDLobbyStatusWidget>(this, WidgetClass);
	LobbyStatusWidget->AddToViewport();
}

void ANDPlayerController::PawnLeavingGame()
{
	// En la casa, el personaje de quien se cae se queda quieto unos segundos en vez de
	// desaparecer de golpe (GDD 21.6). Lo que cargaba lo suelta el sistema de botín.
	APawn* LeavingPawn = GetPawn();
	const float Linger = UNDNetworkSettings::Get()->DisconnectedPawnLingerSeconds;
	if (LeavingPawn && Linger > 0.f && GetWorld()->GetGameState<ANDExpeditionGameState>())
	{
		UnPossess();
		LeavingPawn->SetLifeSpan(Linger);
		return;
	}
	Super::PawnLeavingGame();
}

void ANDPlayerController::RequestSetReady(bool bReady)
{
	ServerSetReady(bReady);
}

void ANDPlayerController::NDReady()
{
	const ANDPlayerState* PS = GetPlayerState<ANDPlayerState>();
	RequestSetReady(!(PS && PS->IsReady()));
}

void ANDPlayerController::ServerSetReady_Implementation(bool bReady)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastReadyRequestTime >= 0.0 && Now - LastReadyRequestTime < NDPlayerController::MinSecondsBetweenReadyRequests)
	{
		return;
	}
	LastReadyRequestTime = Now;

	if (ANDLobbyGameMode* GM = GetWorld()->GetAuthGameMode<ANDLobbyGameMode>())
	{
		GM->HandleReadyRequest(this, bReady);
	}
}

void ANDPlayerController::NDDebugReturnToLobby(int32 Coins)
{
#if !UE_BUILD_SHIPPING
	ServerDebugReturnToLobby(Coins);
#endif
}

void ANDPlayerController::ServerDebugReturnToLobby_Implementation(int32 Coins)
{
#if !UE_BUILD_SHIPPING
	// Solo el host puede forzarlo, para que un cliente no corte la partida de todos.
	if (!IsLocalController())
	{
		UE_LOG(LogNDNet, Warning, TEXT("NDDebugReturnToLobby ignorado: solo el host puede usarlo."));
		return;
	}
	if (ANDExpeditionGameMode* GM = GetWorld()->GetAuthGameMode<ANDExpeditionGameMode>())
	{
		GM->GrantRunRewardToAll(Coins > 0 ? Coins : 100, 1);
		GM->EndExpedition();
	}
#endif
}

void ANDPlayerController::ClientHostClosedSession_Implementation()
{
	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		GI->ReturnToMainMenuWithReason(ENDDisconnectReason::HostLeft);
	}
}

void ANDPlayerController::LeaveSession()
{
	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		GI->ReturnToMainMenuWithReason(ENDDisconnectReason::LeftVoluntarily);
	}
}
