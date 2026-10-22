#include "UI/Screens/NDUINativeRootLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

bool UNDUINativeRootLayout::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// De abajo arriba: lo que va en una capa tapa a las de debajo.
		auto AddLayer = [this, Root](FName Name)
		{
			UCommonActivatableWidgetStack* Stack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), Name);
			UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Stack);
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
			return Stack;
		};
		Layer_Game = AddLayer(TEXT("Layer_Game"));
		Layer_GameMenu = AddLayer(TEXT("Layer_GameMenu"));
		Layer_Menu = AddLayer(TEXT("Layer_Menu"));
		Layer_Modal = AddLayer(TEXT("Layer_Modal"));
	}
	return true;
}
