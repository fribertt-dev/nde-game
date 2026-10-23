#include "UI/Screens/NDUIResultsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/Screens/NDUINativeStyle.h"

#define LOCTEXT_NAMESPACE "NDUIResultsScreen"

namespace NDUIResultsStyle
{
	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 Size, const FLinearColor& Color, FName Typeface = TEXT("Regular"))
	{
		UTextBlock* Block = NDUINativeStyle::MakeText(Tree, Name, FText::GetEmpty(), Size, Color, Typeface);
		Block->SetJustification(ETextJustify::Center);
		return Block;
	}

	FText GetOutcomeSubtitle(ENDUIRunOutcome Outcome)
	{
		switch (Outcome)
		{
		case ENDUIRunOutcome::EscapedBeforeDawn: return LOCTEXT("EscapedBeforeDawn", "Escapasteis antes del amanecer");
		case ENDUIRunOutcome::EscapedDuringDawnChase: return LOCTEXT("EscapedDuringChase", "Escapasteis durante la persecución");
		case ENDUIRunOutcome::CapturedAtDawn: return LOCTEXT("CapturedAtDawn", "Capturados al amanecer");
		case ENDUIRunOutcome::ContractFailed: return LOCTEXT("ContractFailedOutcome", "La banda volvió con las manos vacías");
		default: return FText::GetEmpty();
		}
	}
}

UNDUIResultsScreen::UNDUIResultsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Solo informa: no quita el control del personaje mientras se vuelve a La Madriguera.
	InputMode = ENDUIInputMode::Game;
	bIsBackHandler = false;
}

bool UNDUIResultsScreen::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return true;
	}
	using namespace NDUIResultsStyle;
	UWidgetTree& Tree = *WidgetTree;
	UOverlay* Root = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	Tree.RootWidget = Root;

	// Panel oscuro en el centro.
	UBorder* Panel = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 12.f));
	Panel->SetBrushColor(NDUINativeStyle::Panel);
	Panel->SetPadding(FMargin(56.f, 32.f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Column = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);
	UTextBlock* Header = MakeText(Tree, TEXT("Header"), 15, NDUINativeStyle::Muted, TEXT("Bold"));
	Header->SetText(LOCTEXT("Header", "FIN DE LA NOCHE"));
	TitleText = MakeText(Tree, TEXT("TitleText"), 46, NDUINativeStyle::Amber, TEXT("Bold"));
	SubtitleText = MakeText(Tree, TEXT("SubtitleText"), 18, NDUINativeStyle::Text, TEXT("Italic"));
	ContractText = MakeText(Tree, TEXT("ContractText"), 18, NDUINativeStyle::Text);
	LootText = MakeText(Tree, TEXT("LootText"), 22, NDUINativeStyle::Text, TEXT("Bold"));
	RewardText = MakeText(Tree, TEXT("RewardText"), 18, FLinearColor(0.7f, 0.85f, 1.f));
	IncidentsText = MakeText(Tree, TEXT("IncidentsText"), 15, NDUINativeStyle::Muted, TEXT("Italic"));
	UTextBlock* Footer = MakeText(Tree, TEXT("Footer"), 14, NDUINativeStyle::Muted);
	Footer->SetText(LOCTEXT("Footer", "En unos segundos volvéis a La Madriguera."));
	struct FRow { UTextBlock* Text; float Top; };
	const FRow Rows[] = {
		{ Header, 0.f }, { TitleText.Get(), 2.f }, { SubtitleText.Get(), 0.f }, { ContractText.Get(), 18.f },
		{ LootText.Get(), 6.f }, { RewardText.Get(), 6.f }, { IncidentsText.Get(), 14.f }, { Footer, 22.f } };
	for (const FRow& Row : Rows)
	{
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row.Text);
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetPadding(FMargin(0.f, Row.Top, 0.f, 0.f));
	}
	return true;
}

void UNDUIResultsScreen::NativeRefreshFromViewModel()
{
	UNDUIDataProvider* Provider = GetDataProvider();
	if (!Provider || !TitleText)
	{
		return;
	}
	const FNDResultsVM Results = Provider->GetLastResults();
	TitleText->SetText(Results.OutcomeTitle);
	TitleText->SetColorAndOpacity(FSlateColor(Results.bContractCompleted ? NDUINativeStyle::Amber : NDUINativeStyle::Danger));
	SubtitleText->SetText(NDUIResultsStyle::GetOutcomeSubtitle(Results.Outcome));
	ContractText->SetText(Results.bContractCompleted
		? LOCTEXT("ContractDone", "Contrato cumplido")
		: FText::Format(LOCTEXT("ContractFailed", "Contrato fallido: {0}"), Results.FailureReason));
	LootText->SetText(Results.Quota > 0
		? FText::Format(LOCTEXT("LootQuota", "Botín en la camioneta: {0} de {1}"), FText::AsNumber(Results.LootPoints, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Results.Quota, &FNumberFormattingOptions::DefaultNoGrouping()))
		: FText::Format(LOCTEXT("Loot", "Botín en la camioneta: {0}"), FText::AsNumber(Results.LootPoints, &FNumberFormattingOptions::DefaultNoGrouping())));
	IncidentsText->SetText(FText::Join(FText::FromString(TEXT("\n")), Results.FunnyIncidents));
	IncidentsText->SetVisibility(Results.FunnyIncidents.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	RewardText->SetText(FText::Format(LOCTEXT("Reward", "Tu parte: {0} monedas y {1} de reputación"), FText::AsNumber(Results.MoneyEarned, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Results.ReputationEarned, &FNumberFormattingOptions::DefaultNoGrouping())));
}

#undef LOCTEXT_NAMESPACE
