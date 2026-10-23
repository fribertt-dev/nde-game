#include "Loot/NDLootActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootDefinition.h"
#include "Tools/NDLoadoutComponent.h"
#include "Tools/NDToolDeployables.h"

ANDLootActor::ANDLootActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	bReplicates = true;
	// Pocos objetos por casa: siempre relevantes, así un cliente que entra tarde o vuelve los recibe todos.
	bAlwaysRelevant = true;
	NetDormancy = DORM_Never;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetGenerateOverlapEvents(true);
	// El botín se mueve y se lleva: no recorta el NavMesh (si no, uno en una puerta cortaría la patrulla).
	Mesh->SetCanEverAffectNavigation(false);

	ArtMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtMesh"));
	ArtMesh->SetupAttachment(Mesh);
	ArtMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtMesh->SetCanEverAffectNavigation(false);
	// A su tamaño real aunque Mesh vaya escalada.
	ArtMesh->SetUsingAbsoluteScale(true);
	SetRootComponent(Mesh);
}

void ANDLootActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANDLootActor, Definition);
	DOREPLIFETIME(ANDLootActor, LootGuid);
	DOREPLIFETIME(ANDLootActor, LootState);
	DOREPLIFETIME(ANDLootActor, Condition);
	DOREPLIFETIME(ANDLootActor, bLocked);
	DOREPLIFETIME(ANDLootActor, RestLocation);
	DOREPLIFETIME(ANDLootActor, RestRotation);
	DOREPLIFETIME(ANDLootActor, Carriers);
}

void ANDLootActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefinitionVisuals();
}

void ANDLootActor::InitializeSpawn(UNDLootDefinition* InDefinition, const FGuid& InStableGuid)
{
	Definition = InDefinition;
	LootGuid = InStableGuid;
}

void ANDLootActor::SetLocked(bool bInLocked)
{
	if (HasAuthority() && bLocked != bInLocked)
	{
		bLocked = bInLocked;
		ApplyStateLocally();
		ForceNetUpdate();
	}
}

void ANDLootActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefinitionVisuals();

	if (HasAuthority())
	{
		if (!LootGuid.IsValid())
		{
			LootGuid = FGuid::NewGuid();
		}
		LastSafeLocation = GetActorLocation();
		// Al aparecer cae unos centímetros: ese golpe no cuenta (los frágiles llegarían dañados sin tocarlos).
		LastImpactTime = GetWorld()->GetTimeSeconds() + SpawnImpactGraceSeconds;
		Mesh->OnComponentHit.AddDynamic(this, &ANDLootActor::HandleMeshHit);
	}

	ApplyStateLocally();
}

void ANDLootActor::ApplyDefinitionVisuals()
{
	if (!Definition || !Mesh)
	{
		return;
	}
	if (!Definition->Mesh.IsNull())
	{
		if (UStaticMesh* LoadedMesh = Definition->Mesh.LoadSynchronous())
		{
			Mesh->SetStaticMesh(LoadedMesh);
		}
	}
	Mesh->SetWorldScale3D(Definition->MeshScale);
	// Arte: visible a su tamaño, con el pivote (abajo) en la base de la forma greybox (de 100 cm, centrada).
	UStaticMesh* Art = Definition->ArtMesh.IsNull() ? nullptr : Definition->ArtMesh.LoadSynchronous();
	ArtMesh->SetStaticMesh(Art);
	ArtMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
	Mesh->SetVisibility(Art == nullptr);
	ArtMesh->SetVisibility(Art != nullptr);
}

void ANDLootActor::OnRep_Definition()
{
	ApplyDefinitionVisuals();
}

void ANDLootActor::OnRep_LootState()
{
	ApplyStateLocally();
}

void ANDLootActor::OnRep_Condition()
{
	OnConditionChanged.Broadcast(this, Condition);
}

void ANDLootActor::OnRep_RestTransform()
{
	ApplyRestTransform();
}

