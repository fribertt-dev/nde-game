#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "NDUIThemeData.generated.h"

class UCommonButtonStyle;
class UCommonTextStyle;
class USoundBase;
class UTexture2D;

USTRUCT(BlueprintType)
struct FNDUIPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Background = FLinearColor(0.02f, 0.03f, 0.07f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Surface = FLinearColor(0.07f, 0.09f, 0.17f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Text = FLinearColor(0.93f, 0.91f, 0.84f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor TextMuted = FLinearColor(0.55f, 0.57f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Accent = FLinearColor(0.98f, 0.78f, 0.32f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Danger = FLinearColor(0.93f, 0.33f, 0.24f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FLinearColor Success = FLinearColor(0.45f, 0.82f, 0.52f);
};

// DA_UITheme_Master (GDD 32.3): un solo sitio para tipografía, paleta día/noche, estilos, iconos y sonidos.
UCLASS(BlueprintType)
class NDE_API UNDUIThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palette")
	FNDUIPalette NightPalette;

	// Se usa tras el amanecer y en pantallas diurnas.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palette")
	FNDUIPalette DayPalette;

	// Modo de alto contraste de accesibilidad (GDD 20.4).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palette")
	FNDUIPalette HighContrastPalette;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo BodyFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> PrimaryButtonStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Styles")
	TSubclassOf<UCommonTextStyle> TitleTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Styles")
	TSubclassOf<UCommonTextStyle> BodyTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Icons")
	TMap<FName, TSoftObjectPtr<UTexture2D>> Icons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ToastSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> DawnWarningSound;

	// Escala de la UI que el jugador puede elegir en accesibilidad (GDD 20.4: 80-150 %).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (ClampMin = "0.5"))
	float MinUIScale = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (ClampMin = "0.5"))
	float MaxUIScale = 1.5f;

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	FNDUIPalette GetPalette(bool bIsDay, bool bHighContrast) const
	{
		return bHighContrast ? HighContrastPalette : (bIsDay ? DayPalette : NightPalette);
	}
};
