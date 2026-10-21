#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/NDUITypes.h"
#include "NDUIRootLayout.generated.h"

class UCommonActivatableWidgetStack;
class UNDUIBaseScreen;

/**
 * WBP_RootLayout: una pila de Common UI por capa, apiladas de abajo arriba en un Overlay.
 * Solo existe una por jugador local y la crea UNDUIManagerSubsystem.
 */
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UNDUIBaseScreen* PushToLayer(ENDUILayer Layer, TSubclassOf<UNDUIBaseScreen> ScreenClass);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UCommonActivatableWidgetStack* GetLayerStack(ENDUILayer Layer) const;

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void ClearLayer(ENDUILayer Layer);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Layers", meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Layer_Game;

	UPROPERTY(BlueprintReadOnly, Category = "Layers", meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Layer_GameMenu;

	UPROPERTY(BlueprintReadOnly, Category = "Layers", meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Layer_Menu;

	UPROPERTY(BlueprintReadOnly, Category = "Layers", meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Layer_Modal;
};
