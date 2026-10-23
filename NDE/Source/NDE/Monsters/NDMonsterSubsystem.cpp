// No Despiertes Eso

#include "Monsters/NDMonsterSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "HouseGen/NDHouseLot.h"
#include "Monsters/NDMonsterCage.h"
#include "Monsters/NDMonsterCharacter.h"
#include "Monsters/NDMonsterDefinition.h"
#include "Monsters/NDMonsterSettings.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDMonster, Log, All);

namespace NDMonsterSubsystem
{
	constexpr float FindLotInterval = 0.5f;
	constexpr int32 MaxFindLotTries = 40;
	/** Distancia máxima en altura para considerar un sitio del mismo piso, en cm. */
	constexpr float SameFloorZ = 300.f;
}

UNDMonsterSubsystem* UNDMonsterSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNDMonsterSubsystem>() : nullptr;
}

bool UNDMonsterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNDMonsterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// El lote puede crearlo el GameMode después de este momento: se busca unos segundos.
	FindLotTries = 0;
	InWorld.GetTimerManager().SetTimer(FindLotTimer, FTimerDelegate::CreateUObject(this, &ThisClass::TryBindToLot), NDMonsterSubsystem::FindLotInterval, true, 0.f);
}

void UNDMonsterSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindLotTimer);
	}
	if (ANDHouseLot* Lot = BoundLot.Get())
	{
		Lot->OnHouseBuilt.RemoveDynamic(this, &ThisClass::HandleHouseBuilt);
	}
	Super::Deinitialize();
}

void UNDMonsterSubsystem::TryBindToLot()
{
	UWorld* World = GetWorld();
	ANDHouseLot* Lot = World ? ANDHouseLot::FindLot(World) : nullptr;
	if (!Lot)
	{
		if (++FindLotTries >= NDMonsterSubsystem::MaxFindLotTries && World)
		{
			World->GetTimerManager().ClearTimer(FindLotTimer);
		}
		return;
	}
	World->GetTimerManager().ClearTimer(FindLotTimer);
	BoundLot = Lot;
	Lot->OnHouseBuilt.AddUniqueDynamic(this, &ThisClass::HandleHouseBuilt);
	if (Lot->IsHouseBuilt())
	{
		HandleHouseBuilt(Lot);
	}
}

void UNDMonsterSubsystem::HandleHouseBuilt(ANDHouseLot* Lot)
{
	if (!Lot || !Lot->HasAuthority())
	{
		return;
	}
	if (!UNDMonsterSettings::Get()->bSpawnInHouses)
	{
		UE_LOG(LogNDMonster, Log, TEXT("Criatura desactivada en NDE Monsters (bSpawnInHouses)."));
		return;
	}
	SpawnForHouse(Lot);
}

ANDMonsterCharacter* UNDMonsterSubsystem::SpawnForHouse(ANDHouseLot* Lot)
{
	UWorld* World = GetWorld();
	if (!World || !Lot)
	{
		return nullptr;
	}
	// Regenerar la casa borra la criatura y la jaula anteriores (y suelta a los presos).
	for (const TWeakObjectPtr<AActor>& Old : SpawnedForHouse)
	{
		if (ANDMonsterCage* OldCage = Cast<ANDMonsterCage>(Old.Get()))
		{
			OldCage->ReleaseAll(nullptr);
		}
		if (AActor* Actor = Old.Get())
		{
			Actor->Destroy();
		}
	}
	SpawnedForHouse.Reset();
	LastStrike.Reset();

	const TArray<FTransform> Anchors = Lot->GetMarkerTransforms(ENDRoomMarkerType::MonsterAnchor);
	if (Anchors.IsEmpty())
	{
		UE_LOG(LogNDMonster, Warning, TEXT("La casa no tiene MonsterAnchor: sin criatura."));
		return nullptr;
	}
	FTransform Bed = Anchors[0];
	Bed.SetScale3D(FVector::OneVector);

	const UNDMonsterSettings* Settings = UNDMonsterSettings::Get();
	UClass* MonsterClass = Settings->MonsterClass.IsNull() ? nullptr : Settings->MonsterClass.LoadSynchronous();
	if (!MonsterClass)
	{
		MonsterClass = ANDMonsterCharacter::StaticClass();
	}
	const float HalfHeight = MonsterClass->GetDefaultObject<ANDMonsterCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.bDeferConstruction = true;
	const FTransform SpawnTransform(Bed.GetRotation(), Bed.GetLocation() + FVector(0.f, 0.f, HalfHeight + 5.f));
	ANDMonsterCharacter* Monster = World->SpawnActor<ANDMonsterCharacter>(MonsterClass, SpawnTransform, Params);
	if (!Monster)
	{
		UE_LOG(LogNDMonster, Warning, TEXT("No se pudo crear la criatura."));
		return nullptr;
	}
	if (!Settings->DefaultDefinition.IsNull())
	{
		Monster->Definition = Settings->DefaultDefinition.LoadSynchronous();
	}
	Monster->SetBedTransform(Bed);
	Monster->FinishSpawning(SpawnTransform);
	SpawnedForHouse.Add(Monster);
	UE_LOG(LogNDMonster, Log, TEXT("Criatura %s en su cama (%s)."), *Monster->GetDefinition()->MonsterId.ToString(), *Bed.GetLocation().ToString());

	if (Settings->bSpawnCage)
	{
		const FVector CageLocation = FindCageLocation(Lot);
		if (ANDMonsterCage* Cage = SpawnCage(CageLocation, Bed.Rotator().Yaw))
		{
			SpawnedForHouse.Add(Cage);
		}
	}
	return Monster;
}

