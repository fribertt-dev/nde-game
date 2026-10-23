#include "UI/Screens/NDUIExpeditionHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/NDUIFunctionLibrary.h"
#include "UI/NDUIManagerSubsystem.h"
#include "UI/NDUITags.h"
#include "UI/Screens/NDUINativeStyle.h"

#define LOCTEXT_NAMESPACE "NDUIExpeditionHUD"

namespace NDUIExpeditionHUDStyle
{
	const FLinearColor TextColor = NDUINativeStyle::Text;
	const FLinearColor Muted = NDUINativeStyle::Muted;
	const FLinearColor Good = NDUINativeStyle::Good;
	const FLinearColor Warning = NDUINativeStyle::Warning;
	const FLinearColor Danger = NDUINativeStyle::Danger;
	constexpr float ClockRefreshSeconds = 0.25f;
	constexpr float InteractionRefreshSeconds = 0.1f;

	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 Size, const FLinearColor& Color, FName Typeface = TEXT("Regular"))
	{
		return NDUINativeStyle::MakeText(Tree, Name, FText::GetEmpty(), Size, Color, Typeface);
	}

	/** Panel redondeado semitransparente anclado en el canvas, con una columna dentro. */
	UVerticalBox* AddPanel(UWidgetTree& Tree, UCanvasPanel& Canvas, FName Name, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, UBorder** OutPanel = nullptr)
	{
		UBorder* Panel = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), *(Name.ToString() + TEXT("Panel")));
		Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 8.f));
		Panel->SetBrushColor(NDUINativeStyle::PanelLight);
		Panel->SetPadding(FMargin(18.f, 10.f));
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Panel);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
		UVerticalBox* Box = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		Panel->SetContent(Box);
		if (OutPanel)
		{
			*OutPanel = Panel;
		}
		return Box;
	}

	/** Texto suelto anclado en el canvas, sin panel. */
	void AddLooseText(UCanvasPanel& Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
	}

	void AddRow(UVerticalBox& Box, UWidget* Widget, EHorizontalAlignment Align)
	{
		UVerticalBoxSlot* Slot = Box.AddChildToVerticalBox(Widget);
		Slot->SetHorizontalAlignment(Align);
		Slot->SetPadding(FMargin(0.f, 2.f));
	}

	/** Busca en el actor un componente cuya clase (o una de sus bases) se llame así. */
	UActorComponent* FindComponentByClassName(const AActor* Actor, FName ClassName)
	{
		if (!Actor)
		{
			return nullptr;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			for (const UClass* Class = Component ? Component->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
			{
				if (Class->GetFName() == ClassName)
				{
					return Component;
				}
			}
		}
		return nullptr;
	}

	/**
	 * Llama por reflexión a una UFUNCTION sin parámetros y lee su valor de vuelta.
	 * Así el HUD lee los componentes del personaje sin depender de sus cabeceras.
	 */
	template <typename PropertyType, typename ValueType>
	bool CallGetter(UObject* Object, FName FunctionName, ValueType& OutValue)
	{
		UFunction* Function = Object ? Object->FindFunction(FunctionName) : nullptr;
		const PropertyType* ReturnProperty = Function ? CastField<PropertyType>(Function->GetReturnProperty()) : nullptr;
		if (!ReturnProperty || Function->NumParms != 1)
		{
			return false;
		}
		uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		Object->ProcessEvent(Function, Params);
		OutValue = ReturnProperty->GetPropertyValue_InContainer(Params);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		return true;
	}
}

UNDUIExpeditionHUD::UNDUIExpeditionHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// HUD de juego: no roba el ratón ni se cierra con Atrás.
	InputMode = ENDUIInputMode::Game;
	bIsBackHandler = false;
}

