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
#include "Players/NDPlayerState.h"
#include "UI/NDLobbyStatusWidget.h"

namespace NDPlayerController
{
	constexpr double MinSecondsBetweenReadyRequests = 0.25;
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

void ANDPlayerController::NDDebugReturnToLobby()
{
#if !UE_BUILD_SHIPPING
	ServerDebugReturnToLobby();
#endif
}

void ANDPlayerController::ServerDebugReturnToLobby_Implementation()
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
