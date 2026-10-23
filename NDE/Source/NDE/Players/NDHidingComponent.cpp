// No Despiertes Eso

#include "Players/NDHidingComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/NDLog.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HouseGen/NDHouseLot.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Tools/NDLoadoutComponent.h"

namespace NDHiding
{
	/** Cuánto se mete el centro del Desvelado dentro del borde del mueble, en cm (la entrada queda 40 cm fuera). */
	constexpr float InsideEdge = 28.f;
	/** Separación lateral entre dos Desvelados en el mismo escondite, en cm. */
	constexpr float SideBySide = 26.f;

	/** Suelo bajo un punto (la Z del punto si no encuentra nada). */
	float FindFloorZ(const UWorld* World, const FVector& Point, const AActor* Ignore)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NDHidingFloor), false, Ignore);
		FHitResult Hit;
		const FVector Start = Point + FVector(0.f, 0.f, 120.f);
		const FVector End = Point - FVector(0.f, 0.f, 250.f);
		return World && World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint.Z : Point.Z;
	}
}

UNDHidingComponent::UNDHidingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNDHidingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNDHidingComponent, HiddenSpotId);
	DOREPLIFETIME(UNDHidingComponent, HiddenSpotKind);
}

ACharacter* UNDHidingComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

TArray<FNDHidingSpotInfo> UNDHidingComponent::GetAllSpots(const UObject* WorldContextObject)
{
	TArray<FNDHidingSpotInfo> Result;
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const ANDHouseLot* Lot = World ? ANDHouseLot::FindLot(World) : nullptr;
	if (!Lot || !Lot->IsHouseBuilt())
	{
		return Result;
	}

	const FTransform LotToWorld = Lot->GetLotToWorld();
	for (const FNDHouseHidingSpot& Spot : Lot->GetLayout().HidingSpots)
	{
		FNDHidingSpotInfo& Info = Result.AddDefaulted_GetRef();
		Info.SpotId = Spot.StableId;
		Info.Kind = Spot.Kind;
		Info.Capacity = FMath::Max(1, Spot.Capacity);
		Info.FurnitureLocation = LotToWorld.TransformPosition(Spot.LotTransform.GetLocation());
		Info.EntryLocation = LotToWorld.TransformPosition(Spot.EntryPoint);
	}

	// Ocupación: quién está escondido en cada uno.
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		const UNDHidingComponent* Hiding = It->FindComponentByClass<UNDHidingComponent>();
		if (!Hiding || !Hiding->IsHidden())
		{
			continue;
		}
		for (FNDHidingSpotInfo& Info : Result)
		{
			if (Info.SpotId == Hiding->GetHiddenSpotId())
			{
				++Info.Occupants;
			}
		}
	}
	return Result;
}

FNDHidingSpotInfo UNDHidingComponent::FindBestSpot() const
{
	FNDHidingSpotInfo Best;
	const ACharacter* Character = GetCharacter();
	if (!Character || IsHidden())
	{
		return Best;
	}

	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	float BestDistance = EnterRange;
	for (const FNDHidingSpotInfo& Info : GetAllSpots(this))
	{
		// Mismo piso: la entrada está a la altura del suelo de la sala.
		if (FMath::Abs(Info.EntryLocation.Z - Feet.Z) > 120.f)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Info.EntryLocation, Feet);
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Info;
		}
	}
	return Best;
}

bool UNDHidingComponent::HandsTooFull() const
{
	const UNDCarryComponent* Carry = GetOwner() ? GetOwner()->FindComponentByClass<UNDCarryComponent>() : nullptr;
	const ANDLootActor* Held = Carry ? Carry->GetHeldLoot() : nullptr;
	const UNDLootDefinition* Definition = Held ? Held->GetDefinition() : nullptr;
	return Definition && Definition->Category != ENDLootCategory::Light && Definition->Category != ENDLootCategory::Pocket;
}