bool UNDMonsterSubsystem::IsSpotFree(const FVector& FloorLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// Caja de la jaula con margen, despegada 10 cm del suelo para no tocarlo.
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(110.f, 110.f, 75.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDMonsterCageSpot), false);
	return !World->OverlapAnyTestByChannel(FloorLocation + FVector(0.f, 0.f, 90.f), FQuat::Identity, ECC_Pawn, Box, Params);
}

FVector UNDMonsterSubsystem::FindCageLocation(ANDHouseLot* Lot) const
{
	const FNDHouseLayout& Layout = Lot->GetLayout();
	const FTransform LotToWorld = Lot->GetLotToWorld();

	// La jaula va en una sala vecina del dormitorio (ni la entrada ni una escalera): rescatar da miedo
	// pero no es imposible. Si no hay, en el propio dormitorio.
	TArray<int32> Candidates;
	const int32 BedNode = Layout.PatrolNodes.IndexOfByPredicate([&Layout](const FNDHousePatrolNode& N) { return N.Room == Layout.BedroomRoom; });
	const int32 BedFloor = Layout.Rooms.IsValidIndex(Layout.BedroomRoom) ? Layout.Rooms[Layout.BedroomRoom].Floor : 0;
	auto IsGoodRoom = [&Layout](int32 Room)
	{
		return Layout.Rooms.IsValidIndex(Room) && Room != Layout.EntryRoom && Room != Layout.BedroomRoom
			&& Layout.Rooms[Room].Def && Layout.Rooms[Room].Def->StairPart == ENDStairPart::None;
	};
	if (Layout.PatrolNodes.IsValidIndex(BedNode))
	{
		for (int32 Neighbor : Layout.PatrolNodes[BedNode].Neighbors)
		{
			if (Layout.PatrolNodes.IsValidIndex(Neighbor) && IsGoodRoom(Layout.PatrolNodes[Neighbor].Room))
			{
				Candidates.Add(Neighbor);
			}
		}
	}
	if (Candidates.IsEmpty())
	{
		for (int32 Node = 0; Node < Layout.PatrolNodes.Num(); ++Node)
		{
			if (IsGoodRoom(Layout.PatrolNodes[Node].Room))
			{
				Candidates.Add(Node);
			}
		}
		const FVector BedLocation = Layout.PatrolNodes.IsValidIndex(BedNode) ? Layout.PatrolNodes[BedNode].Location : FVector::ZeroVector;
		// Primero las salas del piso de la cama (una sala justo debajo está cerca en línea recta, pero lejos andando).
		Candidates.Sort([&Layout, &BedLocation, BedFloor](int32 A, int32 B)
		{
			const bool bSameA = Layout.Rooms[Layout.PatrolNodes[A].Room].Floor == BedFloor;
			const bool bSameB = Layout.Rooms[Layout.PatrolNodes[B].Room].Floor == BedFloor;
			if (bSameA != bSameB)
			{
				return bSameA;
			}
			return FVector::DistSquared2D(Layout.PatrolNodes[A].Location, BedLocation) < FVector::DistSquared2D(Layout.PatrolNodes[B].Location, BedLocation);
		});
	}
	if (Layout.PatrolNodes.IsValidIndex(BedNode))
	{
		Candidates.Add(BedNode);
	}

	// En cada sala candidata, el primer hueco libre en espiral alrededor de su centro.
	for (int32 Node : Candidates)
	{
		const FVector Center = LotToWorld.TransformPosition(Layout.PatrolNodes[Node].Location);
		for (int32 Ring = 0; Ring <= 3; ++Ring)
		{
			const int32 Steps = Ring == 0 ? 1 : 8;
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const float Angle = 2.f * PI * Step / Steps;
				const FVector Candidate = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Ring * 90.f);
				if (IsSpotFree(Candidate))
				{
					return Candidate;
				}
			}
		}
	}
	// Último recurso: junto a la cama.
	const TArray<FTransform> Anchors = Lot->GetMarkerTransforms(ENDRoomMarkerType::MonsterAnchor);
	return Anchors.IsEmpty() ? Lot->GetActorLocation() : Anchors[0].GetLocation() + Anchors[0].GetRotation().GetForwardVector() * 300.f;
}