void ANDLootActor::ApplyRestTransform()
{
	if (!HasAuthority() && LootState == ENDLootState::WorldLoose && !GetAttachParentActor() && !RestLocation.IsZero())
	{
		SetActorLocationAndRotation(RestLocation, RestRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ANDLootActor::OnRep_Carriers()
{
	UpdateCarryAttachment();
}

void ANDLootActor::ApplyStateLocally()
{
	// Mientras alguien lo lleva el servidor no manda su posición: cada máquina lo coloca sola.
	if (HasAuthority())
	{
		SetReplicateMovement(LootState == ENDLootState::WorldLoose);
	}

	switch (LootState)
	{
	case ENDLootState::WorldLoose:
		// Se suelta donde se ve ahora (sin saltos) y solo entonces vuelve la física.
		if (GetAttachParentActor())
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		SetActorHiddenInGame(false);
		// La física solo corre en el servidor; los clientes siguen la posición replicada. Si un cliente la
		// simulara, uno que entra tarde lo vería caer antes de tener la casa construida (y quedarse ahí).
		// Bloqueado en un contenedor: quieto y sin física hasta que se abra.
		Mesh->SetCollisionEnabled(bLocked || !HasAuthority() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
		Mesh->SetSimulatePhysics(!bLocked && HasAuthority());
		ApplyRestTransform();
		SetActorTickInterval(0.2f);
		break;
	case ENDLootState::Carried:
		SetActorHiddenInGame(false);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UpdateCarryAttachment();
		break;
	case ENDLootState::Pocketed:
	case ENDLootState::Deposited:
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorHiddenInGame(true);
		break;
	}
}

void ANDLootActor::UpdateCarryAttachment()
{
	if (LootState != ENDLootState::Carried)
	{
		return;
	}
	// Un cuerpo con física ignora el acople: en los clientes puede seguir activa si el estado aún no llegó.
	Mesh->SetSimulatePhysics(false);

	int32 ValidCarriers = 0;
	for (const APawn* Carrier : Carriers)
	{
		ValidCarriers += Carrier ? 1 : 0;
	}
	if (ValidCarriers >= 2)
	{
		// Bulto compartido: suelto y colocado entre los dos cada frame.
		if (GetAttachParentActor())
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		SetActorTickInterval(0.f);
		FTransform Shared;
		if (GetSharedCarryTransform(Shared))
		{
			SetActorLocationAndRotation(Shared.GetLocation(), Shared.GetRotation());
		}
		return;
	}
	SetActorTickInterval(0.2f);
	AttachToLeader();
}

bool ANDLootActor::GetSharedCarryTransform(FTransform& OutTransform) const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const APawn* Carrier : Carriers)
	{
		if (Carrier)
		{
			Sum += Carrier->GetActorLocation();
			++Count;
		}
	}
	const APawn* Leader = GetLeadCarrier();
	if (Count < 2 || !Leader)
	{
		return false;
	}
	// Entre los portadores, a la altura de las manos, mirando hacia donde mira el primero.
	OutTransform = FTransform(FRotator(0.f, Leader->GetActorRotation().Yaw, 0.f), Sum / Count + FVector(0.f, 0.f, GetCarryOffset().Z));
	return true;
}

bool ANDLootActor::IsCarriedBy(const APawn* Pawn) const
{
	return Pawn && Carriers.Contains(Pawn);
}

int32 ANDLootActor::GetEffectiveCarrierCount() const
{
	int32 Count = 0;
	bool bHasCart = false;
	for (const APawn* Carrier : Carriers)
	{
		if (!Carrier)
		{
			continue;
		}
		++Count;
		if (const UNDLoadoutComponent* Loadout = Carrier->FindComponentByClass<UNDLoadoutComponent>())
		{
			bHasCart |= Loadout->HasCart();
		}
	}
	if (bHasCart && Definition && Definition->bCartAllowed)
	{
		++Count;
	}
	return Count;
}

bool ANDLootActor::HasEnoughCarriers() const
{
	return Definition && GetEffectiveCarrierCount() >= Definition->GetRequiredCarriers();
}

int32 ANDLootActor::GetCurrentLootPoints() const
{
	return Definition ? FMath::RoundToInt(Definition->BaseValue * Definition->GetConditionFactor(Condition)) : 0;
}

bool ANDLootActor::CanBeGrabbedBy(const APawn* Pawn) const
{
	if (!Pawn || !Definition || IsCarriedBy(Pawn))
	{
		return false;
	}
	if (bLocked)
	{
		return false;
	}
	switch (LootState)
	{
	case ENDLootState::WorldLoose:
		return true;
	case ENDLootState::Carried:
		// Un segundo jugador puede unirse a un bulto que necesita ayuda.
		return Carriers.Num() < Definition->GetRequiredCarriers();
	default:
		return false;
	}
}

bool ANDLootActor::AddCarrier(APawn* Pawn)
{
	if (!HasAuthority() || !CanBeGrabbedBy(Pawn))
	{
		return false;
	}

	Carriers.Add(Pawn);
	SetInstigator(Pawn);
	if (Carriers.Num() == 1)
	{
		LootState = ENDLootState::Carried;
		ApplyStateLocally();
	}
	else
	{
		UpdateCarryAttachment();
	}
	return true;
}

void ANDLootActor::RemoveCarrier(APawn* Pawn, const FVector& ReleaseVelocity)
{
	if (!HasAuthority() || !Carriers.Contains(Pawn))
	{
		return;
	}

	Carriers.Remove(Pawn);
	if (UNDCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UNDCarryComponent>() : nullptr)
	{
		Carry->NotifyLootReleased(this);
	}

	if (Carriers.Num() == 0)
	{
		if (Pawn)
		{
			LastSafeLocation = Pawn->GetActorLocation();
		}
		ReleaseToWorld(ReleaseVelocity);
	}
	else
	{
		// Queda uno: vuelve a ir acoplado a él.
		UpdateCarryAttachment();
	}
}

void ANDLootActor::SetPocketed(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn)
	{
		return;
	}
	SetInstigator(Pawn);
	PocketOwner = Pawn;
	LootState = ENDLootState::Pocketed;
	ApplyStateLocally();
	AttachToActor(Pawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

void ANDLootActor::DropFromPocket(const FVector& Location)
{
	if (!HasAuthority() || LootState != ENDLootState::Pocketed)
	{
		return;
	}
	if (APawn* PocketPawn = PocketOwner.Get())
	{
		LastSafeLocation = PocketPawn->GetActorLocation();
	}
	PocketOwner.Reset();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	LootState = ENDLootState::WorldLoose;
	ApplyStateLocally();
}

void ANDLootActor::MarkDeposited()
{
	if (!HasAuthority() || LootState == ENDLootState::Deposited)
	{
		return;
	}

	// Se suelta de todos sin devolverlo al mundo.
	TArray<TObjectPtr<APawn>> PreviousCarriers = Carriers;
	Carriers.Reset();
	if (APawn* PocketPawn = PocketOwner.Get())
	{
		PreviousCarriers.Add(PocketPawn);
	}
	PocketOwner.Reset();
	for (APawn* Carrier : PreviousCarriers)
	{
		if (UNDCarryComponent* Carry = Carrier ? Carrier->FindComponentByClass<UNDCarryComponent>() : nullptr)
		{
			Carry->NotifyLootReleased(this);
		}
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	LootState = ENDLootState::Deposited;
	ApplyStateLocally();

	// La riqueza no se representa con mallas amontonadas (GDD 5.7).
	SetLifeSpan(1.f);
}

void ANDLootActor::AttachToLeader()
{
	APawn* Leader = GetLeadCarrier();
	if (!Leader)
	{
		return;
	}
	AttachToActor(Leader, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(GetCarryOffset());
	SetActorRelativeRotation(FRotator::ZeroRotator);
}

FVector ANDLootActor::GetCarryOffset() const
{
	if (!Definition)
	{
		return FVector(60.f, 0.f, 10.f);
	}
	switch (Definition->Category)
	{
	case ENDLootCategory::Medium: return FVector(70.f, 0.f, 20.f);
	case ENDLootCategory::Heavy:  return FVector(110.f, 0.f, -10.f);
	case ENDLootCategory::Huge:   return FVector(140.f, 0.f, -20.f);
	default:                      return FVector(60.f, 0.f, 10.f);
	}
}

void ANDLootActor::ReleaseToWorld(const FVector& ReleaseVelocity)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	LootState = ENDLootState::WorldLoose;
	ApplyStateLocally();
	if (!ReleaseVelocity.IsNearlyZero())
	{
		Mesh->SetPhysicsLinearVelocity(ReleaseVelocity);
	}
}

void ANDLootActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Bulto compartido: todas las máquinas lo colocan entre los portadores.
	FTransform Shared;
	if (LootState == ENDLootState::Carried && !GetAttachParentActor() && GetSharedCarryTransform(Shared))
	{
		SetActorLocationAndRotation(Shared.GetLocation(), Shared.GetRotation());
	}

	// Servidor: cuando el objeto suelto se queda quieto, se guarda su sitio para los clientes.
	if (HasAuthority() && LootState == ENDLootState::WorldLoose && GetVelocity().SizeSquared() < 1.f
		&& FVector::DistSquared(GetActorLocation(), RestLocation) > FMath::Square(2.f))
	{
		RestLocation = GetActorLocation();
		RestRotation = GetActorRotation();
	}

	if (!HasAuthority() || LootState != ENDLootState::Carried || Carriers.Num() < 2)
	{
		return;
	}

	// Si los portadores se separan demasiado, el que se aleja suelta el bulto.
	const APawn* Leader = GetLeadCarrier();
	for (int32 Index = Carriers.Num() - 1; Index >= 1; --Index)
	{
		APawn* Helper = Carriers[Index];
		if (!Helper || !Leader || FVector::Dist(Helper->GetActorLocation(), Leader->GetActorLocation()) > MaxCoCarryDistance)
		{
			RemoveCarrier(Helper, FVector::ZeroVector);
		}
	}
}

void ANDLootActor::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (LootState != ENDLootState::WorldLoose)
	{
		return;
	}
	const float Mass = FMath::Max(1.f, Mesh->GetMass());
	ApplyImpact(NormalImpulse.Size() / Mass, Hit.ImpactPoint);
}

void ANDLootActor::ApplyImpact(float DeltaSpeed, const FVector& Location)
{
	if (!HasAuthority() || !Definition)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastImpactTime < ImpactCooldownSeconds)
	{
		return;
	}

	const float DamageThreshold = Definition->GetDamageSpeedThreshold();
	if (DeltaSpeed < DamageThreshold * 0.5f)
	{
		return;
	}
	LastImpactTime = Now;

	// Ruido del golpe para el sistema de sigilo. La espuma silenciosa lo amortigua.
	const float NoiseScale = FMath::Clamp(DeltaSpeed / FMath::Max(1.f, DamageThreshold), 0.2f, 1.5f);
	const float Muffle = ANDSilentFoamPatch::GetNoiseMultiplierAt(this, Location);
	MakeNoise(FMath::Clamp(Definition->ImpactNoiseLoudness * NoiseScale * Muffle, 0.f, 1.f), GetInstigator(), Location);

	if (Muffle < 1.f)
	{
		// Sobre espuma el golpe no daña (GDD 18, T17).
		return;
	}

	if (DeltaSpeed >= Definition->GetBreakSpeedThreshold())
	{
		SetCondition(ENDLootCondition::Broken);
	}
	else if (DeltaSpeed >= DamageThreshold && Condition == ENDLootCondition::Intact)
	{
		SetCondition(ENDLootCondition::Damaged);
	}
}

void ANDLootActor::SetCondition(ENDLootCondition NewCondition)
{
	if (NewCondition <= Condition)
	{
		return;
	}
	Condition = NewCondition;
	OnConditionChanged.Broadcast(this, Condition);

	if (Condition == ENDLootCondition::Broken)
	{
		if (UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this))
		{
			Expedition->NotifyLootBroken(this);
		}
	}
}

void ANDLootActor::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!HasAuthority())
	{
		return;
	}

	// Nunca se destruye ni se duplica: vuelve al último punto seguro con penalización.
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	SetActorLocation(LastSafeLocation + FVector(0.f, 0.f, 50.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Condition == ENDLootCondition::Intact)
	{
		SetCondition(ENDLootCondition::Damaged);
	}
}

void ANDLootActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Depuración de red: un objeto que desaparece en un cliente sin haberse depositado es un error.
	UE_CLOG(!HasAuthority() && LootState != ENDLootState::Deposited && EndPlayReason == EEndPlayReason::Destroyed, LogTemp, Warning,
		TEXT("NDLoot: %s (%s) quitado en el cliente sin depositarse"), *LootGuid.ToString(), Definition ? *Definition->LootId.ToString() : TEXT("?"));
	Super::EndPlay(EndPlayReason);
}
