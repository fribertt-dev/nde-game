#include "UI/NDUIManagerSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/Data/NDUIScreenRegistry.h"
#include "UI/Data/NDUIThemeData.h"
#include "UI/NDUISettings.h"
#include "UI/NDUITags.h"
#include "UI/Widgets/NDUIBaseModal.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "UI/Widgets/NDUIRootLayout.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDUI, Log, All);

void UNDUIManagerSubsystem::Deinitialize()
{
	if (RootLayout)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}
	Super::Deinitialize();
}

void UNDUIManagerSubsystem::LoadSettingsAssets()
{
	const UNDUISettings* Settings = GetDefault<UNDUISettings>();

	if (!ScreenRegistry)
	{
		ScreenRegistry = Settings->ScreenRegistry.LoadSynchronous();
		UE_CLOG(!ScreenRegistry, LogNDUI, Warning, TEXT("ScreenRegistry is not set in Project Settings > Game > NDE UI"));
	}
	if (!Theme)
	{
		Theme = Settings->Theme.LoadSynchronous();
	}
	if (!DataProvider)
	{
		if (UClass* ProviderClass = Settings->DataProviderClass.LoadSynchronous())
		{
			DataProvider = NewObject<UNDUIDataProvider>(this, ProviderClass);
		}
		UE_CLOG(!DataProvider, LogNDUI, Warning, TEXT("DataProviderClass is not set in Project Settings > Game > NDE UI"));
	}
}

void UNDUIManagerSubsystem::NotifyPlayerControllerReady(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	LoadSettingsAssets();

	// Al cambiar de mapa la raíz anterior sale del viewport con el mundo viejo.
	if (RootLayout && RootLayout->IsInViewport() && RootLayout->GetOwningPlayer() == PlayerController)
	{
		return;
	}
	if (RootLayout)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	UClass* LayoutClass = GetDefault<UNDUISettings>()->RootLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogNDUI, Error, TEXT("RootLayoutClass is not set in Project Settings > Game > NDE UI"));
		return;
	}

	RootLayout = CreateWidget<UNDUIRootLayout>(PlayerController, LayoutClass);
	if (RootLayout)
	{
		RootLayout->AddToPlayerScreen(1000);
	}
}

UNDUIBaseScreen* UNDUIManagerSubsystem::PushScreen(FGameplayTag ScreenTag)
{
	if (!RootLayout || !ScreenRegistry)
	{
		UE_LOG(LogNDUI, Error, TEXT("PushScreen(%s) called before the UI was initialized"), *ScreenTag.ToString());
		return nullptr;
	}

	const FNDUIScreenEntry* Entry = ScreenRegistry->FindScreen(ScreenTag);
	if (!Entry)
	{
		UE_LOG(LogNDUI, Error, TEXT("Screen %s is not in the screen registry"), *ScreenTag.ToString());
		return nullptr;
	}

	TSubclassOf<UNDUIBaseScreen> ScreenClass = Entry->WidgetClass.LoadSynchronous();
	UNDUIBaseScreen* Screen = RootLayout->PushToLayer(Entry->Layer, ScreenClass);
	if (!Screen)
	{
		UE_LOG(LogNDUI, Error, TEXT("Could not create screen %s"), *ScreenTag.ToString());
		return nullptr;
	}

	Screen->SetScreenTag(ScreenTag);
	OnScreenPushed.Broadcast(ScreenTag, Screen);
	return Screen;
}

UNDUIBaseModal* UNDUIManagerSubsystem::ShowConfirmation(const FText& Title, const FText& Body, const FText& ConfirmText, const FText& CancelText)
{
	UNDUIBaseModal* Modal = Cast<UNDUIBaseModal>(PushScreen(NDUITags::Screen_Confirm));
	if (Modal)
	{
		Modal->SetupModal(Title, Body, ConfirmText, CancelText);
	}
	return Modal;
}

void UNDUIManagerSubsystem::PopScreen(UNDUIBaseScreen* Screen)
{
	if (Screen)
	{
		Screen->DeactivateWidget();
	}
}

void UNDUIManagerSubsystem::ClearLayer(ENDUILayer Layer)
{
	if (RootLayout)
	{
		RootLayout->ClearLayer(Layer);
	}
}

void UNDUIManagerSubsystem::ShowToast(const FNDUIToast& Toast)
{
	OnToastRequested.Broadcast(Toast);
}

void UNDUIManagerSubsystem::SetDataProvider(UNDUIDataProvider* NewProvider)
{
	DataProvider = NewProvider;
}