bool UNDUIExpeditionHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return true;
	}
	using namespace NDUIExpeditionHUDStyle;
	UWidgetTree& Tree = *WidgetTree;
	UCanvasPanel* Canvas = Tree.ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
	Tree.RootWidget = Canvas;

	// Reloj: arriba en el centro.
	UVerticalBox* ClockBox = AddPanel(Tree, *Canvas, TEXT("ClockBox"), FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));
	PhaseText = MakeText(Tree, TEXT("PhaseText"), 15, Muted, TEXT("Bold"));
	ClockText = MakeText(Tree, TEXT("ClockText"), 36, TextColor, TEXT("Bold"));
	NightBar = Tree.ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("NightBar"));
	NightBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.45f, 0.9f));
	USizeBox* BarSize = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NightBarSize"));
	BarSize->SetWidthOverride(260.f);
	BarSize->SetHeightOverride(8.f);
	BarSize->SetContent(NightBar);
	AddRow(*ClockBox, PhaseText, HAlign_Center);
	AddRow(*ClockBox, ClockText, HAlign_Center);
	AddRow(*ClockBox, BarSize, HAlign_Center);

	// Contrato y botín: arriba a la izquierda.
	UVerticalBox* ContractBox = AddPanel(Tree, *Canvas, TEXT("ContractBox"), FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(28.f, 24.f));
	ContractText = MakeText(Tree, TEXT("ContractText"), 18, NDUINativeStyle::Amber, TEXT("Bold"));
	ObjectiveText = MakeText(Tree, TEXT("ObjectiveText"), 14, Muted);
	LootText = MakeText(Tree, TEXT("LootText"), 16, TextColor, TEXT("Bold"));
	AddRow(*ContractBox, ContractText, HAlign_Left);
	AddRow(*ContractBox, ObjectiveText, HAlign_Left);
	AddRow(*ContractBox, LootText, HAlign_Left);

	// Manos y mochila: abajo a la derecha.
	UVerticalBox* CarryBox = AddPanel(Tree, *Canvas, TEXT("CarryBox"), FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-28.f, -28.f));
	HandsText = MakeText(Tree, TEXT("HandsText"), 16, TextColor, TEXT("Bold"));
	PocketText = MakeText(Tree, TEXT("PocketText"), 14, Muted);
	AddRow(*CarryBox, HandsText, HAlign_Right);
	AddRow(*CarryBox, PocketText, HAlign_Right);

	// Retícula: un punto en el centro, con el texto de interacción debajo.
	UImage* Crosshair = Tree.ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Crosshair"));
	Crosshair->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
	Crosshair->SetDesiredSizeOverride(FVector2D(6.f, 6.f));
	Crosshair->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.75f));
	AddLooseText(*Canvas, Crosshair, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	PromptText = MakeText(Tree, TEXT("PromptText"), 18, TextColor, TEXT("Bold"));
	PromptText->SetJustification(ETextJustify::Center);
	AddLooseText(*Canvas, PromptText, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 26.f));
	HiddenText = MakeText(Tree, TEXT("HiddenText"), 16, Good, TEXT("Bold"));
	HiddenText->SetText(LOCTEXT("Hidden", "Escondido"));
	HiddenText->SetVisibility(ESlateVisibility::Collapsed);
	AddLooseText(*Canvas, HiddenText, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 1.f), FVector2D(0.f, -22.f));

	// Avisos: por encima del centro, en su panel.
	UBorder* ToastBorder = nullptr;
	UVerticalBox* ToastBox = AddPanel(Tree, *Canvas, TEXT("ToastBox"), FAnchors(0.5f, 0.28f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, &ToastBorder);
	ToastPanel = ToastBorder;
	ToastText = MakeText(Tree, TEXT("ToastText"), 24, Warning, TEXT("Bold"));
	ToastText->SetJustification(ETextJustify::Center);
	AddRow(*ToastBox, ToastText, HAlign_Center);
	ToastPanel->SetVisibility(ESlateVisibility::Collapsed);

	// Teclas: abajo a la izquierda.
	UTextBlock* ControlsText = MakeText(Tree, TEXT("ControlsText"), 13, Muted);
	ControlsText->SetText(LOCTEXT("Controls", "E  coger / votar     G  soltar     Clic  lanzar     Ctrl  agacharse     P  menú"));
	AddLooseText(*Canvas, ControlsText, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(28.f, -28.f));
	return true;
}

