#include "Core/NDPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "UI/NDUIManagerSubsystem.h"
#include "UI/NDUITags.h"

void ANDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UNDUIManagerSubsystem* UIManager = ULocalPlayer::GetSubsystem<UNDUIManagerSubsystem>(GetLocalPlayer()))
	{
		UIManager->NotifyPlayerControllerReady(this);
		if (InitialScreen.IsValid())
		{
			UIManager->PushScreen(InitialScreen);
		}
	}
}

ANDFrontEndPlayerController::ANDFrontEndPlayerController()
{
	InitialScreen = NDUITags::Screen_MainMenu;
	bShowMouseCursor = true;
}
