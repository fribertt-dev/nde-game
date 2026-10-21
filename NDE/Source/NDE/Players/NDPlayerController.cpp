// No Despiertes Eso

#include "Players/NDPlayerController.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "Networking/NDExpeditionGameMode.h"
#include "Networking/NDLobbyGameMode.h"
#include "Players/NDPlayerState.h"

namespace NDPlayerController
{
	constexpr double MinSecondsBetweenReadyRequests = 0.25;
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

void ANDPlayerController::LeaveSession()
{
	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		GI->ReturnToMainMenuWithReason(ENDDisconnectReason::LeftVoluntarily);
	}
}
