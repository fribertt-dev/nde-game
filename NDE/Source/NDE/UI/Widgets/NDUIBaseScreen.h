#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "UI/NDUITypes.h"
#include "UI/Data/NDUIViewModels.h"
#include "NDUIBaseScreen.generated.h"

class UNDUIDataProvider;

/**
 * Base de toda pantalla (UNDUIBaseScreen, GDD 32.3 y 32.8). Cada WBP_ de pantalla hereda de esta clase
 * o de una de sus bases (modal, lista), nunca de otra pantalla concreta.
 * Al activarse se suscribe al proveedor de datos y pide un refresco; refresca por evento, no por Tick.
 */
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIBaseScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UNDUIBaseScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetLoading(bool bInLoading);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	bool IsLoading() const { return bLoading; }

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void RequestRefresh();

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UNDUIDataProvider* GetDataProvider() const;

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	FGameplayTag GetScreenTag() const { return ScreenTag; }

	void SetScreenTag(FGameplayTag InScreenTag) { ScreenTag = InScreenTag; }

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// Igual que BP_RefreshFromViewModel, para pantallas hechas en C++. Se llama antes.
	virtual void NativeRefreshFromViewModel() {}

	// Leer del proveedor y pintar. Se llama al activarse, con RequestRefresh y cuando cambia un tema escuchado.
	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_RefreshFromViewModel();

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_OnScreenDataChanged(FName Topic);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_OnLoadingChanged(bool bIsLoading);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_OnCommandResult(const FNDUICommandResult& Result);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	ENDUIInputMode InputMode = ENDUIInputMode::Menu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	// Temas del proveedor (Lobby, Shop, NightClock...) que provocan refresco. Vacío = todos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	TArray<FName> RefreshTopics;

private:
	UFUNCTION()
	void HandleDataChanged(FName Topic);

	UFUNCTION()
	void HandleCommandResult(const FNDUICommandResult& Result);

	void BindProvider();
	void UnbindProvider();

	TWeakObjectPtr<UNDUIDataProvider> BoundProvider;
	FGameplayTag ScreenTag;
	bool bLoading = false;
};
