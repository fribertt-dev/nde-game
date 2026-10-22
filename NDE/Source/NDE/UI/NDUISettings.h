#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "NDUISettings.generated.h"

class UNDUIDataProvider;
class UNDUIRootLayout;
class UNDUIScreenRegistry;
class UNDUIThemeData;

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
};
