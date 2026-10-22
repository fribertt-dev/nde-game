// No Despiertes Eso

#include "HouseGen/NDHouseGameMode.h"

#include "Core/NDLog.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HouseGen/NDHouseLot.h"
#include "Kismet/GameplayStatics.h"

void ANDHouseGameMode::StartPlay()
{
	Super::StartPlay();

	HouseLot = ANDHouseLot::FindLot(GetWorld());
	if (!HouseLot)
	{
		UE_LOG(LogNDHouseGen, Log, TEXT("El mapa no tiene ANDHouseLot; se crea uno en el origen"));
		HouseLot = GetWorld()->SpawnActor<ANDHouseLot>(ANDHouseLot::StaticClass(), FTransform::Identity);
	}
	if (!HouseLot)
	{
		return;
	}

	const FString SeedOption = UGameplayStatics::ParseOption(OptionsString, TEXT("Seed"));
	if (!SeedOption.IsEmpty())
	{
		HouseLot->SetSeedOverride(FCString::Atoi64(*SeedOption));
	}
	HouseLot->OnHouseBuilt.AddDynamic(this, &ThisClass::HandleHouseBuilt);
	if (HouseLot->IsHouseBuilt())
	{
		HandleHouseBuilt(HouseLot);
	}
}

void ANDHouseGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	// Sin casa todavía no hay suelo: el jugador espera y aparece en HandleHouseBuilt.
	if (!HouseLot || !HouseLot->IsHouseBuilt())
	{
		return;
	}
	const int32 SpawnIndex = NextSpawnIndex++;
	RestartPlayerAtTransform(NewPlayer, HouseLot->GetPlayerSpawnTransform(SpawnIndex));
	UE_LOG(LogNDHouseGen, Log, TEXT("%s aparece en la Entrada (spawn %d)"),
		NewPlayer->PlayerState ? *NewPlayer->PlayerState->GetPlayerName() : *NewPlayer->GetName(), SpawnIndex);
}

void ANDHouseGameMode::HandleHouseBuilt(ANDHouseLot* Lot)
{
	// También tras regenerar: los que ya tienen pawn vuelven a la Entrada.
	NextSpawnIndex = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			const FTransform Spawn = Lot->GetPlayerSpawnTransform(NextSpawnIndex++);
			Pawn->TeleportTo(Spawn.GetLocation(), Spawn.Rotator());
			PC->ClientSetRotation(Spawn.Rotator());
		}
		else
		{
			RestartPlayer(PC);
		}
	}
}
