#include "Loot/NDExtractionVan.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"

ANDExtractionVan::ANDExtractionVan()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	SafeZone = CreateDefaultSubobject<UBoxComponent>(TEXT("SafeZone"));
	SafeZone->SetupAttachment(Root);
	SafeZone->SetBoxExtent(FVector(200.f, 150.f, 120.f));
	SafeZone->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	SafeZone->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	SafeZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SafeZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	SafeZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SafeZone->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SafeZone->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SafeZone->SetGenerateOverlapEvents(true);

	// Greybox: medidas de una camioneta real (~5 x 2 x 2,4 m). Ejes: X hacia la calle, Y a lo largo de la fachada.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;

	// Caja de carga: puertas traseras (Y = 170) hacia la zona de depósito.
	Body->SetStaticMesh(CubeFinder.Object);
	Body->SetRelativeLocation(FVector(0.f, 345.f, 140.f));
	Body->SetRelativeScale3D(FVector(2.f, 3.5f, 2.f));

	Cab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cab"));
	Cab->SetupAttachment(Root);
	Cab->SetStaticMesh(CubeFinder.Object);
	Cab->SetRelativeLocation(FVector(0.f, 595.f, 120.f));
	Cab->SetRelativeScale3D(FVector(2.f, 1.5f, 1.6f));

	const FVector WheelSpots[] = { FVector(-100.f, 230.f, 35.f), FVector(100.f, 230.f, 35.f), FVector(-100.f, 600.f, 35.f), FVector(100.f, 600.f, 35.f) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WheelSpots); ++Index)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"), Index));
		Wheel->SetupAttachment(Root);
		Wheel->SetStaticMesh(CylinderFinder.Object);
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// El cilindro del motor está de pie: tumbado, su eje queda a lo ancho (X).
		Wheel->SetRelativeLocationAndRotation(WheelSpots[Index], FRotator(90.f, 0.f, 0.f));
		Wheel->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.25f));
		Wheels.Add(Wheel);
	}

	// Arte: la malla mide 150 cm de largo (eje Y, morro en +Y); se agranda a 5 m sobre la caja y la cabina.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> VanFinder(TEXT("/Game/_NDE_/Props/Vehicles/SM_Van_Desvelados/SM_Van_Desvelados.SM_Van_Desvelados"));
	ArtMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtMesh"));
	ArtMesh->SetupAttachment(Root);
	ArtMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtMesh->SetCanEverAffectNavigation(false);
	if (VanFinder.Succeeded())
	{
		constexpr float VanScale = 500.f / 150.f;
		ArtMesh->SetStaticMesh(VanFinder.Object);
		ArtMesh->SetRelativeLocation(FVector(0.f, 420.f, 0.f));
		ArtMesh->SetRelativeScale3D(FVector(VanScale));
	}

	DepositRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DepositRing"));
	DepositRing->SetupAttachment(Root);
	DepositRing->SetStaticMesh(CylinderFinder.Object);
	DepositRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DepositRing->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	DepositRing->SetRelativeScale3D(FVector(3.f, 3.f, 0.03f));

	DepositLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DepositLight"));
	DepositLight->SetupAttachment(Root);
	DepositLight->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	DepositLight->SetLightColor(FLinearColor(0.2f, 1.f, 0.3f));
	DepositLight->SetIntensity(4000.f);
	DepositLight->SetAttenuationRadius(450.f);
	DepositLight->SetCastShadows(false);
}

void ANDExtractionVan::OnConstruction(const FTransform& Transform)
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
	Paint(Body, FLinearColor(0.75f, 0.78f, 0.82f));
	Paint(Cab, FLinearColor(0.2f, 0.35f, 0.6f));
	for (UStaticMeshComponent* Wheel : Wheels)
	{
		Paint(Wheel, FLinearColor(0.03f, 0.03f, 0.03f));
	}
	Paint(DepositRing, FLinearColor(0.1f, 0.9f, 0.25f));

	// Sin greybox queda solo la zona (p. ej. si el nivel pone su propia malla en Body). Con la malla de arte,
	// la caja y la cabina siguen dando colisión pero no se ven.
	const bool bHasArt = ArtMesh && ArtMesh->GetStaticMesh();
	for (UStaticMeshComponent* Mesh : { Body.Get(), Cab.Get() })
	{
		Mesh->SetVisibility(bShowGreybox && !bHasArt);
		Mesh->SetCollisionEnabled(bShowGreybox ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Wheel : Wheels)
	{
		Wheel->SetVisibility(bShowGreybox && !bHasArt);
	}
	ArtMesh->SetVisibility(bShowGreybox);
}

void ANDExtractionVan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANDExtractionVan, Occupants);
}

void ANDExtractionVan::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SafeZone->OnComponentBeginOverlap.AddDynamic(this, &ANDExtractionVan::HandleBeginOverlap);
		SafeZone->OnComponentEndOverlap.AddDynamic(this, &ANDExtractionVan::HandleEndOverlap);
		GetWorldTimerManager().SetTimer(SweepTimer, this, &ANDExtractionVan::SweepOccupants, 0.5f, true);

		if (UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this))
		{
			Expedition->RegisterVan(this);
		}
	}
}

bool ANDExtractionVan::IsPawnInside(const APawn* Pawn) const
{
	return Pawn && Occupants.Contains(Pawn);
}

TArray<APawn*> ANDExtractionVan::GetOccupants() const
{
	TArray<APawn*> Result;
	for (APawn* Pawn : Occupants)
	{
		if (Pawn)
		{
			Result.Add(Pawn);
		}
	}
	return Result;
}

void ANDExtractionVan::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		Occupants.AddUnique(Pawn);
		DepositCarriedBy(Pawn);
	}
	else if (ANDLootActor* Loot = Cast<ANDLootActor>(OtherActor))
	{
		// Botín suelto o lanzado dentro de la camioneta.
		if (Loot->GetLootState() == ENDLootState::WorldLoose)
		{
			TryDeposit(Loot, Loot->GetInstigator());
		}
	}
}

void ANDExtractionVan::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		Occupants.Remove(Pawn);
	}
}

void ANDExtractionVan::SweepOccupants()
{
	Occupants.RemoveAll([](const TObjectPtr<APawn>& Pawn) { return !IsValid(Pawn); });
	for (APawn* Pawn : GetOccupants())
	{
		DepositCarriedBy(Pawn);
	}
}

void ANDExtractionVan::DepositCarriedBy(APawn* Pawn)
{
	if (const UNDCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UNDCarryComponent>() : nullptr)
	{
		for (ANDLootActor* Loot : Carry->GetAllCarriedLoot())
		{
			TryDeposit(Loot, Pawn);
		}
	}
}

bool ANDExtractionVan::TryDeposit(ANDLootActor* Loot, APawn* Depositor)
{
	if (!HasAuthority() || !Loot || Loot->GetLootState() == ENDLootState::Deposited)
	{
		return false;
	}

	UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	if (!Expedition || !Expedition->RecordDeposit(Loot, Depositor))
	{
		return false;
	}

	Loot->MarkDeposited();
	return true;
}
