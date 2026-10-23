#include "UI/NDHUD.h"

#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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

	if (bAllowPauseMenu)
	{
		// Teclas fijas provisionales, como las de ANDPlayerController. Con la pausa abierta las recibe la propia pantalla.
		EnableInput(PlayerController);
		for (const FKey& Key : { EKeys::P, EKeys::Escape, EKeys::Gamepad_Special_Right })
		{
			InputComponent->BindKey(Key, IE_Pressed, this, &ANDHUD::HandlePausePressed);
		}
	}
}

void ANDHUD::HandlePausePressed()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (UNDUIManagerSubsystem* UIManager = PlayerController ? ULocalPlayer::GetSubsystem<UNDUIManagerSubsystem>(PlayerController->GetLocalPlayer()) : nullptr)
	{
		UIManager->TogglePause();
	}
}

ANDFrontEndHUD::ANDFrontEndHUD()
{
	InitialScreen = NDUITags::Screen_MainMenu;
	bAllowPauseMenu = false;
}

ANDExpeditionHUD::ANDExpeditionHUD()
{
	InitialScreen = NDUITags::Screen_ExpeditionHUD;
}
