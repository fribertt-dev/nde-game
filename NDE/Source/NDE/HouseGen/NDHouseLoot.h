// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Expedition/NDExpeditionTypes.h"
#include "HouseGen/NDHouseLayout.h"

class ANDHouseLot;
class ANDLootActor;
class UNDContractDefinition;
class UNDLootDefinition;

/** Tipo de botín que puede salir en la casa (copia de lo que importa de UNDLootDefinition). */
struct FNDHouseLootCandidate
{
	FName LootId;
	ENDLootCategory Category = ENDLootCategory::Light;
	int32 Value = 0;
};

/** Lo que el contrato pide al reparto del botín (copia de UNDContractDefinition, sin UObjects). */
struct FNDHouseLootContractSpec
{
	int32 Quota = 0;
	TArray<FName> RequiredLootIds;
	TArray<FName> DecoyLootIds;
	FNDContractHouseRequirements House;

	static FNDHouseLootContractSpec FromContract(const UNDContractDefinition* Contract);
};

/** Qué botín va en qué sitio. */
struct FNDHouseLootPlacement
{
	/** Índice en FNDHouseLayout::Markers, o INDEX_NONE si no va en un hueco de botín. */
	int32 MarkerIndex = INDEX_NONE;
	/** Índice en la lista de candidatos. */
	int32 Candidate = INDEX_NONE;
	/** Sala donde queda. */
	int32 Room = INDEX_NONE;
	/** LootGuid estable: sale de la semilla, del lote y del sitio (StableSpawnId). */
	FGuid LootGuid;
	/** Relativo al centro del lote (Z = suelo de su piso). */
	FTransform LotTransform;
	/** Lo exige el contrato. */
	bool bRequired = false;
	/** Señuelo del contrato (parecido al objetivo o distractor). */
	bool bDecoy = false;
	/** Va dentro de un tarro de rescate que hay que abrir. */
	bool bInRescueContainer = false;
};

/**
 * Reparto del botín de una casa generada. Es puro y determinista: con el mismo manifiesto,
 * contrato y candidatos sale el mismo reparto en cualquier máquina. Solo el servidor lo usa
 * para crear los objetos; el botín se replica como actores.
 */
struct NDE_API FNDHouseLootPlanner
{
	/**
	 * 1) Lo que pide el contrato, en una de sus ubicaciones candidatas (sala objetivo, sala obligatoria,
	 *    cabecera de la cama o tarro de rescate), elegida con la semilla.
	 * 2) Los señuelos, en las demás candidatas y cerca.
	 * 3) Relleno al azar hasta cubrir la cuota con el margen del contrato. Los pesados extra solo en planta baja.
	 */
	static void Plan(const FNDHouseLayout& Layout, int64 RunSeed, const FNDHouseLootContractSpec& Spec,
		const TArray<FNDHouseLootCandidate>& Candidates, TArray<FNDHouseLootPlacement>& OutPlacements);

	/** Ubicaciones candidatas para el objetivo del contrato (en orden estable). */
	static void GetObjectiveCandidates(const FNDHouseLayout& Layout, const FNDContractHouseRequirements& House,
		TArray<FTransform>& OutTransforms, TArray<int32>& OutMarkers);

	/** LootGuid estable de un sitio. */
	static FGuid MakeLootGuid(int64 RunSeed, FName LotId, int32 SpawnId);

	/** Valor mínimo a repartir: la cuota con el margen del contrato, o un mínimo si no tiene cuota. */
	static int32 GetTargetValue(int32 Quota, float Coverage = 1.35f);

	/** Solo servidor: crea los objetos del reparto (y los tarros de rescate) en el lote. */
	static TArray<AActor*> SpawnLoot(ANDHouseLot& Lot, const UNDContractDefinition* Contract, const TArray<UNDLootDefinition*>& Pool);
};
