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

/** Qué botín va en qué hueco. */
struct FNDHouseLootPlacement
{
	/** Índice en FNDHouseLayout::Markers, o INDEX_NONE si va en el centro de la sala objetivo. */
	int32 MarkerIndex = INDEX_NONE;
	/** Índice en la lista de candidatos. */
	int32 Candidate = INDEX_NONE;
	/** Sala donde queda. */
	int32 Room = INDEX_NONE;
	/** LootGuid estable: sale de la semilla, del lote y del hueco (StableSpawnId). */
	FGuid LootGuid;
	/** Relativo al centro del lote (Z = suelo de su piso). */
	FTransform LotTransform;
	/** Lo exige el contrato. */
	bool bRequired = false;
};

/**
 * Reparto del botín de una casa generada. Es puro y determinista: con el mismo manifiesto,
 * contrato y candidatos sale el mismo reparto en cualquier máquina. Solo el servidor lo usa
 * para crear los objetos; el botín se replica como actores.
 */
struct NDE_API FNDHouseLootPlanner
{
	/**
	 * Los objetos que pide el contrato van primero, en la sala objetivo (la ruta del bulto ya
	 * está validada hasta ella). Después se rellenan huecos al azar hasta superar la cuota con
	 * margen. Los pesados extra solo van en la planta baja.
	 */
	static void Plan(const FNDHouseLayout& Layout, int64 RunSeed, int32 Quota, const TArray<FName>& RequiredLootIds,
		const TArray<FNDHouseLootCandidate>& Candidates, TArray<FNDHouseLootPlacement>& OutPlacements);

	/** LootGuid estable de un hueco. */
	static FGuid MakeLootGuid(int64 RunSeed, FName LotId, int32 SpawnId);

	/** Valor mínimo a repartir: la cuota con margen, o un mínimo si el contrato no tiene cuota. */
	static int32 GetTargetValue(int32 Quota);

	/** Solo servidor: crea los objetos del reparto en el lote. */
	static TArray<ANDLootActor*> SpawnLoot(ANDHouseLot& Lot, const UNDContractDefinition* Contract, const TArray<UNDLootDefinition*>& Pool);
};
