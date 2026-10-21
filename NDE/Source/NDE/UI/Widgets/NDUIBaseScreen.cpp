#include "UI/Widgets/NDUIBaseScreen.h"

#include "UI/Data/NDUIDataProvider.h"
#include "UI/NDUIFunctionLibrary.h"

UNDUIBaseScreen::UNDUIBaseScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UNDUIBaseScreen::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case ENDUIInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ENDUIInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ENDUIInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ENDUIInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UNDUIBaseScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	BindProvider();
	RequestRefresh();
}

void UNDUIBaseScreen::NativeOnDeactivated()
{
	UnbindProvider();
	Super::NativeOnDeactivated();
}

void UNDUIBaseScreen::SetLoading(bool bInLoading)
{
	if (bLoading != bInLoading)
	{
		bLoading = bInLoading;
		BP_OnLoadingChanged(bLoading);
	}
}

void UNDUIBaseScreen::RequestRefresh()
{
	BP_RefreshFromViewModel();
}

UNDUIDataProvider* UNDUIBaseScreen::GetDataProvider() const
{
	return UNDUIFunctionLibrary::GetUIDataProvider(this);
}

void UNDUIBaseScreen::BindProvider()
{
	UnbindProvider();
	if (UNDUIDataProvider* Provider = GetDataProvider())
	{
		Provider->OnDataChanged.AddDynamic(this, &ThisClass::HandleDataChanged);
		Provider->OnCommandResult.AddDynamic(this, &ThisClass::HandleCommandResult);
		BoundProvider = Provider;
	}
}

void UNDUIBaseScreen::UnbindProvider()
{
	if (UNDUIDataProvider* Provider = BoundProvider.Get())
	{
		Provider->OnDataChanged.RemoveDynamic(this, &ThisClass::HandleDataChanged);
		Provider->OnCommandResult.RemoveDynamic(this, &ThisClass::HandleCommandResult);
	}
	BoundProvider.Reset();
}

void UNDUIBaseScreen::HandleDataChanged(FName Topic)
{
	if (RefreshTopics.Num() == 0 || RefreshTopics.Contains(Topic))
	{
		BP_OnScreenDataChanged(Topic);
		RequestRefresh();
	}
}

void UNDUIBaseScreen::HandleCommandResult(const FNDUICommandResult& Result)
{
	SetLoading(false);
	BP_OnCommandResult(Result);
}
