#include "Loot/NDExtractionVan.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
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
