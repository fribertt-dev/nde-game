#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "NDUIButtonBase.generated.h"

class UCommonTextBlock;

// Botón base del juego: texto, estado de espera y estilos de Common UI (UNDUIButtonBase, GDD 32.3).
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetButtonText(const FText& InText);

	// Mientras espera respuesta del proveedor no se puede pulsar dos veces.
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetLoading(bool bInLoading);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	bool IsLoading() const { return bLoading; }

protected:
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_OnLoadingChanged(bool bIsLoading);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FText ButtonText;

	UPROPERTY(BlueprintReadOnly, Category = "Button", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Label;

private:
	bool bLoading = false;
};
