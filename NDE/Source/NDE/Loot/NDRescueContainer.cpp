#include "Loot/NDRescueContainer.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Loot/NDLootActor.h"

namespace NDRescueContainer
{
	constexpr float CheckInterval = 0.25f;
}

ANDRescueContainer::ANDRescueContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;

	// Tarro de 60 x 60 x 70 cm: el objeto se ve dentro. Sin colisión: es un placeholder.
	Jar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Jar"));
	Jar->SetupAttachment(Root);
	Jar->SetStaticMesh(CylinderFinder.Object);
	Jar->SetRelativeLocation(FVector(0.f, 0.f, 35.f));
	Jar->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.7f));
	Jar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	Lid->SetStaticMesh(CylinderFinder.Object);
	Lid->SetRelativeLocation(FVector(0.f, 0.f, 74.f));
	Lid->SetRelativeScale3D(FVector(0.66f, 0.66f, 0.08f));
	Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OpenZone = CreateDefaultSubobject<USphereComponent>(TEXT("OpenZone"));
	OpenZone->SetupAttachment(Root);
	OpenZone->SetSphereRadius(120.f);
	OpenZone->SetCollisionProfileName(TEXT("Trigger"));
}

void ANDRescueContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDRescueContainer, bOpened);
	DOREPLIFETIME(ANDRescueContainer, ContainedLoot);
}

void ANDRescueContainer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	auto Paint = [this](UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (Mesh && BaseMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, Material);
		}
	};
	Paint(Jar, FLinearColor(0.55f, 0.75f, 0.85f));
	Paint(Lid, FLinearColor(0.8f, 0.6f, 0.15f));
}

void ANDRescueContainer::BeginPlay()
{
	Super::BeginPlay();
	ApplyOpenedVisuals();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckOpening, NDRescueContainer::CheckInterval, true);
	}
}

void ANDRescueContainer::SetContainedLoot(ANDLootActor* InLoot)
{
	if (!HasAuthority() || !InLoot)
	{
		return;
	}
	ContainedLoot = InLoot;
	InLoot->SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 25.f));
	InLoot->SetLocked(!bOpened);
}

void ANDRescueContainer::CheckOpening()
{
	if (bOpened)
	{
		GetWorldTimerManager().ClearTimer(CheckTimer);
		return;
	}
	TArray<AActor*> Overlapping;
	OpenZone->GetOverlappingActors(Overlapping, APawn::StaticClass());
	// Quedarse al lado abre el tarro; alejarse lo hace retroceder poco a poco.
	OpenProgress = Overlapping.Num() > 0 ? OpenProgress + NDRescueContainer::CheckInterval : FMath::Max(0.f, OpenProgress - NDRescueContainer::CheckInterval * 0.5f);
	if (OpenProgress >= OpenSeconds)
	{
		Open();
	}
}

void ANDRescueContainer::Open()
{
	if (!HasAuthority() || bOpened)
	{
		return;
	}
	bOpened = true;
	ApplyOpenedVisuals();
	if (ContainedLoot)
	{
		ContainedLoot->SetLocked(false);
	}
	ForceNetUpdate();
}

void ANDRescueContainer::OnRep_Opened()
{
	ApplyOpenedVisuals();
}

void ANDRescueContainer::ApplyOpenedVisuals()
{
	Jar->SetVisibility(!bOpened);
	Lid->SetVisibility(!bOpened);
}