FText UNDUIExpeditionHUD::GetPhaseText(ENDUINightPhase Phase)
{
	switch (Phase)
	{
	case ENDUINightPhase::Insertion: return LOCTEXT("PhaseInsertion", "Entrada: cruzad la puerta");
	case ENDUINightPhase::Infiltration: return LOCTEXT("PhaseNight", "Noche cerrada");
	case ENDUINightPhase::NightEscalation: return LOCTEXT("PhaseLateNight", "Madrugada");
	case ENDUINightPhase::DawnTriggered: return LOCTEXT("PhaseDawn", "¡Amanece!");
	case ENDUINightPhase::DawnEscapeGrace: return LOCTEXT("PhaseEscape", "¡Amanece! Huid a la camioneta");
	case ENDUINightPhase::Settlement: return LOCTEXT("PhaseEnd", "Fin de la noche");
	default: return FText::GetEmpty();
	}
}

void UNDUIExpeditionHUD::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UNDUIManagerSubsystem* UIManager = UNDUIFunctionLibrary::GetUIManager(this))
	{
		UIManager->OnToastRequested.AddUniqueDynamic(this, &ThisClass::HandleToastRequested);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ClockTimer, this, &ThisClass::UpdateClock, NDUIExpeditionHUDStyle::ClockRefreshSeconds, true);
		World->GetTimerManager().SetTimer(InteractionTimer, this, &ThisClass::UpdateInteraction, NDUIExpeditionHUDStyle::InteractionRefreshSeconds, true);
	}
}

void UNDUIExpeditionHUD::NativeOnDeactivated()
{
	if (UNDUIManagerSubsystem* UIManager = UNDUIFunctionLibrary::GetUIManager(this))
	{
		UIManager->OnToastRequested.RemoveDynamic(this, &ThisClass::HandleToastRequested);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimer);
		World->GetTimerManager().ClearTimer(ToastTimer);
		World->GetTimerManager().ClearTimer(InteractionTimer);
	}
	Super::NativeOnDeactivated();
}

void UNDUIExpeditionHUD::NativeRefreshFromViewModel()
{
	UNDUIDataProvider* Provider = GetDataProvider();
	if (!Provider || !ContractText)
	{
		return;
	}
	using namespace NDUIExpeditionHUDStyle;
	Clock = Provider->GetNightClock();
	const FNDExpeditionHUDVM HUD = Provider->GetExpeditionHUD();

	ContractText->SetText(HUD.ContractName);
	ObjectiveText->SetText(HUD.bObjectiveMet
		? FText::Format(LOCTEXT("ObjectiveDone", "{0} (cumplido)"), HUD.ObjectiveText)
		: HUD.ObjectiveText);
	ObjectiveText->SetColorAndOpacity(FSlateColor(HUD.bObjectiveMet ? Good : Muted));
	LootText->SetText(Clock.Quota > 0
		? FText::Format(LOCTEXT("LootQuota", "Botín asegurado: {0} / {1}"), FText::AsNumber(Clock.SecuredLootValue, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Clock.Quota, &FNumberFormattingOptions::DefaultNoGrouping()))
		: FText::Format(LOCTEXT("LootNoQuota", "Botín asegurado: {0}"), FText::AsNumber(Clock.SecuredLootValue, &FNumberFormattingOptions::DefaultNoGrouping())));
	LootText->SetColorAndOpacity(FSlateColor(Clock.Quota > 0 && Clock.SecuredLootValue >= Clock.Quota ? Good : TextColor));

	HandsText->SetText(HUD.HandsItem.IsEmpty()
		? LOCTEXT("HandsEmpty", "Manos: libres")
		: FText::Format(HUD.bHandsNeedHelp ? LOCTEXT("HandsHelp", "Manos: {0} (¡necesita ayuda!)") : LOCTEXT("Hands", "Manos: {0}"), HUD.HandsItem));
	HandsText->SetColorAndOpacity(FSlateColor(HUD.bHandsNeedHelp ? Warning : TextColor));
	PocketText->SetText(FText::Format(LOCTEXT("Pocket", "Mochila: {0} / {1}"), HUD.PocketCount, HUD.PocketSlots));

	UpdateClock();

	// Fin de la noche: el resultado se abre una sola vez.
	const FNDResultsVM Results = Provider->GetLastResults();
	if (!bResultsShown && Clock.Phase == ENDUINightPhase::Settlement && !Results.OutcomeTitle.IsEmpty())
	{
		bResultsShown = true;
		if (UNDUIManagerSubsystem* UIManager = UNDUIFunctionLibrary::GetUIManager(this))
		{
			UIManager->PushScreen(NDUITags::Screen_Results);
		}
	}
}

