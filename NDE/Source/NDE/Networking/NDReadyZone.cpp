// No Despiertes Eso

#include "Networking/NDReadyZone.h"

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Networking/NDLobbyGameMode.h"
#include "Players/NDPlayerController.h"

ANDReadyZone::ANDReadyZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zona"));
	Zone->SetBoxExtent(FVector(200.f, 200.f, 150.f));
	// Solo solapa con pawns (el DefaultPawn usa el perfil Pawn); no bloquea nada.
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	// Visible en juego mientras no haya arte propio.
	Zone->SetHiddenInGame(false);
	Zone->ShapeColor = FColor::Green;
	RootComponent = Zone;

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Rotulo"));
	Label->SetupAttachment(Zone);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(40.f);
	Label->SetTextRenderColor(FColor::Green);
	Label->SetText(NSLOCTEXT("NDE", "ReadyZoneLabel", "SALIDA - entra para estar listo"));
}

void ANDReadyZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	HandleOverlap(OtherActor, true);
}

void ANDReadyZone::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	HandleOverlap(OtherActor, false);
}

bool ANDReadyZone::ContainsController(const AController* Controller) const
{
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn && IsOverlappingActor(Pawn);
}

void ANDReadyZone::HandleOverlap(AActor* OtherActor, bool bInside)
{
	if (!HasAuthority())
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(OtherActor);
	ANDPlayerController* PC = Pawn ? Cast<ANDPlayerController>(Pawn->GetController()) : nullptr;
	ANDLobbyGameMode* GM = GetWorld()->GetAuthGameMode<ANDLobbyGameMode>();
	if (!PC || !GM)
	{
		return;
	}
	UE_LOG(LogNDNet, Log, TEXT("%s %s la zona de salida"), *PC->GetName(), bInside ? TEXT("entra en") : TEXT("sale de"));
	GM->HandleReadyRequest(PC, bInside);
}
