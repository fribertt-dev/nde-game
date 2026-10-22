// No Despiertes Eso

#include "HouseGen/NDHouseLoot.h"

#include "Contracts/NDContractDefinition.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "HouseGen/NDHouseLot.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"

namespace NDHouseLoot
{
	/** Altura sobre el suelo a la que aparece el objeto; cae solo con sus físicas. */
	constexpr float SpawnHeight = 40.f;
	/** Pesados extra (sin contar los del contrato) como máximo. */
	constexpr int32 MaxExtraHeavy = 2;
	/** Huecos que se llenan como mínimo aunque ya se haya llegado al valor. */
	constexpr int32 MinItems = 6;
	/** Spawn ids de lo que va en el centro de la sala objetivo (no tiene hueco). */
	constexpr int32 CenterSpawnIdBase = 0x10000;
}

int32 FNDHouseLootPlanner::GetTargetValue(int32 Quota)
{
	return Quota > 0 ? FMath::CeilToInt(Quota * 1.5f) : 900;
}

FGuid FNDHouseLootPlanner::MakeLootGuid(int64 RunSeed, FName LotId, int32 SpawnId)
{
	// 'NDLT' marca los GUID de botín generado; el resto sale de semilla, lote y hueco.
	return FGuid(GetTypeHash(RunSeed), GetTypeHash(LotId.ToString()), static_cast<uint32>(SpawnId), 0x4E444C54u);
}

void FNDHouseLootPlanner::Plan(const FNDHouseLayout& Layout, int64 RunSeed, int32 Quota, const TArray<FName>& RequiredLootIds,
	const TArray<FNDHouseLootCandidate>& Candidates, TArray<FNDHouseLootPlacement>& OutPlacements)
{
	OutPlacements.Reset();
	if (Candidates.IsEmpty())
	{
		return;
	}

	FRandomStream Rng(static_cast<int32>(HashCombineFast(GetTypeHash(RunSeed), GetTypeHash(FString(TEXT("Loot"))))));

	// Huecos en el orden del layout (igual en todas las máquinas) y luego barajados.
	TArray<int32> FreeSockets;
	for (int32 Index = 0; Index < Layout.Markers.Num(); ++Index)
	{
		if (Layout.Markers[Index].Type == ENDRoomMarkerType::LootSocket)
		{
			FreeSockets.Add(Index);
		}
	}
	for (int32 Index = FreeSockets.Num() - 1; Index > 0; --Index)
	{
		FreeSockets.Swap(Index, Rng.RandRange(0, Index));
	}

	auto RoomFloor = [&Layout](int32 Room)
	{
		return Layout.Rooms.IsValidIndex(Room) ? Layout.Rooms[Room].Floor : 0;
	};

	auto AddAtSocket = [&](int32 SocketSlot, int32 Candidate, bool bRequired)
	{
		const FNDHouseLayoutMarker& Marker = Layout.Markers[FreeSockets[SocketSlot]];
		FNDHouseLootPlacement& Placement = OutPlacements.AddDefaulted_GetRef();
		Placement.MarkerIndex = FreeSockets[SocketSlot];
		Placement.Candidate = Candidate;
		Placement.Room = Marker.Room;
		Placement.LootGuid = MakeLootGuid(RunSeed, Layout.Lot.LotId, Placement.MarkerIndex);
		Placement.LotTransform = Marker.LotTransform;
		Placement.bRequired = bRequired;
		FreeSockets.RemoveAt(SocketSlot);
	};

	// 1) Lo que pide el contrato, en la sala objetivo.
	int32 CenterCount = 0;
	for (const FName& RequiredId : RequiredLootIds)
	{
		const int32 Candidate = Candidates.IndexOfByPredicate([&RequiredId](const FNDHouseLootCandidate& C) { return C.LootId == RequiredId; });
		if (Candidate == INDEX_NONE)
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("El contrato pide %s, pero no esta en el botin disponible"), *RequiredId.ToString());
			continue;
		}
		const int32 SocketSlot = FreeSockets.IndexOfByPredicate([&Layout](int32 MarkerIndex) { return Layout.Markers[MarkerIndex].Room == Layout.ObjectiveRoom; });
		if (SocketSlot != INDEX_NONE)
		{
			AddAtSocket(SocketSlot, Candidate, true);
			continue;
		}
		// Sala objetivo sin huecos libres: en su centro.
		const int32 Room = Layout.ObjectiveRoom != INDEX_NONE ? Layout.ObjectiveRoom : Layout.EntryRoom;
		FNDHouseLootPlacement& Placement = OutPlacements.AddDefaulted_GetRef();
		Placement.Candidate = Candidate;
		Placement.Room = Room;
		Placement.LootGuid = MakeLootGuid(RunSeed, Layout.Lot.LotId, NDHouseLoot::CenterSpawnIdBase + CenterCount++);
		Placement.LotTransform = FTransform(Layout.GetRoomCenter(Room));
		Placement.bRequired = true;
	}

	// 2) Relleno al azar hasta pasar la cuota con margen.
	const int32 TargetValue = GetTargetValue(Quota);
	int32 TotalValue = 0;
	int32 ExtraHeavy = 0;
	while (!FreeSockets.IsEmpty() && (TotalValue < TargetValue || OutPlacements.Num() < NDHouseLoot::MinItems))
	{
		const int32 Room = Layout.Markers[FreeSockets[0]].Room;
		const bool bHeavyAllowed = RoomFloor(Room) == 0 && ExtraHeavy < NDHouseLoot::MaxExtraHeavy;

		TArray<int32> Allowed;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index].Category != ENDLootCategory::Heavy || bHeavyAllowed)
			{
				Allowed.Add(Index);
			}
		}
		if (Allowed.IsEmpty())
		{
			FreeSockets.RemoveAt(0);
			continue;
		}
		const int32 Candidate = Allowed[Rng.RandRange(0, Allowed.Num() - 1)];
		ExtraHeavy += Candidates[Candidate].Category == ENDLootCategory::Heavy ? 1 : 0;
		TotalValue += Candidates[Candidate].Value;
		AddAtSocket(0, Candidate, false);
	}
}

