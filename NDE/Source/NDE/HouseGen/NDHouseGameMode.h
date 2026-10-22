// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Networking/NDExpeditionGameMode.h"
#include "NDHouseGameMode.generated.h"

class ANDHouseLot;

/**
 * Expedición en una casa generada. Igual que ANDExpeditionGameMode, pero antes de dar
 * pawn a nadie espera a que el lote construya la casa, y hace aparecer a la banda en la
 * Entrada. Si el mapa no tiene ANDHouseLot crea uno en el origen.
 * Opción de URL: ?Seed=1234 fija la semilla de la casa.
 */
UCLASS()
class NDE_API ANDHouseGameMode : public ANDExpeditionGameMode
{
	GENERATED_BODY()

public:
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:
	UFUNCTION()
	void HandleHouseBuilt(ANDHouseLot* Lot);

	UPROPERTY(Transient)
	TObjectPtr<ANDHouseLot> HouseLot;

	int32 NextSpawnIndex = 0;
};