ANDMonsterCage* UNDMonsterSubsystem::SpawnCage(const FVector& FloorLocation, float Yaw)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const UNDMonsterSettings* Settings = UNDMonsterSettings::Get();
	UClass* CageClass = Settings->CageClass.IsNull() ? nullptr : Settings->CageClass.LoadSynchronous();
	if (!CageClass)
	{
		CageClass = ANDMonsterCage::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANDMonsterCage* Cage = World->SpawnActor<ANDMonsterCage>(CageClass, FTransform(FRotator(0.f, Yaw, 0.f), FloorLocation), Params);
	UE_LOG(LogNDMonster, Log, TEXT("Jaula en %s."), *FloorLocation.ToString());
	return Cage;
}

ANDMonsterCage* UNDMonsterSubsystem::GetOrSpawnCage(const FVector& Near)
{
	for (const TWeakObjectPtr<ANDMonsterCage>& Cage : Cages)
	{
		if (Cage.IsValid())
		{
			return Cage.Get();
		}
	}
	// Mapa sin casa generada: jaula en un punto navegable cerca.
	FVector Location = Near;
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Random;
		if (NavSys->GetRandomReachablePointInRadius(Near, 500.f, Random))
		{
			Location = Random.Location;
		}
	}
	ANDMonsterCage* Cage = SpawnCage(Location, 0.f);
	if (Cage)
	{
		SpawnedForHouse.Add(Cage);
	}
	return Cage;
}

void UNDMonsterSubsystem::RegisterCage(ANDMonsterCage* Cage)
{
	Cages.RemoveAll([](const TWeakObjectPtr<ANDMonsterCage>& C) { return !C.IsValid(); });
	Cages.AddUnique(Cage);
}

TArray<ANDMonsterCharacter*> UNDMonsterSubsystem::GetMonsters() const
{
	TArray<ANDMonsterCharacter*> Result;
	for (TActorIterator<ANDMonsterCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Result.Add(*It);
		}
	}
	return Result;
}

ANDMonsterCharacter* UNDMonsterSubsystem::FindNearestMonster(const FVector& Location) const
{
	ANDMonsterCharacter* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (ANDMonsterCharacter* Monster : GetMonsters())
	{
		const float Distance = FVector::DistSquared(Monster->GetActorLocation(), Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Monster;
		}
	}
	return Best;
}

void UNDMonsterSubsystem::GetPatrolRoute(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const ANDHouseLot* Lot = GetWorld() ? ANDHouseLot::FindLot(GetWorld()) : nullptr;
	if (!Lot || !Lot->IsHouseBuilt())
	{
		return;
	}
	const FNDHouseLayout& Layout = Lot->GetLayout();
	const FTransform LotToWorld = Lot->GetLotToWorld();
	for (int32 Node : Layout.PatrolRoute)
	{
		if (Layout.PatrolNodes.IsValidIndex(Node))
		{
			OutPoints.Add(LotToWorld.TransformPosition(Layout.PatrolNodes[Node].Location));
		}
	}
}

