#include "UI/NDUIManagerSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/Data/NDUIScreenRegistry.h"
#include "UI/Data/NDUIThemeData.h"
#include "UI/NDUISettings.h"
#include "UI/NDUITags.h"
#include "UI/Screens/NDUIExpeditionHUD.h"
#include "UI/Screens/NDUIPauseScreen.h"
#include "UI/Screens/NDUIResultsScreen.h"
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

	if (!NativeScreenRegistry)
	{
		// Pantallas hechas en C++ (HUD de expedición, pausa y resultados). Las del DA_UIScreenRegistry tienen prioridad.
		NativeScreenRegistry = NewObject<UNDUIScreenRegistry>(this, TEXT("NativeScreenRegistry"));
		NativeScreenRegistry->Screens.Add(NDUITags::Screen_ExpeditionHUD, FNDUIScreenEntry{ UNDUIExpeditionHUD::StaticClass(), ENDUILayer::Game });
		NativeScreenRegistry->Screens.Add(NDUITags::Screen_Pause, FNDUIScreenEntry{ UNDUIPauseScreen::StaticClass(), ENDUILayer::GameMenu });
		NativeScreenRegistry->Screens.Add(NDUITags::Screen_Results, FNDUIScreenEntry{ UNDUIResultsScreen::StaticClass(), ENDUILayer::GameMenu });
	}
	if (!ScreenRegistry)
	{
		ScreenRegistry = Settings->ScreenRegistry.LoadSynchronous();
		UE_CLOG(!ScreenRegistry, LogNDUI, Log, TEXT("ScreenRegistry is not set in Project Settings > Game > NDE UI: using the native screens"));
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
	if (!RootLayout || !NativeScreenRegistry)
	{
		UE_LOG(LogNDUI, Error, TEXT("PushScreen(%s) called before the UI was initialized"), *ScreenTag.ToString());
		return nullptr;
	}

	const FNDUIScreenEntry* Entry = ScreenRegistry ? ScreenRegistry->FindScreen(ScreenTag) : nullptr;
	if (!Entry)
	{
		Entry = NativeScreenRegistry->FindScreen(ScreenTag);
	}
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

void UNDUIManagerSubsystem::TogglePause()
{
	if (UNDUIBaseScreen* Open = PauseScreen.Get(); Open && Open->IsActivated())
	{
		Open->DeactivateWidget();
		PauseScreen.Reset();
		return;
	}
	PauseScreen = PushScreen(NDUITags::Screen_Pause);
}

void UNDUIManagerSubsystem::ShowToast(const FNDUIToast& Toast)
{
	OnToastRequested.Broadcast(Toast);
}

void UNDUIManagerSubsystem::SetDataProvider(UNDUIDataProvider* NewProvider)
{
	DataProvider = NewProvider;
}
