#include "UI/Screens/NDUIResultsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Data/NDUIDataProvider.h"

#define LOCTEXT_NAMESPACE "NDUIResultsScreen"

namespace NDUIResultsStyle
{
	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetJustification(ETextJustify::Center);
		return Block;
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
	Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.82f));
	Panel->SetPadding(FMargin(48.f, 28.f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Column = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);
	TitleText = MakeText(Tree, TEXT("TitleText"), 40, FLinearColor(1.f, 0.85f, 0.4f));
	ContractText = MakeText(Tree, TEXT("ContractText"), 18, FLinearColor(0.9f, 0.9f, 0.9f));
	LootText = MakeText(Tree, TEXT("LootText"), 20, FLinearColor(0.95f, 0.93f, 0.85f));
	RewardText = MakeText(Tree, TEXT("RewardText"), 18, FLinearColor(0.7f, 0.85f, 1.f));
	for (UTextBlock* Text : { TitleText.Get(), ContractText.Get(), LootText.Get(), RewardText.Get() })
	{
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Text);
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetPadding(FMargin(0.f, 6.f));
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
	ContractText->SetText(Results.bContractCompleted
		? LOCTEXT("ContractDone", "Contrato cumplido")
		: FText::Format(LOCTEXT("ContractFailed", "Contrato fallido: {0}"), Results.FailureReason));
	LootText->SetText(Results.Quota > 0
		? FText::Format(LOCTEXT("LootQuota", "Botín en la camioneta: {0} de {1}"), FText::AsNumber(Results.LootPoints, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Results.Quota, &FNumberFormattingOptions::DefaultNoGrouping()))
		: FText::Format(LOCTEXT("Loot", "Botín en la camioneta: {0}"), FText::AsNumber(Results.LootPoints, &FNumberFormattingOptions::DefaultNoGrouping())));
	RewardText->SetText(FText::Format(LOCTEXT("Reward", "Tu parte: {0} monedas y {1} de reputación"), FText::AsNumber(Results.MoneyEarned, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Results.ReputationEarned, &FNumberFormattingOptions::DefaultNoGrouping())));
}

#undef LOCTEXT_NAMESPACE
