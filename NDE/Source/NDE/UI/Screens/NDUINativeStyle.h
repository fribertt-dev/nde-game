#pragma once

#include "CoreMinimal.h"

class UButton;
class UTextBlock;
class UWidgetTree;

/**
 * Colores y piezas comunes de las pantallas hechas en C++ (menú, Madriguera, pausa, HUD y
 * resultados), para que todas se vean igual hasta tener DA_UITheme_Master y sus WBP.
 */
namespace NDUINativeStyle
{
	extern NDE_API const FLinearColor Night;
	extern NDE_API const FLinearColor Panel;
	extern NDE_API const FLinearColor PanelLight;
	extern NDE_API const FLinearColor Button;
	extern NDE_API const FLinearColor Amber;
	extern NDE_API const FLinearColor Text;
	extern NDE_API const FLinearColor DarkText;
	extern NDE_API const FLinearColor Muted;
	extern NDE_API const FLinearColor Good;
	extern NDE_API const FLinearColor Warning;
	extern NDE_API const FLinearColor Danger;

	/** Texto con sombra. Typeface: "Regular", "Bold" o "Italic". */
	NDE_API UTextBlock* MakeText(UWidgetTree& Tree, FName Name, const FText& Text, int32 Size, const FLinearColor& Color, FName Typeface = TEXT("Regular"));

	/** Botón redondeado con su etiqueta; el color lo da SetButtonHighlighted. */
	NDE_API UButton* MakeButton(UWidgetTree& Tree, const FText& Label, UTextBlock*& OutLabel);

	NDE_API void SetButtonHighlighted(UButton* Button, UTextBlock* Label, bool bHighlighted);

	/**
	 * Resalta el botón bajo el ratón o, si no hay, el que tiene el foco del teclado o del mando.
	 * Devuelve el resaltado. Labels va en el mismo orden que Buttons.
	 */
	NDE_API UButton* UpdateHighlight(const TArray<TObjectPtr<UButton>>& Buttons, const TArray<TObjectPtr<UTextBlock>>& Labels, UButton* Previous);
}
