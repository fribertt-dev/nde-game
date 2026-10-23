#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "NDUISettings.generated.h"

class UNDUIDataProvider;
class UNDUIRootLayout;
class UNDUIScreenRegistry;
class UNDUIThemeData;
class UTexture2D;

// Project Settings > Game > NDE UI. Se guarda en Config/DefaultGame.ini.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NDE UI"))
class NDE_API UNDUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNDUISettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UNDUIRootLayout> RootLayoutClass;

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftObjectPtr<UNDUIScreenRegistry> ScreenRegistry;

	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftObjectPtr<UNDUIThemeData> Theme;

	// Por defecto, datos reales de la expedición donde la hay y simulados para el resto (lobby, tienda...).
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UNDUIDataProvider> DataProviderClass;

	// Imágenes de la pantalla de transición entre mapas. Se elige una al azar en cada viaje. Vacío = fondo liso.
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TArray<TSoftObjectPtr<UTexture2D>> LoadingScreenImages;

	// Segundos mínimos que se ve la pantalla de transición, aunque la carga sea más rápida.
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenMinSeconds = 1.5f;

	// Tras cargar el mapa, segundos máximos esperando a que el jugador tenga personaje (la casa se genera antes).
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenMaxHoldSeconds = 12.f;

	// Duración del fundido al quitar la pantalla de transición.
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenFadeSeconds = 0.5f;

	// Fondo del menú principal. Vacío = una de las imágenes de transición.
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TSoftObjectPtr<UTexture2D> MainMenuBackground;
};
