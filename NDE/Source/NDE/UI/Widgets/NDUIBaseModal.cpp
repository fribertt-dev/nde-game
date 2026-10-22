#include "UI/Widgets/NDUIBaseModal.h"

#include "CommonTextBlock.h"
#include "UI/Widgets/NDUIButtonBase.h"

void UNDUIBaseModal::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked().AddUObject(this, &ThisClass::HandleConfirmClicked);
	}
	if (Button_Cancel)
	{
		Button_Cancel->OnClicked().AddUObject(this, &ThisClass::HandleCancelClicked);
	}
}

void UNDUIBaseModal::SetupModal(const FText& Title, const FText& Body, const FText& ConfirmText, const FText& CancelText)
{
	bClosed = false;
	if (Text_Title)
	{
		Text_Title->SetText(Title);
	}
	if (Text_Body)
	{
		Text_Body->SetText(Body);
	}
	if (Button_Confirm)
	{
		Button_Confirm->SetButtonText(ConfirmText);
	}
	if (Button_Cancel)
	{
		// Sin texto de cancelar es un aviso de un solo botón.
		Button_Cancel->SetButtonText(CancelText);
		Button_Cancel->SetVisibility(CancelText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UNDUIBaseModal::HandleConfirmClicked()
{
	BP_OnConfirm();
}

void UNDUIBaseModal::HandleCancelClicked()
{
	BP_OnCancel();
}

bool UNDUIBaseModal::NativeOnHandleBackAction()
{
	BP_OnCancel();
	return true;
}

void UNDUIBaseModal::BP_OnConfirm_Implementation()
{
	Close(true);
}

void UNDUIBaseModal::BP_OnCancel_Implementation()
{
	Close(false);
}

void UNDUIBaseModal::Close(bool bConfirmed)
{
	if (bClosed)
	{
		return;
	}
	bClosed = true;
	OnModalResult.Broadcast(bConfirmed);
	DeactivateWidget();
}
