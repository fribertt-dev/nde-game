#include "Tools/NDToolDeployables.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ANDSilentFoamPatch::ANDSilentFoamPatch()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void ANDSilentFoamPatch::BeginPlay()
{
	Super::BeginPlay();

	// El cilindro básico mide 100 uu de diámetro y 100 de alto.
	SetActorScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.04f));
	if (HasAuthority())
	{
		SetLifeSpan(DurationSeconds);
	}
}

float ANDSilentFoamPatch::GetNoiseMultiplierAt(const UObject* WorldContextObject, FVector Location)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 1.f;
	}

	float Multiplier = 1.f;
	for (TActorIterator<ANDSilentFoamPatch> It(World); It; ++It)
	{
		const FVector Delta = Location - It->GetActorLocation();
		if (Delta.Size2D() <= It->Radius && FMath::Abs(Delta.Z) <= 80.f)
		{
			Multiplier = FMath::Min(Multiplier, It->NoiseMultiplier);
		}
	}
	return Multiplier;
}

ANDWindUpMouse::ANDWindUpMouse()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.2f, 0.12f, 0.12f));
}

void ANDWindUpMouse::BeginPlay()
{
	Super::BeginPlay();

	SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 10.f));
	if (HasAuthority())
	{
		SetLifeSpan(DurationSeconds);
	}
}

void ANDWindUpMouse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	FHitResult Hit;
	AddActorWorldOffset(GetActorForwardVector() * Speed * DeltaSeconds, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Choca y da media vuelta: a veces vuelve hacia quien lo soltó (GDD 18, T18).
		AddActorWorldRotation(FRotator(0.f, Bounces % 2 == 0 ? 180.f : 135.f, 0.f));
		++Bounces;
	}

	NoiseAccumulator += DeltaSeconds;
	if (NoiseAccumulator >= NoiseInterval)
	{
		NoiseAccumulator = 0.f;
		MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation());
	}
}
