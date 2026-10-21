// GameMode PROVISIONAL del nivel de prueba del bucle. El GameMode real de la expedición
// es ANDExpeditionGameMode (frente de red); este solo fija el personaje y su equipo de prueba.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDPrototypeGameMode.generated.h"

class UNDToolDefinition;

UCLASS()
class NDE_API ANDPrototypeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDPrototypeGameMode();

	/** Equipo de prueba (los assets los crea Scripts/nde_setup_bucle.py). */
	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> ToolA;

	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> ToolB;

	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> PingTool;

	/** Se reparten por turnos para probar los dos consumibles. */
	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TArray<TSoftObjectPtr<UNDToolDefinition>> Consumables;

	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

protected:
	int32 NextConsumableIndex = 0;
};
