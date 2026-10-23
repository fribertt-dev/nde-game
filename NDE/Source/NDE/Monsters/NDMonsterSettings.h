// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NDMonsterSettings.generated.h"

class ANDMonsterCage;
class ANDMonsterCharacter;
class UNDMonsterDefinition;

/**
 * Criatura gigante en las casas generadas. Project Settings > Game > NDE Monsters (DefaultGame.ini).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NDE Monsters"))
class NDE_API UNDMonsterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNDMonsterSettings();

	static const UNDMonsterSettings* Get() { return GetDefault<UNDMonsterSettings>(); }

	UPROPERTY(Config, EditAnywhere, Category = "Spawn", meta = (ToolTip = "Pone la criatura en su cama cuando se construye una casa generada (solo servidor)."))
	bool bSpawnInHouses = true;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn", meta = (ToolTip = "Clase de la criatura. Vacío = la provisional de C++ (ANDMonsterCharacter). Pon aquí tu Blueprint hijo cuando tenga malla."))
	TSoftClassPtr<ANDMonsterCharacter> MonsterClass;

	UPROPERTY(Config, EditAnywhere, Category = "Spawn", meta = (ToolTip = "Data Asset del monstruo de la casa. Vacío = El Dormilón (M01) con los valores por defecto."))
	TSoftObjectPtr<UNDMonsterDefinition> DefaultDefinition;

	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ToolTip = "Pone la jaula donde la criatura encierra a los atrapados, en una sala junto a su dormitorio."))
	bool bSpawnCage = true;

	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ToolTip = "Clase de la jaula. Vacío = la provisional de C++ (ANDMonsterCage)."))
	TSoftClassPtr<ANDMonsterCage> CageClass;

	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "1", ToolTip = "Segundos que un compañero tiene que pasar junto a la jaula para abrirla (GDD 7.4: 8 s)."))
	float RescueSeconds = 8.f;

	UPROPERTY(Config, EditAnywhere, Category = "Capture", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Ruido al abrir la jaula (0-100). Ruido medio: la criatura lo puede oír."))
	float RescueNoise = 30.f;

	UPROPERTY(Config, EditAnywhere, Category = "AI", meta = (ClampMin = "2", ClampMax = "30", ToolTip = "Veces por segundo que piensa la criatura (GDD 10.4: 5-10)."))
	float ThinkRate = 10.f;
};