float UNDHidingComponent::GetVisibilityFactor() const
{
	if (IsHidden())
	{
		return HiddenSpotKind == ENDHidingSpotKind::BehindFurniture ? 0.3f : 0.f;
	}
	const ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return 1.f;
	}
	const UNDLoadoutComponent* Loadout = Character->FindComponentByClass<UNDLoadoutComponent>();
	if (Loadout && Loadout->IsFlashlightOn())
	{
		return 1.f;
	}
	const float Speed = Character->GetVelocity().Size2D();
	if (Character->bIsCrouched)
	{
		return Speed > 10.f ? 0.55f : 0.45f;
	}
	if (Speed > Character->GetCharacterMovement()->MaxWalkSpeed * 0.5f && Speed > 380.f)
	{
		// Corriendo.
		return 1.f;
	}
	return Speed > 10.f ? 0.8f : 0.7f;
}

void UNDHidingComponent::RequestToggleHide()
{
	ServerToggleHide();
}

void UNDHidingComponent::ServerToggleHide_Implementation()
{
	if (IsHidden())
	{
		ExitSpot();
		return;
	}
	if (HandsTooFull())
	{
		return;
	}
	const FNDHidingSpotInfo Spot = FindBestSpot();
	if (Spot.IsValid() && !Spot.IsFull())
	{
		EnterSpot(Spot);
	}
}

void UNDHidingComponent::EnterSpot(const FNDHidingSpotInfo& Spot)
{
	ACharacter* Character = GetCharacter();
	if (!Character || !GetOwner()->HasAuthority())
	{
		return;
	}

	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FloorZ = NDHiding::FindFloorZ(GetWorld(), Spot.EntryLocation, Character);
	ExitLocation = FVector(Spot.EntryLocation.X, Spot.EntryLocation.Y, FloorZ + HalfHeight + 2.f);

	// Justo dentro del borde del mueble, mirando hacia fuera: así se ve la sala desde el escondite.
	FVector Out = (Spot.EntryLocation - Spot.FurnitureLocation).GetSafeNormal2D();
	if (Out.IsNearlyZero())
	{
		Out = Character->GetActorForwardVector();
	}
	FVector Location = Spot.EntryLocation - Out * (40.f + NDHiding::InsideEdge);
	if (Spot.Capacity > 1)
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Out);
		Location += Side * (Spot.Occupants == 0 ? -NDHiding::SideBySide : NDHiding::SideBySide);
	}
	Location.Z = FloorZ + HalfHeight + 2.f;
	const FRotator Facing(0.f, Out.Rotation().Yaw, 0.f);

	HiddenSpotKind = Spot.Kind;
	HiddenSpotId = Spot.SpotId;
	ApplyHiddenState();
	Character->SetActorLocationAndRotation(Location, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		PC->ClientSetRotation(Facing);
	}
	UE_LOG(LogNDNet, Log, TEXT("%s se esconde en el escondite %d (%d/%d)."), *GetOwner()->GetName(), Spot.SpotId, Spot.Occupants + 1, Spot.Capacity);
}

void UNDHidingComponent::ExitSpot()
{
	ACharacter* Character = GetCharacter();
	if (!Character || !IsHidden())
	{
		return;
	}
	HiddenSpotId = INDEX_NONE;
	ApplyHiddenState();
	// TeleportTo busca un hueco libre si otro jugador está justo en la entrada.
	if (!Character->TeleportTo(ExitLocation, Character->GetActorRotation()))
	{
		Character->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UNDHidingComponent::ForceUnhide()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ExitSpot();
	}
}

void UNDHidingComponent::OnRep_HiddenSpotId()
{
	ApplyHiddenState();
}

void UNDHidingComponent::ApplyHiddenState()
{
	ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	const bool bHidden = IsHidden();
	if (bHiddenStateApplied == bHidden)
	{
		return;
	}
	bHiddenStateApplied = bHidden;

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (bHidden)
	{
		Character->UnCrouch();
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	// Sin colisión dentro del mueble (su colisión greybox lo rodea).
	Character->SetActorEnableCollision(!bHidden);
	OnHiddenChanged.Broadcast(bHidden);
}

void UNDHidingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && IsHidden())
	{
		HiddenSpotId = INDEX_NONE;
	}
	Super::EndPlay(EndPlayReason);
}
