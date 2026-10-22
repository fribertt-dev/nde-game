#include "UI/Widgets/NDUIButtonBase.h"

#include "CommonTextBlock.h"

void UNDUIButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetButtonText(ButtonText);
}

void UNDUIButtonBase::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	if (Text_Label)
	{
		Text_Label->SetText(ButtonText);
	}
}

void UNDUIButtonBase::SetLoading(bool bInLoading)
{
	if (bLoading != bInLoading)
	{
		bLoading = bInLoading;
		SetIsInteractionEnabled(!bLoading);
		BP_OnLoadingChanged(bLoading);
	}
}
