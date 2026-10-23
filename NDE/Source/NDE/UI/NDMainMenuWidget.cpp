// No Despiertes Eso

#include "UI/NDMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Networking/NDNetworkSettings.h"
#include "UI/Loading/NDLoadingScreenSubsystem.h"
#include "UI/NDUISettings.h"
#include "UI/Screens/NDUINativeStyle.h"

void UNDMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// WBP recién creado (sin raíz o con un panel vacío): disposición mínima.
	const UPanelWidget* RootPanel = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (WidgetTree && (!WidgetTree->RootWidget || (RootPanel && RootPanel->GetChildrenCount() == 0)))
	{
		BuildFallbackLayout();
	}
}

void UNDMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostSessionComplete.AddUniqueDynamic(this, &ThisClass::HandleHostComplete);
		Sessions->OnFindSessionsComplete.AddUniqueDynamic(this, &ThisClass::HandleFindComplete);
		Sessions->OnJoinSessionComplete.AddUniqueDynamic(this, &ThisClass::HandleJoinComplete);
	}

	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		const ENDDisconnectReason Reason = GI->ConsumeLastDisconnectReason();
		if (Reason != ENDDisconnectReason::None && Reason != ENDDisconnectReason::LeftVoluntarily)
		{
			FString Message = UNDGameInstance::GetDisconnectReasonText(Reason).ToString();
			if (GI->CanRejoinExpedition())
			{
				Message += FString::Printf(TEXT(" Puedes volver a la expedición durante %.0f s."), GI->GetRejoinSecondsLeft());
			}
			SetStatus(Message);
			OnSessionError(Reason);
		}
	}
}

void UNDMainMenuWidget::NativeDestruct()
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostSessionComplete.RemoveAll(this);
		Sessions->OnFindSessionsComplete.RemoveAll(this);
		Sessions->OnJoinSessionComplete.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UNDMainMenuWidget::HostSession(bool bFriendsOnly)
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(TEXT("Creando sala..."));
		ShowTransition(NSLOCTEXT("NDE", "MenuHostLoading", "Abriendo La Madriguera..."));
		Sessions->HostSession(bFriendsOnly);
	}
}

void UNDMainMenuWidget::FindSessions()
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(TEXT("Buscando salas..."));
		Sessions->FindSessions();
	}
}

void UNDMainMenuWidget::JoinSession(int32 SearchIndex)
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(FString::Printf(TEXT("Entrando en la sala %d..."), SearchIndex));
		ShowTransition(NSLOCTEXT("NDE", "MenuJoinLoading", "Entrando en La Madriguera..."));
		Sessions->JoinSessionByIndex(SearchIndex);
	}
}

void UNDMainMenuWidget::JoinFirstCompatibleSession()
{
	for (const FNDSessionInfo& Info : LastSessions)
	{
		if (Info.bIsCompatible)
		{
			JoinSession(Info.SearchIndex);
			return;
		}
	}
	SetStatus(TEXT("No hay salas compatibles. Pulsa Buscar salas."));
}

void UNDMainMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

bool UNDMainMenuWidget::CanRejoinExpedition() const
{
	const UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	return GI && GI->CanRejoinExpedition();
}

void UNDMainMenuWidget::RejoinExpedition()
{
	UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	if (GI && GI->RejoinExpedition())
	{
		SetStatus(TEXT("Volviendo a la expedición..."));
		ShowTransition(NSLOCTEXT("NDE", "MenuRejoinLoading", "Volviendo a la casa..."));
		return;
	}
	SetStatus(UNDGameInstance::GetDisconnectReasonText(ENDDisconnectReason::RejoinExpired).ToString());
}

void UNDMainMenuWidget::HandleHostComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		HideTransition();
		SetStatus(TEXT("No se pudo crear la sala."));
		OnSessionError(ENDDisconnectReason::JoinFailed);
	}
}

void UNDMainMenuWidget::HandleFindComplete(const TArray<FNDSessionInfo>& Results, bool bWasSuccessful)
{
	LastSessions = Results;

	FString Message = FString::Printf(TEXT("Salas encontradas: %d"), Results.Num());
	for (const FNDSessionInfo& Info : Results)
	{
		Message += FString::Printf(TEXT("\n[%d] %s  %d/%d  %d ms%s"), Info.SearchIndex, *Info.HostName,
			Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs, Info.bIsCompatible ? TEXT("") : TEXT("  (incompatible)"));
	}
	SetStatus(Message);
	OnSessionsFound(Results, bWasSuccessful);
}

void UNDMainMenuWidget::HandleJoinComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		HideTransition();
		SetStatus(TEXT("No se pudo entrar en la sala."));
		OnSessionError(ENDDisconnectReason::JoinFailed);
	}
}

