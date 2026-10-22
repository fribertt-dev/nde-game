#include "UI/NDHUD.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/NDUIManagerSubsystem.h"
#include "UI/NDUITags.h"

void ANDHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (UNDUIManagerSubsystem* UIManager = ULocalPlayer::GetSubsystem<UNDUIManagerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		UIManager->NotifyPlayerControllerReady(PlayerController);
		if (InitialScreen.IsValid())
		{
			UIManager->PushScreen(InitialScreen);
		}
	}
}

ANDFrontEndHUD::ANDFrontEndHUD()
{
	InitialScreen = NDUITags::Screen_MainMenu;
}

ANDExpeditionHUD::ANDExpeditionHUD()
{
	InitialScreen = NDUITags::Screen_ExpeditionHUD;
}
