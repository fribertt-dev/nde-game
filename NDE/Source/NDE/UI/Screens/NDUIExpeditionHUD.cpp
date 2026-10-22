#include "UI/Screens/NDUIExpeditionHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/NDUIFunctionLibrary.h"
#include "UI/NDUIManagerSubsystem.h"
#include "UI/NDUITags.h"

#define LOCTEXT_NAMESPACE "NDUIExpeditionHUD"

namespace NDUIExpeditionHUDStyle
{
	const FLinearColor TextColor(0.95f, 0.93f, 0.85f);
	const FLinearColor Muted(0.7f, 0.72f, 0.75f);
	const FLinearColor Good(0.45f, 0.95f, 0.5f);
	const FLinearColor Warning(1.f, 0.75f, 0.25f);
	const FLinearColor Danger(1.f, 0.35f, 0.3f);
	constexpr float ClockRefreshSeconds = 0.25f;

	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(1.f, 1.f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		return Block;
	}

	/** Columna anclada en una esquina (o arriba en el centro) del canvas. */
	UVerticalBox* AddColumn(UWidgetTree& Tree, UCanvasPanel& Canvas, FName Name, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UVerticalBox* Box = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Box);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
		return Box;
	}

	void AddRow(UVerticalBox& Box, UWidget* Widget, EHorizontalAlignment Align)
	{
		UVerticalBoxSlot* Slot = Box.AddChildToVerticalBox(Widget);
		Slot->SetHorizontalAlignment(Align);
		Slot->SetPadding(FMargin(0.f, 2.f));
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
	UVerticalBox* ClockBox = AddColumn(Tree, *Canvas, TEXT("ClockBox"), FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 24.f));
	PhaseText = MakeText(Tree, TEXT("PhaseText"), 16, Muted);
	ClockText = MakeText(Tree, TEXT("ClockText"), 34, TextColor);
	NightBar = Tree.ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("NightBar"));
	NightBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.45f, 0.9f));
	AddRow(*ClockBox, PhaseText, HAlign_Center);
	AddRow(*ClockBox, ClockText, HAlign_Center);
	AddRow(*ClockBox, NightBar, HAlign_Fill);

	// Contrato y botín: arriba a la izquierda.
	UVerticalBox* ContractBox = AddColumn(Tree, *Canvas, TEXT("ContractBox"), FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(32.f, 28.f));
	ContractText = MakeText(Tree, TEXT("ContractText"), 18, TextColor);
	ObjectiveText = MakeText(Tree, TEXT("ObjectiveText"), 14, Muted);
	LootText = MakeText(Tree, TEXT("LootText"), 16, TextColor);
	AddRow(*ContractBox, ContractText, HAlign_Left);
	AddRow(*ContractBox, ObjectiveText, HAlign_Left);
	AddRow(*ContractBox, LootText, HAlign_Left);

	// Manos y mochila: abajo a la derecha.
	UVerticalBox* CarryBox = AddColumn(Tree, *Canvas, TEXT("CarryBox"), FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-32.f, -32.f));
	HandsText = MakeText(Tree, TEXT("HandsText"), 16, TextColor);
	PocketText = MakeText(Tree, TEXT("PocketText"), 14, Muted);
	AddRow(*CarryBox, HandsText, HAlign_Right);
	AddRow(*CarryBox, PocketText, HAlign_Right);

	// Avisos: un poco por encima del centro.
	UVerticalBox* ToastBox = AddColumn(Tree, *Canvas, TEXT("ToastBox"), FAnchors(0.5f, 0.3f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	ToastText = MakeText(Tree, TEXT("ToastText"), 24, Warning);
	ToastText->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(*ToastBox, ToastText, HAlign_Center);
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
	if (PendingToasts.IsEmpty() || !ToastText || !GetWorld())
	{
		bShowingToast = false;
		return;
	}
	const FNDUIToast Toast = PendingToasts[0];
	PendingToasts.RemoveAt(0);
	bShowingToast = true;
	ToastText->SetText(Toast.Message);
	ToastText->SetVisibility(ESlateVisibility::HitTestInvisible);
	GetWorld()->GetTimerManager().SetTimer(ToastTimer, this, &ThisClass::HideToast, FMath::Max(0.5f, Toast.DurationSeconds), false);
}

void UNDUIExpeditionHUD::HideToast()
{
	if (ToastText)
	{
		ToastText->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowNextToast();
}

#undef LOCTEXT_NAMESPACE
