#include "Expedition/NDExpeditionDirector.h"

#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDNightClockComponent.h"

ANDExpeditionDirector::ANDExpeditionDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	NightClock = CreateDefaultSubobject<UNDNightClockComponent>(TEXT("NightClock"));
	Expedition = CreateDefaultSubobject<UNDExpeditionComponent>(TEXT("Expedition"));

	HouseEntry = CreateDefaultSubobject<UBoxComponent>(TEXT("HouseEntry"));
	HouseEntry->SetupAttachment(Root);
	HouseEntry->SetBoxExtent(FVector(100.f, 200.f, 150.f));
	HouseEntry->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	HouseEntry->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HouseEntry->SetCollisionResponseToAllChannels(ECR_Ignore);
	HouseEntry->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HouseEntry->SetGenerateOverlapEvents(true);
}

ANDExpeditionDirector* ANDExpeditionDirector::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ANDExpeditionDirector> It(World);
	return It ? *It : nullptr;
}

UNDExpeditionComponent* ANDExpeditionDirector::GetExpedition(const UObject* WorldContextObject)
{
	const ANDExpeditionDirector* Director = Get(WorldContextObject);
	return Director ? Director->Expedition.Get() : nullptr;
}

UNDNightClockComponent* ANDExpeditionDirector::GetNightClock(const UObject* WorldContextObject)
{
	const ANDExpeditionDirector* Director = Get(WorldContextObject);
	return Director ? Director->NightClock.Get() : nullptr;
}

void ANDExpeditionDirector::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		HouseEntry->OnComponentBeginOverlap.AddDynamic(this, &ANDExpeditionDirector::HandleHouseEntryOverlap);
	}
	UpdateSkyVisuals();
}

void ANDExpeditionDirector::HandleHouseEntryOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<APawn>(OtherActor))
	{
		Expedition->NotifyPlayerEnteredHouse();
	}
}

void ANDExpeditionDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSkyVisuals();
}

void ANDExpeditionDirector::UpdateSkyVisuals()
{
	if (!SkyLightSource || !NightClock)
	{
		return;
	}

	// La luz solo se mueve en el pre-amanecer; antes es noche cerrada.
	const float Alpha = NightClock->GetNightAlpha();
	const float DawnBlend = FMath::Clamp((Alpha - NightClock->PreDawnStartAlpha) / FMath::Max(0.01f, 1.f - NightClock->PreDawnStartAlpha), 0.f, 1.f);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, DawnBlend);

	const FRotator Current = SkyLightSource->GetActorRotation();
	SkyLightSource->SetActorRotation(FRotator(FMath::Lerp(NightLightPitch, DawnLightPitch, Smooth), Current.Yaw, 0.f));
	if (ULightComponent* Light = SkyLightSource->GetLightComponent())
	{
		Light->SetIntensity(FMath::Lerp(NightLightIntensity, DawnLightIntensity, Smooth));
	}
}
