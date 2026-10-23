// No Despiertes Eso

#include "Monsters/NDMonsterCage.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Monsters/NDMonsterSettings.h"
#include "Monsters/NDMonsterSubsystem.h"
#include "Monsters/NDMonsterUtils.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace NDMonsterCage
{
	constexpr float CheckInterval = 0.25f;
	/** Medio lado interior y alto de la jaula, en cm (caben cuatro Desvelados de 44 cm). */
	constexpr float HalfSize = 85.f;
	constexpr float Height = 150.f;
	constexpr int32 BarsPerSide = 4;
	const FName RescueNoiseTag(TEXT("NDE.Noise.Rescue"));
}

ANDMonsterCage::ANDMonsterCage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = MaterialFinder.Object;

	auto MakeVisual = [this](FName Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Shape = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Shape->SetupAttachment(Root);
		Shape->SetStaticMesh(Mesh);
		Shape->SetRelativeLocation(Location);
		Shape->SetRelativeScale3D(Scale);
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetCanEverAffectNavigation(false);
		return Shape;
	};

	const float Side = NDMonsterCage::HalfSize * 2.f / 100.f;
	BaseMesh = MakeVisual(TEXT("Base"), CubeFinder.Object, FVector(0.f, 0.f, 3.f), FVector(Side + 0.1f, Side + 0.1f, 0.06f));
	LidMesh = MakeVisual(TEXT("Lid"), CubeFinder.Object, FVector(0.f, 0.f, NDMonsterCage::Height), FVector(Side + 0.1f, Side + 0.1f, 0.06f));

	// Barrotes por los cuatro lados.
	int32 BarIndex = 0;
	for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
	{
		for (int32 Step = 0; Step < NDMonsterCage::BarsPerSide; ++Step)
		{
			const float Along = -NDMonsterCage::HalfSize + (Step + 0.5f) * (NDMonsterCage::HalfSize * 2.f / NDMonsterCage::BarsPerSide);
			FVector Location;
			switch (SideIndex)
			{
			case 0: Location = FVector(NDMonsterCage::HalfSize, Along, 0.f); break;
			case 1: Location = FVector(-NDMonsterCage::HalfSize, Along, 0.f); break;
			case 2: Location = FVector(Along, NDMonsterCage::HalfSize, 0.f); break;
			default: Location = FVector(Along, -NDMonsterCage::HalfSize, 0.f); break;
			}
			Location.Z = NDMonsterCage::Height * 0.5f;
			Bars.Add(MakeVisual(*FString::Printf(TEXT("Bar%d"), BarIndex++), CylinderFinder.Object, Location, FVector(0.06f, 0.06f, NDMonsterCage::Height / 100.f)));
		}
	}

	// Paredes invisibles que encierran (y techo, para no salir saltando).
	auto MakeWall = [this](FName Name, const FVector& Location, const FVector& Extent)
	{
		UBoxComponent* Wall = CreateDefaultSubobject<UBoxComponent>(Name);
		Wall->SetupAttachment(Root);
		Wall->SetRelativeLocation(Location);
		Wall->SetBoxExtent(Extent);
		Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Walls.Add(Wall);
	};
	const float H = NDMonsterCage::Height * 0.5f;
	const float S = NDMonsterCage::HalfSize;
	MakeWall(TEXT("WallN"), FVector(S + 5.f, 0.f, H), FVector(5.f, S + 10.f, H));
	MakeWall(TEXT("WallS"), FVector(-S - 5.f, 0.f, H), FVector(5.f, S + 10.f, H));
	MakeWall(TEXT("WallE"), FVector(0.f, S + 5.f, H), FVector(S + 10.f, 5.f, H));
	MakeWall(TEXT("WallW"), FVector(0.f, -S - 5.f, H), FVector(S + 10.f, 5.f, H));
	MakeWall(TEXT("WallTop"), FVector(0.f, 0.f, NDMonsterCage::Height + 5.f), FVector(S + 10.f, S + 10.f, 5.f));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, NDMonsterCage::Height + 60.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(40.f);
	Label->SetTextRenderColor(FColor(255, 220, 120));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANDMonsterCage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDMonsterCage, Prisoners);
	DOREPLIFETIME(ANDMonsterCage, RescueProgress);
}

