#include "Loot/NDCarryComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"

UNDCarryComponent::UNDCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNDCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNDCarryComponent, HeldLoot);
	DOREPLIFETIME(UNDCarryComponent, PocketLoot);
}

APawn* UNDCarryComponent::GetPawn() const
{
	return Cast<APawn>(GetOwner());
}

TArray<ANDLootActor*> UNDCarryComponent::GetPocketLoot() const
{
	TArray<ANDLootActor*> Result;
	for (ANDLootActor* Loot : PocketLoot)
	{
		if (Loot)
		{
			Result.Add(Loot);
		}
	}
	return Result;
}

TArray<ANDLootActor*> UNDCarryComponent::GetAllCarriedLoot() const
{
	TArray<ANDLootActor*> Result = GetPocketLoot();
	if (HeldLoot)
	{
		Result.Add(HeldLoot);
	}
	return Result;
}

float UNDCarryComponent::GetSpeedMultiplier() const
{
	const UNDLootDefinition* Definition = HeldLoot ? HeldLoot->GetDefinition() : nullptr;
	if (!Definition)
	{
		return 1.f;
	}

	const bool bEnoughHands = HeldLoot->HasEnoughCarriers();
	switch (Definition->Category)
	{
	case ENDLootCategory::Light:
		return 0.9f;
	case ENDLootCategory::Medium:
	case ENDLootCategory::Unique:
	case ENDLootCategory::Objective:
		return 0.75f;
	case ENDLootCategory::Heavy:
		// Sin ayuda se arrastra muy despacio (GDD 6.3: método alternativo más lento).
		return bEnoughHands ? 0.6f : 0.3f;
	case ENDLootCategory::Huge:
		return bEnoughHands ? 0.45f : 0.15f;
	default:
		return 1.f;
	}
}

bool UNDCarryComponent::CanSprint() const
{
	const UNDLootDefinition* Definition = HeldLoot ? HeldLoot->GetDefinition() : nullptr;
	return !Definition || Definition->Category == ENDLootCategory::Light;
}

bool UNDCarryComponent::IsWithinReach(const ANDLootActor* Loot) const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn || !Loot)
	{
		return false;
	}
	// El servidor es algo tolerante con la latencia; el ángulo solo se filtra en el cliente.
	const float Radius = Loot->GetComponentsBoundingBox().GetExtent().Size2D();
	return FVector::Dist(Pawn->GetActorLocation(), Loot->GetActorLocation()) <= InteractionRange + Radius + 60.f;
}

ANDLootActor* UNDCarryComponent::FindBestLootTarget() const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn || !GetWorld())
	{
		return nullptr;
	}

	const FVector Origin = Pawn->GetActorLocation();
	const FRotator ViewRotation = Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Pawn->GetActorRotation();
	const FVector Forward = FRotator(0.f, ViewRotation.Yaw, 0.f).Vector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(InteractionAngleDegrees));

	ANDLootActor* Best = nullptr;
	float BestScore = -1.f;
	for (TActorIterator<ANDLootActor> It(GetWorld()); It; ++It)
	{
		ANDLootActor* Loot = *It;
		if (!Loot->CanBeGrabbedBy(Pawn))
		{
			continue;
		}
		const FVector ToLoot = Loot->GetActorLocation() - Origin;
		const float Radius = Loot->GetComponentsBoundingBox().GetExtent().Size2D();
		const float Distance = ToLoot.Size2D();
		if (Distance > InteractionRange + Radius)
		{
			continue;
		}
		const float Dot = FVector::DotProduct(ToLoot.GetSafeNormal2D(), Forward);
		if (Dot < MinDot && Distance > Radius)
		{
			continue;
		}
		const float Score = Dot - Distance / (InteractionRange * 4.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Loot;
		}
	}
	return Best;
}

void UNDCarryComponent::RequestGrab(ANDLootActor* Target)
{
	if (Target)
	{
		ServerGrab(Target);
	}
}

void UNDCarryComponent::RequestDrop()
{
	ServerDrop();
}

void UNDCarryComponent::RequestThrow(FVector Direction, float Charge)
{
	ServerThrow(Direction.GetSafeNormal(), FMath::Clamp(Charge, 0.f, 1.f));
}

void UNDCarryComponent::ServerGrab_Implementation(ANDLootActor* Target)
{
	APawn* Pawn = GetPawn();
	if (!Pawn || !Target || !IsWithinReach(Target) || !Target->CanBeGrabbedBy(Pawn))
	{
		return;
	}

	const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	if (Expedition && !Expedition->CanPlayerAct(Pawn))
	{
		return;
	}

	const UNDLootDefinition* Definition = Target->GetDefinition();
	if (Definition->IsPocketSized() && Target->GetLootState() == ENDLootState::WorldLoose)
	{
		if (!HasFreePocketSlot())
		{
			return;
		}
		PocketLoot.Add(Target);
		Target->SetPocketed(Pawn);
		return;
	}

	if (HeldLoot)
	{
		return;
	}
	if (Target->AddCarrier(Pawn))
	{
		HeldLoot = Target;
	}
}

void UNDCarryComponent::ServerDrop_Implementation()
{
	if (HeldLoot)
	{
		HeldLoot->RemoveCarrier(GetPawn(), FVector::ZeroVector);
	}
}

void UNDCarryComponent::ServerThrow_Implementation(FVector_NetQuantizeNormal Direction, float Charge)
{
	if (!HeldLoot)
	{
		return;
	}
	const UNDLootDefinition* Definition = HeldLoot->GetDefinition();
	// Solo se lanza lo que se lleva sin ayuda (ligero o mediano).
	if (!Definition || HeldLoot->GetCarrierCount() > 1 ||
		(Definition->Category != ENDLootCategory::Light && Definition->Category != ENDLootCategory::Medium))
	{
		ServerDrop_Implementation();
		return;
	}

	const float Speed = FMath::Lerp(ThrowSpeedMin, ThrowSpeedMax, FMath::Clamp(Charge, 0.f, 1.f));
	const float WeightFactor = Definition->Category == ENDLootCategory::Medium ? 0.6f : 1.f;
	FVector Velocity = FVector(Direction) * Speed * WeightFactor;
	if (const APawn* Pawn = GetPawn())
	{
		Velocity += Pawn->GetVelocity();
	}
	HeldLoot->RemoveCarrier(GetPawn(), Velocity);
}

void UNDCarryComponent::DropEverything()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APawn* Pawn = GetPawn();
	if (HeldLoot)
	{
		HeldLoot->RemoveCarrier(Pawn, FVector::ZeroVector);
	}

	// Los objetos de bolsillo caen alrededor del jugador.
	TArray<TObjectPtr<ANDLootActor>> Pockets = PocketLoot;
	PocketLoot.Reset();
	int32 Index = 0;
	for (ANDLootActor* Loot : Pockets)
	{
		if (!Loot || !Pawn)
		{
			continue;
		}
		const FVector Offset = FRotator(0.f, 120.f * Index++, 0.f).Vector() * 50.f + FVector(0.f, 0.f, 40.f);
		Loot->DropFromPocket(Pawn->GetActorLocation() + Offset);
	}
}

void UNDCarryComponent::NotifyLootReleased(ANDLootActor* Loot)
{
	if (HeldLoot == Loot)
	{
		HeldLoot = nullptr;
	}
	PocketLoot.Remove(Loot);
}