void UNDUIExpeditionHUD::UpdateClock()
{
	if (!ClockText)
	{
		return;
	}
	using namespace NDUIExpeditionHUDStyle;
	const bool bDawn = UNDUIFunctionLibrary::IsDawn(Clock);
	const float Remaining = bDawn
		? UNDUIFunctionLibrary::GetDawnEscapeSecondsRemaining(this, Clock)
		: UNDUIFunctionLibrary::GetNightSecondsRemaining(this, Clock);
	PhaseText->SetText(GetPhaseText(Clock.Phase));
	ClockText->SetText(UNDUIFunctionLibrary::FormatClock(Remaining));
	NightBar->SetPercent(UNDUIFunctionLibrary::GetNightProgress(this, Clock));

	// Color por los umbrales de aviso (120, 60, 30 y 10 s): naranja y luego rojo.
	FLinearColor Color = TextColor;
	if (bDawn)
	{
		Color = Danger;
	}
	else if (Clock.Phase != ENDUINightPhase::Insertion && Clock.DawnWarningThresholds.Num() > 0)
	{
		const float FirstWarning = FMath::Max(Clock.DawnWarningThresholds[0], 0.f);
		const float LastWarning = Clock.DawnWarningThresholds.Num() > 2 ? Clock.DawnWarningThresholds[2] : 30.f;
		Color = Remaining <= LastWarning ? Danger : (Remaining <= FirstWarning ? Warning : TextColor);
	}
	ClockText->SetColorAndOpacity(FSlateColor(Color));
}

void UNDUIExpeditionHUD::HandleToastRequested(const FNDUIToast& Toast)
{
	// Por prioridad; a igual prioridad, por orden de llegada.
	const int32 Index = PendingToasts.IndexOfByPredicate([&Toast](const FNDUIToast& Other) { return Other.Priority < Toast.Priority; });
	PendingToasts.Insert(Toast, Index == INDEX_NONE ? PendingToasts.Num() : Index);
	if (!bShowingToast)
	{
		ShowNextToast();
	}
}

void UNDUIExpeditionHUD::ShowNextToast()
{
	if (PendingToasts.IsEmpty() || !ToastText || !ToastPanel || !GetWorld())
	{
		bShowingToast = false;
		return;
	}
	const FNDUIToast Toast = PendingToasts[0];
	PendingToasts.RemoveAt(0);
	bShowingToast = true;
	ToastText->SetText(Toast.Message);
	ToastPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	GetWorld()->GetTimerManager().SetTimer(ToastTimer, this, &ThisClass::HideToast, FMath::Max(0.5f, Toast.DurationSeconds), false);
}

void UNDUIExpeditionHUD::HideToast()
{
	if (ToastPanel)
	{
		ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowNextToast();
}

void UNDUIExpeditionHUD::UpdateInteraction()
{
	if (!PromptText || !HiddenText)
	{
		return;
	}
	using namespace NDUIExpeditionHUDStyle;
	const APlayerController* PlayerController = GetOwningPlayer();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;

	// Componentes del frente del personaje (Players/NDInteractionComponent y NDHidingComponent).
	FText Prompt;
	if (UActorComponent* Interaction = FindComponentByClassName(Pawn, TEXT("NDInteractionComponent")))
	{
		CallGetter<FTextProperty>(Interaction, TEXT("GetPromptText"), Prompt);
	}
	bool bHidden = false;
	if (UActorComponent* Hiding = FindComponentByClassName(Pawn, TEXT("NDHidingComponent")))
	{
		CallGetter<FBoolProperty>(Hiding, TEXT("IsHidden"), bHidden);
	}

	if (!Prompt.EqualTo(PromptText->GetText()))
	{
		PromptText->SetText(Prompt);
	}
	PromptText->SetVisibility(Prompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	HiddenText->SetVisibility(bHidden ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
