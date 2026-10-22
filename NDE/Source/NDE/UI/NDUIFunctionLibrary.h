#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/Data/NDUIViewModels.h"
#include "NDUIFunctionLibrary.generated.h"

class UNDUIDataProvider;
class UNDUIManagerSubsystem;

UCLASS()
class NDE_API UNDUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Tiempo del servidor replicado por el GameState; sin GameState, el tiempo local del mundo.
	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static double GetServerTimeSeconds(const UObject* WorldContextObject);

	// Segundos hasta el amanecer, nunca negativos. Solo para mostrar: el amanecer lo decide el servidor.
	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static float GetNightSecondsRemaining(const UObject* WorldContextObject, const FNDNightClockVM& Clock);

	// 0 al empezar la noche, 1 al amanecer. Para la barra segmentada de WBP_NightClock.
	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static float GetNightProgress(const UObject* WorldContextObject, const FNDNightClockVM& Clock);

	// Segundos de huida tras el amanecer, nunca negativos. 0 si aún no amaneció.
	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static float GetDawnEscapeSecondsRemaining(const UObject* WorldContextObject, const FNDNightClockVM& Clock);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	static bool IsDawn(const FNDNightClockVM& Clock);

	// "MM:SS", redondeando hacia arriba para no mostrar 0:00 con tiempo restante.
	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	static FText FormatClock(float Seconds);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	static FText GetSpeciesDisplayName(ENDDesveladoSpecies Species);

	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static UNDUIManagerSubsystem* GetUIManager(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NDE|UI", meta = (WorldContext = "WorldContextObject"))
	static UNDUIDataProvider* GetUIDataProvider(const UObject* WorldContextObject);
};