void UNDMonsterSubsystem::GetSearchPoints(const FVector& Around, int32 Count, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const ANDHouseLot* Lot = GetWorld() ? ANDHouseLot::FindLot(GetWorld()) : nullptr;
	if (Lot && Lot->IsHouseBuilt())
	{
		// Nodos de patrulla del mismo piso, del más cercano al más lejano (saltando el que está encima).
		const FNDHouseLayout& Layout = Lot->GetLayout();
		const FTransform LotToWorld = Lot->GetLotToWorld();
		TArray<FVector> Nodes;
		for (const FNDHousePatrolNode& Node : Layout.PatrolNodes)
		{
			const FVector NodeLocation = LotToWorld.TransformPosition(Node.Location);
			if (FMath::Abs(NodeLocation.Z - Around.Z) < NDMonsterSubsystem::SameFloorZ + 250.f && FVector::Dist2D(NodeLocation, Around) > 250.f)
			{
				Nodes.Add(NodeLocation);
			}
		}
		Nodes.Sort([&Around](const FVector& A, const FVector& B) { return FVector::DistSquared(A, Around) < FVector::DistSquared(B, Around); });
		for (int32 Index = 0; Index < Nodes.Num() && OutPoints.Num() < Count; ++Index)
		{
			OutPoints.Add(Nodes[Index]);
		}
	}
	// Sin casa (o pocas salas): puntos navegables al azar cerca.
	if (OutPoints.Num() < Count)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			for (int32 Try = 0; Try < Count * 2 && OutPoints.Num() < Count; ++Try)
			{
				FNavLocation Random;
				if (NavSys->GetRandomReachablePointInRadius(Around, 700.f, Random))
				{
					OutPoints.Add(Random.Location);
				}
			}
		}
	}
}

double UNDMonsterSubsystem::GetLastStrikeTime(const APawn* Pawn) const
{
	const double* Found = LastStrike.Find(const_cast<APawn*>(Pawn));
	return Found ? *Found : -1.0e9;
}

bool UNDMonsterSubsystem::HandleGrab(ANDMonsterCharacter* Monster, APawn* Victim)
{
	if (!Monster || !Victim || !Monster->HasAuthority())
	{
		return false;
	}
	const UNDMonsterDefinition* Def = Monster->GetDefinition();
	const double Now = Monster->GetServerTime();
	const double Previous = GetLastStrikeTime(Victim);
	LastStrike.Add(Victim, Now);

	// Primer encuentro: aturdido y empujado (GDD 7.4). Si ya le dio hace poco: a la jaula.
	if (Now - Previous > Def->StrikeMemorySeconds)
	{
		Monster->AddDazedVictim(Victim, Now + Def->DazedSeconds);
		if (ACharacter* Character = Cast<ACharacter>(Victim))
		{
			FVector Push = Victim->GetActorLocation() - Monster->GetActorLocation();
			Push.Z = 0.f;
			Push = Push.GetSafeNormal() * Def->DazedKnockback + FVector(0.f, 0.f, Def->DazedKnockback * 0.5f);
			Character->LaunchCharacter(Push, true, true);
		}
		UE_LOG(LogNDMonster, Log, TEXT("Manotazo a %s: aturdido %.1f s."), *Victim->GetName(), Def->DazedSeconds);
		return false;
	}

	ANDMonsterCage* Cage = GetOrSpawnCage(Monster->GetBedTransform().GetLocation());
	if (!Cage)
	{
		return false;
	}
	if (UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(Monster))
	{
		if (APlayerState* PlayerState = Victim->GetPlayerState())
		{
			// Marca capturado y suelta todo el botín; si ya no queda nadie libre, la expedición termina.
			Expedition->SetParticipantCaptured(PlayerState, true);
		}
	}
	Cage->Imprison(Victim);
	LastStrike.Remove(Victim);
	UE_LOG(LogNDMonster, Log, TEXT("%s atrapado y encerrado en la jaula."), *Victim->GetName());
	return true;
}
