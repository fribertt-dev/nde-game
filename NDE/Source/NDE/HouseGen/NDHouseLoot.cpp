// No Despiertes Eso

#include "HouseGen/NDHouseLoot.h"

#include "Contracts/NDContractDefinition.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "HouseGen/NDHouseLot.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Loot/NDRescueContainer.h"

namespace NDHouseLoot
{
	/** Altura sobre el suelo a la que aparece el objeto; cae solo con sus físicas. */
	constexpr float SpawnHeight = 40.f;
	/** Pesados extra (sin contar los del contrato) como máximo. */
	constexpr int32 MaxExtraHeavy = 2;
	/** Huecos que se llenan como mínimo aunque ya se haya llegado al valor. */
	constexpr int32 MinItems = 6;
	/** Spawn ids de lo que no va en un hueco de botín. */
	constexpr int32 FreeSpawnIdBase = 0x10000;
	/** Margen mínimo sobre la cuota (GDD: al menos 1,35 veces). */
	constexpr float MinCoverage = 1.35f;
	/** Del ancla de la criatura (la cama) al borde accesible de la cabecera. */
	constexpr float HeadboardEdgeDistance = 90.f;
	/** Valor mínimo del botín de relleno: lo que vale menos es señuelo. */
	constexpr int32 MinFillerValue = 50;
}

FNDHouseLootContractSpec FNDHouseLootContractSpec::FromContract(const UNDContractDefinition* Contract)
{
	FNDHouseLootContractSpec Spec;
	if (Contract)
	{
		Spec.Quota = Contract->RequiredQuotaValue;
		Spec.RequiredLootIds = Contract->RequiredLootIds;
		Spec.DecoyLootIds = Contract->DecoyLootIds;
		Spec.House = Contract->HouseRequirements;
	}
	return Spec;
}

int32 FNDHouseLootPlanner::GetTargetValue(int32 Quota, float Coverage)
{
	return Quota > 0 ? FMath::CeilToInt(Quota * FMath::Max(Coverage, NDHouseLoot::MinCoverage)) : 900;
}

FGuid FNDHouseLootPlanner::MakeLootGuid(int64 RunSeed, FName LotId, int32 SpawnId)
{
	// 'NDLT' marca los GUID de botín generado; el resto sale de semilla, lote y sitio.
	return FGuid(GetTypeHash(RunSeed), GetTypeHash(LotId.ToString()), static_cast<uint32>(SpawnId), 0x4E444C54u);
}

void FNDHouseLootPlanner::GetObjectiveCandidates(const FNDHouseLayout& Layout, const FNDContractHouseRequirements& House,
	TArray<FTransform>& OutTransforms, TArray<int32>& OutMarkers)
{
	OutTransforms.Reset();
	OutMarkers.Reset();
	const int32 Room = Layout.GetAnchorRoom(House);
	if (!Layout.Rooms.IsValidIndex(Room))
	{
		return;
	}
	const int32 Wanted = FMath::Clamp(House.CandidateLocations, 1, 5);

	// Cabecera de la cama: el borde de la cama (ancla de la criatura) hacia el centro del dormitorio.
	if (House.ObjectiveAnchor == ENDContractAnchor::BedHeadboard)
	{
		for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
		{
			if (Marker.Type == ENDRoomMarkerType::MonsterAnchor && Marker.Room == Room)
			{
				FVector ToCenter = Layout.GetRoomCenter(Room) - Marker.LotTransform.GetLocation();
				ToCenter.Z = 0.f;
				const FVector Edge = Marker.LotTransform.GetLocation() + ToCenter.GetSafeNormal() * FMath::Min(NDHouseLoot::HeadboardEdgeDistance, ToCenter.Size());
				OutTransforms.Add(FTransform(Marker.LotTransform.GetRotation(), Edge));
				OutMarkers.Add(INDEX_NONE);
			}
		}
	}
	// Huecos de botín de la sala.
	for (int32 Index = 0; Index < Layout.Markers.Num() && OutTransforms.Num() < Wanted; ++Index)
	{
		const FNDHouseLayoutMarker& Marker = Layout.Markers[Index];
		if (Marker.Type == ENDRoomMarkerType::LootSocket && Marker.Room == Room)
		{
			OutTransforms.Add(Marker.LotTransform);
			OutMarkers.Add(Index);
		}
	}
	// Si faltan, el centro de las celdas de la sala.
	TArray<FIntPoint> Cells;
	Layout.GetRoomCells(Room, Cells);
	const float FloorZ = Layout.Rooms[Room].Floor * NDHouseGen::FloorHeight;
	for (int32 Index = 0; Index < Cells.Num() && OutTransforms.Num() < Wanted; ++Index)
	{
		OutTransforms.Add(FTransform(Layout.Lot.GetCellCenter(Cells[Index]) + FVector(0.f, 0.f, FloorZ)));
		OutMarkers.Add(INDEX_NONE);
	}
	if (OutTransforms.Num() > Wanted)
	{
		OutTransforms.SetNum(Wanted);
		OutMarkers.SetNum(Wanted);
	}
}

