#include "UI/Screens/NDUINativeStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

namespace NDUINativeStyle
{
	const FLinearColor Night(0.012f, 0.016f, 0.045f, 1.f);
	const FLinearColor Panel(0.f, 0.f, 0.02f, 0.7f);
	const FLinearColor PanelLight(0.02f, 0.03f, 0.07f, 0.55f);
	const FLinearColor Button(0.09f, 0.1f, 0.16f, 0.92f);
	const FLinearColor Amber(1.f, 0.7f, 0.25f, 1.f);
	const FLinearColor Text(0.96f, 0.94f, 0.88f, 1.f);
	const FLinearColor DarkText(0.06f, 0.05f, 0.04f, 1.f);
	const FLinearColor Muted(0.72f, 0.74f, 0.8f, 1.f);
	const FLinearColor Good(0.45f, 0.95f, 0.5f, 1.f);
	const FLinearColor Warning(1.f, 0.75f, 0.25f, 1.f);
	const FLinearColor Danger(1.f, 0.35f, 0.3f, 1.f);

	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, const FText& InText, int32 Size, const FLinearColor& Color, FName Typeface)
	{
		UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(InText);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = Typeface;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(1.f, 1.f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.75f));
		return Block;
	}

	UButton* MakeButton(UWidgetTree& Tree, const FText& Label, UTextBlock*& OutLabel)
	{
		UButton* NewButton = Tree.ConstructWidget<UButton>(UButton::StaticClass());
		// Un mismo fondo redondeado para todos los estados: el color lo pone SetButtonHighlighted.
		FButtonStyle Style = NewButton->GetStyle();
		const FSlateRoundedBoxBrush Brush(FLinearColor::White, 6.f);
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.8f, 0.8f, 0.8f, 1.f), 6.f));
		Style.SetNormalPadding(FMargin(20.f, 10.f));
		Style.SetPressedPadding(FMargin(20.f, 11.f, 20.f, 9.f));
		NewButton->SetStyle(Style);

		OutLabel = MakeText(Tree, NAME_None, Label, 21, Text, TEXT("Bold"));
		NewButton->AddChild(OutLabel);
		SetButtonHighlighted(NewButton, OutLabel, false);
		return NewButton;
	}

	void SetButtonHighlighted(UButton* InButton, UTextBlock* Label, bool bHighlighted)
	{
		if (InButton)
		{
			InButton->SetBackgroundColor(bHighlighted ? Amber : Button);
		}
		if (Label)
		{
			Label->SetColorAndOpacity(FSlateColor(bHighlighted ? DarkText : Text));
		}
	}

	UButton* UpdateHighlight(const TArray<TObjectPtr<UButton>>& Buttons, const TArray<TObjectPtr<UTextBlock>>& Labels, UButton* Previous)
	{
		UButton* Highlight = nullptr;
		for (UButton* Candidate : Buttons)
		{
			if (Candidate && Candidate->GetVisibility() == ESlateVisibility::Visible && Candidate->IsHovered())
			{
				Highlight = Candidate;
				break;
			}
		}
		if (!Highlight)
		{
			for (UButton* Candidate : Buttons)
			{
				if (Candidate && Candidate->HasKeyboardFocus())
				{
					Highlight = Candidate;
					break;
				}
			}
		}
		if (Highlight != Previous)
		{
			for (int32 Index = 0; Index < Buttons.Num(); ++Index)
			{
				SetButtonHighlighted(Buttons[Index], Labels.IsValidIndex(Index) ? Labels[Index].Get() : nullptr, Buttons[Index] == Highlight);
			}
		}
		return Highlight;
	}
}
