#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/NDUITypes.h"
#include "Engine/TimerHandle.h"
#include "NDUIToastQueue.generated.h"

/**
 * WBP_Toast: muestra de una en una las notificaciones de UNDUIManagerSubsystem::ShowToast,
 * por prioridad. El WBP anima la entrada y salida en BP_ShowToast / BP_HideToast.
 */
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIToastQueue : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_ShowToast(const FNDUIToast& Toast);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_HideToast();

private:
	UFUNCTION()
	void HandleToastRequested(const FNDUIToast& Toast);

	void ShowNext();
	void HandleToastExpired();

	TArray<FNDUIToast> Pending;
	FTimerHandle ToastTimer;
	bool bShowing = false;
};