void FNDHouseLootPlanner::Plan(const FNDHouseLayout& Layout, int64 RunSeed, const FNDHouseLootContractSpec& Spec,
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

	int32 FreeSpawnCount = 0;
	int32 TotalValue = 0;
	auto Add = [&](int32 Candidate, int32 MarkerIndex, const FTransform& Transform) -> FNDHouseLootPlacement&
	{
		FNDHouseLootPlacement& Placement = OutPlacements.AddDefaulted_GetRef();
		Placement.Candidate = Candidate;
		Placement.MarkerIndex = MarkerIndex;
		Placement.LotTransform = Transform;
		Placement.Room = MarkerIndex != INDEX_NONE ? Layout.Markers[MarkerIndex].Room : Layout.FindRoomAt(Transform.GetLocation());
		Placement.LootGuid = MakeLootGuid(RunSeed, Layout.Lot.LotId, MarkerIndex != INDEX_NONE ? MarkerIndex : NDHouseLoot::FreeSpawnIdBase + FreeSpawnCount++);
		FreeSockets.Remove(MarkerIndex);
		// Los señuelos no cuentan para cubrir la cuota.
		TotalValue += Spec.DecoyLootIds.Contains(Candidates[Candidate].LootId) ? 0 : Candidates[Candidate].Value;
		return Placement;
	};
	auto FindCandidate = [&Candidates](const FName& LootId)
	{
		return Candidates.IndexOfByPredicate([&LootId](const FNDHouseLootCandidate& C) { return C.LootId == LootId; });
	};

	// 1) Objetivo del contrato en una de sus candidatas (la semilla decide en cuál empieza).
	TArray<FTransform> AnchorTransforms;
	TArray<int32> AnchorMarkers;
	GetObjectiveCandidates(Layout, Spec.House, AnchorTransforms, AnchorMarkers);
	const int32 Start = AnchorTransforms.Num() > 0 ? Rng.RandRange(0, AnchorTransforms.Num() - 1) : 0;
	int32 NextAnchor = 0;
	auto TakeAnchor = [&](int32 Candidate) -> FNDHouseLootPlacement*
	{
		if (NextAnchor >= AnchorTransforms.Num())
		{
			return nullptr;
		}
		const int32 Slot = (Start + NextAnchor++) % AnchorTransforms.Num();
		return &Add(Candidate, AnchorMarkers[Slot], AnchorTransforms[Slot]);
	};
	for (const FName& RequiredId : Spec.RequiredLootIds)
	{
		const int32 Candidate = FindCandidate(RequiredId);
		if (Candidate == INDEX_NONE)
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("El contrato pide %s, pero no esta en el botin disponible"), *RequiredId.ToString());
			continue;
		}
		FNDHouseLootPlacement* Placement = TakeAnchor(Candidate);
		if (!Placement)
		{
			// Más objetos que candidatas: el centro de la sala del objetivo.
			const int32 Room = Layout.GetAnchorRoom(Spec.House);
			Placement = &Add(Candidate, INDEX_NONE, FTransform(Layout.GetRoomCenter(Layout.Rooms.IsValidIndex(Room) ? Room : Layout.EntryRoom)));
		}
		Placement->bRequired = true;
		Placement->bInRescueContainer = Spec.House.ObjectiveAnchor == ENDContractAnchor::RescueContainer;
	}

	// 2) Señuelos: en las candidatas que quedan y, si no, en huecos libres.
	for (const FName& DecoyId : Spec.DecoyLootIds)
	{
		const int32 Candidate = FindCandidate(DecoyId);
		if (Candidate == INDEX_NONE)
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("Senuelo %s no esta en el botin disponible"), *DecoyId.ToString());
			continue;
		}
		FNDHouseLootPlacement* Placement = TakeAnchor(Candidate);
		if (!Placement && !FreeSockets.IsEmpty())
		{
			Placement = &Add(Candidate, FreeSockets[0], Layout.Markers[FreeSockets[0]].LotTransform);
		}
		if (Placement)
		{
			Placement->bDecoy = true;
		}
	}

	// 3) Relleno al azar hasta cubrir la cuota con margen (los señuelos no cuentan como botín de valor).
	const int32 TargetValue = GetTargetValue(Spec.Quota, Spec.House.QuotaCoverage);
	int32 ExtraHeavy = 0;
	TArray<int32> Fillable;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		// Ni señuelos (de este u otro contrato: valen casi nada) ni lo que pide el contrato, que es único.
		if (!Spec.DecoyLootIds.Contains(Candidates[Index].LootId) && !Spec.RequiredLootIds.Contains(Candidates[Index].LootId)
			&& Candidates[Index].Value >= NDHouseLoot::MinFillerValue)
		{
			Fillable.Add(Index);
		}
	}
	while (!FreeSockets.IsEmpty() && !Fillable.IsEmpty() && (TotalValue < TargetValue || OutPlacements.Num() < NDHouseLoot::MinItems))
	{
		const int32 MarkerIndex = FreeSockets[0];
		const int32 Room = Layout.Markers[MarkerIndex].Room;
		const bool bHeavyAllowed = Layout.Rooms.IsValidIndex(Room) && Layout.Rooms[Room].Floor == NDHouseGen::GroundFloor && ExtraHeavy < NDHouseLoot::MaxExtraHeavy;
		TArray<int32> Allowed = Fillable.FilterByPredicate([&](int32 Index) { return Candidates[Index].Category != ENDLootCategory::Heavy || bHeavyAllowed; });
		if (Allowed.IsEmpty())
		{
			FreeSockets.RemoveAt(0);
			continue;
		}
		const int32 Candidate = Allowed[Rng.RandRange(0, Allowed.Num() - 1)];
		ExtraHeavy += Candidates[Candidate].Category == ENDLootCategory::Heavy ? 1 : 0;
		Add(Candidate, MarkerIndex, Layout.Markers[MarkerIndex].LotTransform);
	}

	// 4) Casa pequeña sin huecos para cubrir la cuota: objetos extra en el suelo de salas de planta baja
	//    que no sean de paso del bulto ni escaleras (nunca se queda corta).
	TArray<int32> FloorRooms;
	for (int32 RoomIndex = 1; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		if (Layout.Rooms[RoomIndex].Floor == NDHouseGen::GroundFloor && Layout.Rooms[RoomIndex].Def->StairPart == ENDStairPart::None)
		{
			FloorRooms.Add(RoomIndex);
		}
	}
	TArray<int32> Light = Fillable.FilterByPredicate([&Candidates](int32 Index) { return Candidates[Index].Category != ENDLootCategory::Heavy; });
	for (int32 Extra = 0; Extra < 20 && TotalValue < TargetValue && !FloorRooms.IsEmpty() && !Light.IsEmpty(); ++Extra)
	{
		const int32 Room = FloorRooms[Rng.RandRange(0, FloorRooms.Num() - 1)];
		TArray<FIntPoint> Cells;
		Layout.GetRoomCells(Room, Cells);
		const FVector Location = Layout.Lot.GetCellCenter(Cells[Rng.RandRange(0, Cells.Num() - 1)])
			+ FVector(Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(-90.f, 90.f), 0.f);
		Add(Light[Rng.RandRange(0, Light.Num() - 1)], INDEX_NONE, FTransform(Location));
	}
}