void ANDMonsterCage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!ShapeMaterial)
	{
		return;
	}
	auto Paint = [this](UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (Mesh)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, Material);
		}
	};
	Paint(BaseMesh, FLinearColor(0.35f, 0.22f, 0.12f));
	Paint(LidMesh, FLinearColor(0.35f, 0.22f, 0.12f));
	for (UStaticMeshComponent* Bar : Bars)
	{
		Paint(Bar, FLinearColor(0.55f, 0.55f, 0.6f));
	}
}

void ANDMonsterCage::BeginPlay()
{
	Super::BeginPlay();
	if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this))
	{
		Monsters->RegisterCage(this);
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckRescue, NDMonsterCage::CheckInterval, true);
	}
	UpdateLabel();
}

void ANDMonsterCage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLocalInputBlocked)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetIgnoreMoveInput(false);
		}
		bLocalInputBlocked = false;
	}
	Super::EndPlay(EndPlayReason);
}

bool ANDMonsterCage::IsImprisoned(const APawn* Pawn) const
{
	return Pawn && Prisoners.ContainsByPredicate([Pawn](const TObjectPtr<APawn>& P) { return P.Get() == Pawn; });
}

FVector ANDMonsterCage::GetSlotLocation(int32 Index) const
{
	static const FVector2D Slots[] = { { 35.f, 35.f }, { -35.f, -35.f }, { 35.f, -35.f }, { -35.f, 35.f } };
	const FVector2D Slot = Slots[Index % UE_ARRAY_COUNT(Slots)];
	return GetActorTransform().TransformPosition(FVector(Slot.X, Slot.Y, 0.f));
}

void ANDMonsterCage::Imprison(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn || IsImprisoned(Pawn))
	{
		return;
	}
	const int32 Slot = Prisoners.Num();
	Prisoners.Add(Pawn);
	RescueProgress = 0.f;
	const float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	Pawn->SetActorLocation(GetSlotLocation(Slot) + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	OnRep_Prisoners();
	ForceNetUpdate();
}

FVector ANDMonsterCage::FindExitLocation(int32 Index) const
{
	const UWorld* World = GetWorld();
	const FVector Center = GetActorLocation();
	const float Out = NDMonsterCage::HalfSize + 80.f;
	const FVector Directions[] = { GetActorForwardVector(), -GetActorForwardVector(), GetActorRightVector(), -GetActorRightVector() };
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDMonsterCageExit), false, this);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(26.f, 40.f);
	for (int32 Try = 0; Try < UE_ARRAY_COUNT(Directions); ++Try)
	{
		const FVector Direction = Directions[(Index + Try) % UE_ARRAY_COUNT(Directions)];
		const FVector Candidate = Center + Direction * Out + FVector(0.f, 0.f, 50.f);
		if (World && !World->OverlapAnyTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			return Candidate;
		}
	}
	// Sin sitio libre alrededor: encima de la jaula.
	return Center + FVector(0.f, 0.f, NDMonsterCage::Height + 60.f);
}

