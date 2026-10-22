// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Networking/NDExpeditionGameMode.h"
#include "NDHouseGameMode.generated.h"

class ANDExpeditionDirector;
class ANDExtractionVan;
class ANDHouseLot;
class ANDLootActor;
class UNDContractDefinition;
class UNDLootDefinition;

/**
 * Expedición en una casa generada. Igual que ANDExpeditionGameMode, pero antes de dar
 * pawn a nadie espera a que el lote construya la casa, y hace aparecer a la banda en la
 * Entrada. Si el mapa no tiene ANDHouseLot crea uno en el origen.
 * También monta el bucle de expedición en la casa: director (reloj y reglas), botín en los
 * huecos de la casa según el contrato y camioneta en la marca de extracción del jardín.
 * Opciones de URL: ?Seed=1234 fija la semilla de la casa; ?Contract=C07 elige el contrato.
 * Si el mapa se abre directamente (sin pasar por La Madriguera) deja entrar clientes,
 * para poder probar la casa con varios jugadores fuera del editor.
 */
UCLASS()
class NDE_API ANDHouseGameMode : public ANDExpeditionGameMode
{
	GENERATED_BODY()

public:
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	ANDHouseGameMode();

	/** Contrato si la URL no trae ?Contract= o no se encuentra su Data Asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TSoftObjectPtr<UNDContractDefinition> DefaultContract;

	/** Botín que puede salir en los huecos de la casa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TArray<TSoftObjectPtr<UNDLootDefinition>> LootPool;

protected:
	UFUNCTION()
	void HandleHouseBuilt(ANDHouseLot* Lot);

	/** Usa el director de expedición del mapa o crea uno, con el contrato de la partida. */
	void SetupExpedition();

	/** Botín, camioneta y entrada de la casa recién construida (también tras regenerar). */
	void SetupHouseExpedition(ANDHouseLot* Lot);

	/** ?Contract=C07 busca DA_Contract_C07; si no, DefaultContract. */
	UNDContractDefinition* ResolveContract() const;

	UPROPERTY(Transient)
	TObjectPtr<ANDExpeditionDirector> Director;

	UPROPERTY(Transient)
	TObjectPtr<ANDExtractionVan> Van;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> HouseLoot;

	UPROPERTY(Transient)
	TObjectPtr<ANDHouseLot> HouseLot;

	int32 NextSpawnIndex = 0;
};
