#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "NDUIBaseModal.generated.h"

class UCommonTextBlock;
class UNDUIButtonBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDUIOnModalResult, bool, bConfirmed);

// Confirmaciones y avisos que bloquean la pantalla. Atrás equivale a cancelar.
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIBaseModal : public UNDUIBaseScreen
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetupModal(const FText& Title, const FText& Body, const FText& ConfirmText, const FText& CancelText);

	UPROPERTY(BlueprintAssignable, Category = "NDE|UI")
	FNDUIOnModalResult OnModalResult;

protected:
	virtual void NativeOnInitialized() override;
	virtual bool NativeOnHandleBackAction() override;

	// Por defecto cierra y emite OnModalResult; si se sobrescribe en el WBP, llamar al padre.
	UFUNCTION(BlueprintNativeEvent, Category = "NDE|UI")
	void BP_OnConfirm();

	UFUNCTION(BlueprintNativeEvent, Category = "NDE|UI")
	void BP_OnCancel();

	UPROPERTY(BlueprintReadOnly, Category = "Modal", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Title;

	UPROPERTY(BlueprintReadOnly, Category = "Modal", meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Body;

	UPROPERTY(BlueprintReadOnly, Category = "Modal", meta = (BindWidgetOptional))
	TObjectPtr<UNDUIButtonBase> Button_Confirm;

	UPROPERTY(BlueprintReadOnly, Category = "Modal", meta = (BindWidgetOptional))
	TObjectPtr<UNDUIButtonBase> Button_Cancel;

private:
	void HandleConfirmClicked();
	void HandleCancelClicked();
	void Close(bool bConfirmed);
	bool bClosed = false;
};
