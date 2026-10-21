#include "UI/Widgets/NDUIRootLayout.h"

#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UI/Widgets/NDUIBaseScreen.h"

UNDUIBaseScreen* UNDUIRootLayout::PushToLayer(ENDUILayer Layer, TSubclassOf<UNDUIBaseScreen> ScreenClass)
{
	UCommonActivatableWidgetStack* Stack = GetLayerStack(Layer);
	if (!Stack || !ScreenClass)
	{
		return nullptr;
	}
	return Stack->AddWidget<UNDUIBaseScreen>(ScreenClass);
}

UCommonActivatableWidgetStack* UNDUIRootLayout::GetLayerStack(ENDUILayer Layer) const
{
	switch (Layer)
	{
	case ENDUILayer::Game:		return Layer_Game;
	case ENDUILayer::GameMenu:	return Layer_GameMenu;
	case ENDUILayer::Menu:		return Layer_Menu;
	case ENDUILayer::Modal:		return Layer_Modal;
	default:					return nullptr;
	}
}

void UNDUIRootLayout::ClearLayer(ENDUILayer Layer)
{
	if (UCommonActivatableWidgetStack* Stack = GetLayerStack(Layer))
	{
		Stack->ClearWidgets();
	}
}
