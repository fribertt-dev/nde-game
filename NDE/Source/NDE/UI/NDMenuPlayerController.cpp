// No Despiertes Eso

#include "UI/NDMenuPlayerController.h"

#include "Core/NDLog.h"
#include "Networking/NDNetworkSettings.h"
#include "UI/NDMainMenuWidget.h"

void ANDMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	const TSubclassOf<UNDMainMenuWidget> MenuClass = UNDNetworkSettings::Get()->MainMenuWidgetClass.LoadSynchronous();
	if (!MenuClass)
	{
		UE_LOG(LogNDNet, Warning, TEXT("MainMenuWidgetClass no configurado en Project Settings > NDE Network."));
		return;
	}

	MenuWidget = CreateWidget<UNDMainMenuWidget>(this, MenuClass);
	MenuWidget->SetIsFocusable(true);
	MenuWidget->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}
