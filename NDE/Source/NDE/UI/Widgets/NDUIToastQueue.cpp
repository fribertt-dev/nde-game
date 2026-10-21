#include "UI/Widgets/NDUIToastQueue.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/NDUIFunctionLibrary.h"
#include "UI/NDUIManagerSubsystem.h"

void UNDUIToastQueue::NativeConstruct()
{
	Super::NativeConstruct();
	if (UNDUIManagerSubsystem* Manager = UNDUIFunctionLibrary::GetUIManager(this))
	{
		Manager->OnToastRequested.AddUniqueDynamic(this, &ThisClass::HandleToastRequested);
	}
}

void UNDUIToastQueue::NativeDestruct()
{
	if (UNDUIManagerSubsystem* Manager = UNDUIFunctionLibrary::GetUIManager(this))
	{
		Manager->OnToastRequested.RemoveDynamic(this, &ThisClass::HandleToastRequested);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ToastTimer);
	}
	Pending.Reset();
	bShowing = false;
	Super::NativeDestruct();
}

void UNDUIToastQueue::HandleToastRequested(const FNDUIToast& Toast)
{
	// Inserción estable: misma prioridad respeta el orden de llegada.
	const int32 Index = Pending.IndexOfByPredicate([&Toast](const FNDUIToast& Existing) { return Existing.Priority < Toast.Priority; });
	Pending.Insert(Toast, Index == INDEX_NONE ? Pending.Num() : Index);

	if (!bShowing)
	{
		ShowNext();
	}
}

void UNDUIToastQueue::ShowNext()
{
	UWorld* World = GetWorld();
	if (Pending.Num() == 0 || !World)
	{
		bShowing = false;
		return;
	}

	const FNDUIToast Toast = Pending[0];
	Pending.RemoveAt(0);
	bShowing = true;
	BP_ShowToast(Toast);
	World->GetTimerManager().SetTimer(ToastTimer, FTimerDelegate::CreateUObject(this, &ThisClass::HandleToastExpired), Toast.DurationSeconds, false);
}

void UNDUIToastQueue::HandleToastExpired()
{
	BP_HideToast();
	ShowNext();
}
