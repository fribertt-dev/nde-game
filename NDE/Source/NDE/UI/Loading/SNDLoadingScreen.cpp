#include "UI/Loading/SNDLoadingScreen.h"

#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NDLoadingScreen"

namespace NDLoadingScreenStyle
{
	const FLinearColor Night(0.012f, 0.016f, 0.045f, 1.f);
	const FLinearColor Band(0.f, 0.f, 0.f, 0.62f);
	const FLinearColor Amber(1.f, 0.72f, 0.28f, 1.f);
	const FLinearColor TextColor(0.96f, 0.94f, 0.88f, 1.f);
	const FLinearColor Muted(0.72f, 0.74f, 0.8f, 1.f);
}

void SNDLoadingScreen::Construct(const FArguments& InArgs)
{
	using namespace NDLoadingScreenStyle;
	BackgroundBrush.TintColor = FSlateColor(Night);
	BandBrush.TintColor = FSlateColor(Band);

	UTexture2D* Texture = InArgs._Image;
	if (Texture)
	{
		ImageBrush.SetResourceObject(Texture);
		ImageBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		ImageBrush.DrawAs = ESlateBrushDrawType::Image;
	}

	TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	if (Texture)
	{
		// Rellena la pantalla recortando lo que sobre, sin deformar la imagen.
		Overlay->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(&ImageBrush)
			]
		];
	}

	Overlay->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	[
		SNew(SBorder)
		.BorderImage(&BandBrush)
		.Padding(FMargin(64.f, 26.f, 64.f, 34.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GameTitle", "NO DESPIERTES ESO"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.ColorAndOpacity(FSlateColor(Amber))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 8.f)
				[
					SAssignNew(HeadlineText, STextBlock)
					.Text(InArgs._Headline)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 34))
					.ColorAndOpacity(FSlateColor(TextColor))
					.ShadowOffset(FVector2D(2.f, 2.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(InArgs._Tip)
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 17))
					.ColorAndOpacity(FSlateColor(Muted))
					.AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(32.f, 0.f, 0.f, 0.f)
			[
				SNew(SCircularThrobber)
				.Radius(22.f)
			]
		]
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&BackgroundBrush)
		.Padding(0.f)
		[
			Overlay
		]
	];
}

void SNDLoadingScreen::SetHeadline(const FText& Headline)
{
	if (HeadlineText.IsValid())
	{
		HeadlineText->SetText(Headline);
	}
}

#undef LOCTEXT_NAMESPACE