TArray<ANDLootActor*> FNDHouseLootPlanner::SpawnLoot(ANDHouseLot& Lot, const UNDContractDefinition* Contract, const TArray<UNDLootDefinition*>& Pool)
{
	TArray<ANDLootActor*> Spawned;
	UWorld* World = Lot.GetWorld();
	if (!World || !Lot.HasAuthority() || !Lot.IsHouseBuilt())
	{
		return Spawned;
	}

	TArray<FNDHouseLootCandidate> Candidates;
	TArray<UNDLootDefinition*> Definitions;
	for (UNDLootDefinition* Definition : Pool)
	{
		if (Definition)
		{
			Candidates.Add({ Definition->LootId, Definition->Category, Definition->BaseValue });
			Definitions.Add(Definition);
		}
	}

	const int64 RunSeed = Lot.GetManifest().RunSeed;
	const int32 Quota = Contract ? Contract->RequiredQuotaValue : 0;
	TArray<FNDHouseLootPlacement> Placements;
	Plan(Lot.GetLayout(), RunSeed, Quota, Contract ? Contract->RequiredLootIds : TArray<FName>(), Candidates, Placements);

	const FTransform LotToWorld = Lot.GetLotToWorld();
	int32 TotalValue = 0;
	for (const FNDHouseLootPlacement& Placement : Placements)
	{
		FTransform SpawnTransform = Placement.LotTransform * LotToWorld;
		SpawnTransform.AddToTranslation(FVector(0.f, 0.f, NDHouseLoot::SpawnHeight));
		SpawnTransform.SetScale3D(FVector::OneVector);

		ANDLootActor* Loot = World->SpawnActorDeferred<ANDLootActor>(ANDLootActor::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Loot)
		{
			continue;
		}
		Loot->InitializeSpawn(Definitions[Placement.Candidate], Placement.LootGuid);
		Loot->FinishSpawning(SpawnTransform);
		Spawned.Add(Loot);
		TotalValue += Candidates[Placement.Candidate].Value;
	}

	UE_LOG(LogNDHouseGen, Log, TEXT("Botin de la casa: %d objetos, %d puntos (cuota %d, contrato %s)"),
		Spawned.Num(), TotalValue, Quota, Contract ? *Contract->ContractId.ToString() : TEXT("ninguno"));
	return Spawned;
}