TArray<AActor*> FNDHouseLootPlanner::SpawnLoot(ANDHouseLot& Lot, const UNDContractDefinition* Contract, const TArray<UNDLootDefinition*>& Pool)
{
	TArray<AActor*> Spawned;
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

	const FNDHouseLootContractSpec Spec = FNDHouseLootContractSpec::FromContract(Contract);
	TArray<FNDHouseLootPlacement> Placements;
	Plan(Lot.GetLayout(), Lot.GetManifest().RunSeed, Spec, Candidates, Placements);

	const FTransform LotToWorld = Lot.GetLotToWorld();
	int32 TotalValue = 0;
	for (const FNDHouseLootPlacement& Placement : Placements)
	{
		FTransform SpawnTransform = Placement.LotTransform * LotToWorld;
		SpawnTransform.SetScale3D(FVector::OneVector);

		ANDRescueContainer* Container = nullptr;
		if (Placement.bInRescueContainer)
		{
			Container = World->SpawnActor<ANDRescueContainer>(ANDRescueContainer::StaticClass(), SpawnTransform);
			Spawned.Add(Container);
		}
		SpawnTransform.AddToTranslation(FVector(0.f, 0.f, NDHouseLoot::SpawnHeight));

		ANDLootActor* Loot = World->SpawnActorDeferred<ANDLootActor>(ANDLootActor::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Loot)
		{
			continue;
		}
		Loot->InitializeSpawn(Definitions[Placement.Candidate], Placement.LootGuid);
		Loot->FinishSpawning(SpawnTransform);
		if (Container)
		{
			Container->SetContainedLoot(Loot);
		}
		Spawned.Add(Loot);
		TotalValue += Candidates[Placement.Candidate].Value;
	}

	UE_LOG(LogNDHouseGen, Log, TEXT("Botin de la casa: %d objetos, %d puntos (cuota %d, contrato %s)"),
		Placements.Num(), TotalValue, Spec.Quota, Contract ? *Contract->ContractId.ToString() : TEXT("ninguno"));
	return Spawned;
}