void ANDMonsterCage::ReleaseAll(APawn* Rescuer)
{
	if (!HasAuthority() || Prisoners.IsEmpty())
	{
		return;
	}
	UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	TArray<TObjectPtr<APawn>> Released = MoveTemp(Prisoners);
	Prisoners.Reset();
	RescueProgress = 0.f;
	for (int32 Index = 0; Index < Released.Num(); ++Index)
	{
		APawn* Pawn = Released[Index];
		if (!IsValid(Pawn))
		{
			continue;
		}
		const FVector Exit = FindExitLocation(Index);
		Pawn->SetActorLocation(Exit + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() - 40.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Expedition && Pawn->GetPlayerState())
		{
			Expedition->SetParticipantCaptured(Pawn->GetPlayerState(), false);
		}
	}
	// Abrir el pestillo hace ruido medio (GDD 7.4): la criatura puede oírlo.
	const UNDMonsterSettings* Settings = UNDMonsterSettings::Get();
	if (Rescuer && Settings->RescueNoise > 0.f)
	{
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), Settings->RescueNoise / 100.f, Rescuer, 0.f, NDMonsterCage::RescueNoiseTag);
	}
	OnRep_Prisoners();
	ForceNetUpdate();
}

void ANDMonsterCage::CheckRescue()
{
	// Quien se fue de la partida ya no cuenta.
	if (Prisoners.RemoveAll([](const TObjectPtr<APawn>& P) { return !IsValid(P); }) > 0)
	{
		OnRep_Prisoners();
	}
	if (Prisoners.IsEmpty())
	{
		RescueProgress = 0.f;
		return;
	}

	APawn* Rescuer = nullptr;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		const FVector Center = GetActorLocation();
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			if (!Pawn || IsImprisoned(Pawn) || !NDMonsterUtils::IsTargetablePlayer(Pawn))
			{
				continue;
			}
			const FVector Delta = Pawn->GetActorLocation() - Center;
			if (FVector2D(Delta).Size() <= RescueRadius && FMath::Abs(Delta.Z) < 250.f)
			{
				Rescuer = Pawn;
				break;
			}
		}
	}

	const float RescueSeconds = FMath::Max(1.f, UNDMonsterSettings::Get()->RescueSeconds);
	const float Step = NDMonsterCage::CheckInterval / RescueSeconds;
	RescueProgress = Rescuer ? RescueProgress + Step : FMath::Max(0.f, RescueProgress - Step * 0.5f);
	if (RescueProgress >= 1.f)
	{
		ReleaseAll(Rescuer);
	}
}

void ANDMonsterCage::OnRep_Prisoners()
{
	UpdateLocalPrisoner();
	UpdateLabel();
}

void ANDMonsterCage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLocalPrisoner();
	UpdateLabel();
	if (GetNetMode() != NM_DedicatedServer)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
		// La tapa se levanta un poco al abrirse.
		const float LidTarget = Prisoners.IsEmpty() ? NDMonsterCage::Height + 40.f : NDMonsterCage::Height;
		const FVector Lid = LidMesh->GetRelativeLocation();
		LidMesh->SetRelativeLocation(FVector(Lid.X, Lid.Y, FMath::FInterpTo(Lid.Z, LidTarget, DeltaSeconds, 3.f)));
	}
}

void ANDMonsterCage::UpdateLocalPrisoner()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	const bool bPrisoner = IsImprisoned(PC->GetPawn());
	if (bPrisoner != bLocalInputBlocked)
	{
		PC->SetIgnoreMoveInput(bPrisoner);
		bLocalInputBlocked = bPrisoner;
	}
	if (bPrisoner != bLocalWasPrisoner)
	{
		bLocalWasPrisoner = bPrisoner;
		NDMonsterUtils::ShowLocalToast(PC, bPrisoner
			? NSLOCTEXT("NDE", "MonsterCaged", "¡Te ha atrapado! Estás en la jaula: un compañero tiene que quedarse a tu lado para abrirla.")
			: NSLOCTEXT("NDE", "MonsterRescued", "¡Libre! Sal de aquí antes de que vuelva."), 5.f, 8);
	}
}

void ANDMonsterCage::UpdateLabel()
{
	if (!Label)
	{
		return;
	}
	if (Prisoners.IsEmpty())
	{
		Label->SetText(FText::GetEmpty());
		return;
	}
	Label->SetText(FText::FromString(FString::Printf(TEXT("AYUDA x%d  %d%%"), Prisoners.Num(), FMath::RoundToInt(RescueProgress * 100.f))));
}
