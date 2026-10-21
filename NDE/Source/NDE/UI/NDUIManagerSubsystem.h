#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "UI/NDUITypes.h"
#include "NDUIManagerSubsystem.generated.h"

class APlayerController;
class UNDUIBaseModal;
class UNDUIBaseScreen;
class UNDUIDataProvider;
class UNDUIRootLayout;
class UNDUIScreenRegistry;
class UNDUIThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDUIOnScreenPushed, FGameplayTag, ScreenTag, UNDUIBaseScreen*, Screen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDUIOnToastRequested, const FNDUIToast&, Toast);

/**
 * Punto único de entrada a la UI de cada jugador local (UNDUIManagerSubsystem, GDD 32.3).
 * Crea la raíz por capas, abre pantallas por etiqueta según DA_UIScreenRegistry y entrega el
 * proveedor de datos. GameMode y Level Blueprint no crean widgets: lo pide el PlayerController local.
 */
UCLASS()
class NDE_API UNDUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// Lo llama ANDPlayerController al empezar. Crea la raíz si falta o si cambió de mapa.
	void NotifyPlayerControllerReady(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI", meta = (Categories = "UI.Screen"))
	UNDUIBaseScreen* PushScreen(FGameplayTag ScreenTag);

	// Abre UI.Screen.Confirm ya configurado. Sin CancelText es un aviso de un botón.
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	UNDUIBaseModal* ShowConfirmation(const FText& Title, const FText& Body, const FText& ConfirmText, const FText& CancelText);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void PopScreen(UNDUIBaseScreen* Screen);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void ClearLayer(ENDUILayer Layer);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void ShowToast(const FNDUIToast& Toast);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UNDUIDataProvider* GetDataProvider() const { return DataProvider; }

	// Para cambiar el mock por el proveedor real sin tocar las pantallas.
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetDataProvider(UNDUIDataProvider* NewProvider);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UNDUIThemeData* GetTheme() const { return Theme; }

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UNDUIRootLayout* GetRootLayout() const { return RootLayout; }

	UPROPERTY(BlueprintAssignable, Category = "NDE|UI")
	FNDUIOnScreenPushed OnScreenPushed;

	UPROPERTY(BlueprintAssignable, Category = "NDE|UI")
	FNDUIOnToastRequested OnToastRequested;

private:
	void LoadSettingsAssets();

	UPROPERTY(Transient)
	TObjectPtr<UNDUIRootLayout> RootLayout;

	UPROPERTY(Transient)
	TObjectPtr<UNDUIDataProvider> DataProvider;

	UPROPERTY(Transient)
	TObjectPtr<UNDUIScreenRegistry> ScreenRegistry;

	UPROPERTY(Transient)
	TObjectPtr<UNDUIThemeData> Theme;
};