void UNDMainMenuWidget::BuildFallbackLayout()
{
	using namespace NDUINativeStyle;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Fondo: la imagen del menú o una de transición; sin imágenes, la noche lisa.
	UTexture2D* Background = GetDefault<UNDUISettings>()->MainMenuBackground.LoadSynchronous();
	if (!Background)
	{
		const UGameInstance* GI = GetGameInstance();
		const UNDLoadingScreenSubsystem* Loading = GI ? GI->GetSubsystem<UNDLoadingScreenSubsystem>() : nullptr;
		Background = Loading ? Loading->GetRandomImage() : nullptr;
	}
	UBorder* NightBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Night"));
	NightBorder->SetBrushColor(Night);
	UOverlaySlot* NightSlot = Root->AddChildToOverlay(NightBorder);
	NightSlot->SetHorizontalAlignment(HAlign_Fill);
	NightSlot->SetVerticalAlignment(VAlign_Fill);
	if (Background)
	{
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("BackgroundScale"));
		Scale->SetStretch(EStretch::ScaleToFill);
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
		Image->SetBrushFromTexture(Background, true);
		Scale->AddChild(Image);
		UOverlaySlot* ImageSlot = Root->AddChildToOverlay(Scale);
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Columna oscura a la izquierda con el título y los botones.
	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	PanelBorder->SetBrushColor(Panel);
	PanelBorder->SetPadding(FMargin(72.f, 0.f, 56.f, 0.f));
	PanelBorder->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelBorder);
	PanelSlot->SetHorizontalAlignment(HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* PanelWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelWidth"));
	PanelWidth->SetWidthOverride(460.f);
	PanelBorder->SetContent(PanelWidth);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Botones"));
	PanelWidth->SetContent(Box);

	UTextBlock* Title = MakeText(*WidgetTree, TEXT("Titulo"), NSLOCTEXT("NDE", "MenuTitle", "No Despiertes Eso"), 46, Text, TEXT("Bold"));
	Title->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	UTextBlock* Subtitle = MakeText(*WidgetTree, TEXT("Subtitulo"), NSLOCTEXT("NDE", "MenuSubtitle", "Una noche con Los Desvelados"), 18, Amber, TEXT("Italic"));
	Box->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 36.f));

	if (CanRejoinExpedition())
	{
		AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuRejoin", "Volver a la expedición"))->OnClicked.AddDynamic(this, &ThisClass::HandleRejoinClicked);
	}
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuHost", "Crear sala"))->OnClicked.AddDynamic(this, &ThisClass::HandleHostClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuFind", "Buscar salas"))->OnClicked.AddDynamic(this, &ThisClass::HandleFindClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuJoin", "Unirse a la primera sala"))->OnClicked.AddDynamic(this, &ThisClass::HandleJoinClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuQuickPlay", "Prueba rápida: ir a la casa"))->OnClicked.AddDynamic(this, &ThisClass::HandleQuickPlayClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuQuit", "Salir"))->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);

	StatusText = MakeText(*WidgetTree, TEXT("StatusText"), FText::GetEmpty(), 15, Muted, TEXT("Regular"));
	StatusText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));

	// Versión abajo a la derecha.
	const FText Version = FText::Format(NSLOCTEXT("NDE", "MenuVersion", "Demo · versión {0}"), FText::FromString(UNDNetworkSettings::Get()->CompatibilityVersion));
	UTextBlock* VersionText = MakeText(*WidgetTree, TEXT("VersionText"), Version, 13, Muted, TEXT("Regular"));
	UOverlaySlot* VersionSlot = Root->AddChildToOverlay(VersionText);
	VersionSlot->SetHorizontalAlignment(HAlign_Right);
	VersionSlot->SetVerticalAlignment(VAlign_Bottom);
	VersionSlot->SetPadding(FMargin(24.f));

	bFocusPending = true;
}

UButton* UNDMainMenuWidget::AddFallbackButton(UVerticalBox* Box, const FText& Label)
{
	UTextBlock* Text = nullptr;
	UButton* Button = NDUINativeStyle::MakeButton(*WidgetTree, Label, Text);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.f, 5.f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	FallbackButtons.Add(Button);
	FallbackButtonLabels.Add(Text);
	return Button;
}

void UNDMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (FallbackButtons.IsEmpty())
	{
		return;
	}
	// El controlador del menú da el foco al widget al añadirlo; aquí pasa al primer botón.
	if (bFocusPending)
	{
		bFocusPending = false;
		FallbackButtons[0]->SetKeyboardFocus();
	}
	UpdateButtonHighlight();
}

void UNDMainMenuWidget::UpdateButtonHighlight()
{
	HighlightedButton = NDUINativeStyle::UpdateHighlight(FallbackButtons, FallbackButtonLabels, HighlightedButton.Get());
}

void UNDMainMenuWidget::ShowTransition(const FText& Headline)
{
	const UGameInstance* GI = GetGameInstance();
	if (UNDLoadingScreenSubsystem* Loading = GI ? GI->GetSubsystem<UNDLoadingScreenSubsystem>() : nullptr)
	{
		Loading->ShowTransition(Headline);
	}
}

void UNDMainMenuWidget::HideTransition()
{
	const UGameInstance* GI = GetGameInstance();
	if (UNDLoadingScreenSubsystem* Loading = GI ? GI->GetSubsystem<UNDLoadingScreenSubsystem>() : nullptr)
	{
		Loading->HideTransition();
	}
}

void UNDMainMenuWidget::QuickPlayHouse()
{
	const FString HousePackage = UNDNetworkSettings::Get()->ExpeditionTestMap.ToSoftObjectPath().GetLongPackageName();
	if (HousePackage.IsEmpty())
	{
		SetStatus(TEXT("ExpeditionTestMap no está configurado en Project Settings > NDE Network."));
		return;
	}
	ShowTransition(NSLOCTEXT("NDE", "MenuQuickPlayLoading", "Rumbo a la casa..."));
	SetStatus(TEXT("Abriendo la casa..."));
	// listen: los amigos pueden entrar después; la casa admite clientes si se abre sin La Madriguera.
	UGameplayStatics::OpenLevel(this, FName(*HousePackage), true, TEXT("listen?game=House"));
}

void UNDMainMenuWidget::SetStatus(const FString& Message)
{
	UE_LOG(LogNDNet, Log, TEXT("Menu: %s"), *Message);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

UNDSessionSubsystem* UNDMainMenuWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNDSessionSubsystem>() : nullptr;
}
