#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class UTexture2D;

/**
 * Pantalla de transición entre mapas: imagen a pantalla completa, franja inferior con el
 * título, el destino ("Rumbo a la casa...") y un consejo, y un indicador de carga.
 * Es Slate puro para que también la pinte el MoviePlayer durante las cargas que bloquean.
 * La textura la mantiene viva UNDLoadingScreenSubsystem.
 */
class SNDLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNDLoadingScreen)
		: _Image(nullptr)
	{}
		SLATE_ARGUMENT(UTexture2D*, Image)
		SLATE_ARGUMENT(FText, Headline)
		SLATE_ARGUMENT(FText, Tip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetHeadline(const FText& Headline);

private:
	FSlateBrush ImageBrush;
	FSlateColorBrush BackgroundBrush = FSlateColorBrush(FLinearColor::White);
	FSlateColorBrush BandBrush = FSlateColorBrush(FLinearColor::White);
	TSharedPtr<STextBlock> HeadlineText;
};
