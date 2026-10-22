// No Despiertes Eso

#include "HouseGen/NDHouseGameMode.h"

#include "Components/BoxComponent.h"
#include "Contracts/NDContractDefinition.h"
#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HouseGen/NDHouseLoot.h"
#include "HouseGen/NDHouseLot.h"
#include "Kismet/GameplayStatics.h"
#include "Loot/NDExtractionVan.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Networking/NDExpeditionGameState.h"
#include "UI/NDHUD.h"

namespace NDHouseGameMode
{
	FSoftObjectPath ContractPath(const FString& ContractId)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/_NDE_/Data/Contracts/DA_Contract_%s.DA_Contract_%s"), *ContractId, *ContractId));
	}

	const TCHAR* LootIds[] = { TEXT("L01"), TEXT("L02"), TEXT("L03"), TEXT("L07"), TEXT("L08"), TEXT("L11"), TEXT("L12"), TEXT("L16"), TEXT("L19"), TEXT("L32") };
}

ANDHouseGameMode::ANDHouseGameMode()
{
	// El HUD del jugador local inicializa la UI (reloj, cuota y avisos con datos reales).
	HUDClass = ANDExpeditionHUD::StaticClass();
	DefaultContract = TSoftObjectPtr<UNDContractDefinition>(NDHouseGameMode::ContractPath(TEXT("C01")));
	for (const TCHAR* LootId : NDHouseGameMode::LootIds)
	{
		LootPool.Add(TSoftObjectPtr<UNDLootDefinition>(FSoftObjectPath(FString::Printf(TEXT("/Game/_NDE_/Data/Loot/DA_Loot_%s.DA_Loot_%s"), LootId, LootId))));
	}
}

void ANDHouseGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// La expedición solo admite a quien vuelve tras caerse. Sin datos del lobby
	// (mapa abierto directamente, p. ej. L_CasaPrueba?listen) es una prueba: se deja entrar.
	const UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	if (ErrorMessage.StartsWith(TEXT("NDE_IN_PROGRESS")) && GI && GI->GetExpectedExpeditionPlayers() == 0)
	{
		UE_LOG(LogNDHouseGen, Log, TEXT("Mapa abierto sin lobby: se admite un cliente nuevo (%s)"), *Address);
		ErrorMessage.Reset();
	}
}

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
	SetupExpedition();

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

UNDContractDefinition* ANDHouseGameMode::ResolveContract() const
{
	const ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	const FName ContractId = ExpState ? ExpState->GetContractId() : NAME_None;
	if (!ContractId.IsNone())
	{
		const FString Id = ContractId.ToString();
		const FSoftObjectPath Path = NDHouseGameMode::ContractPath(Id);
		if (UNDContractDefinition* Contract = Cast<UNDContractDefinition>(Path.TryLoad()))
		{
			return Contract;
		}
		UE_LOG(LogNDHouseGen, Warning, TEXT("No existe el contrato %s; se usa el de por defecto"), *Id);
	}
	return DefaultContract.LoadSynchronous();
}

void ANDHouseGameMode::SetupExpedition()
{
	Director = ANDExpeditionDirector::Get(this);
	if (!Director)
	{
		Director = GetWorld()->SpawnActor<ANDExpeditionDirector>(ANDExpeditionDirector::StaticClass(), FTransform::Identity);
	}
	if (!Director || !Director->Expedition)
	{
		return;
	}
	// Un contrato puesto a mano en el director del mapa manda sobre el de por defecto.
	const ANDExpeditionGameState* ExpState = GetGameState<ANDExpeditionGameState>();
	if (!Director->Expedition->Contract || (ExpState && !ExpState->GetContractId().IsNone()))
	{
		Director->Expedition->SetContract(ResolveContract());
	}
	const UNDContractDefinition* Contract = Director->Expedition->Contract;
	UE_LOG(LogNDHouseGen, Log, TEXT("Expedicion en la casa: contrato %s, cuota %d"),
		Contract ? *Contract->ContractId.ToString() : TEXT("ninguno"), Contract ? Contract->RequiredQuotaValue : 0);
}

void ANDHouseGameMode::SetupHouseExpedition(ANDHouseLot* Lot)
{
	// Botín: se borra el de la casa anterior y se reparte el de esta.
	for (ANDLootActor* Loot : HouseLoot)
	{
		if (IsValid(Loot))
		{
			Loot->Destroy();
		}
	}
	HouseLoot.Reset();

	TArray<UNDLootDefinition*> Pool;
	for (const TSoftObjectPtr<UNDLootDefinition>& Entry : LootPool)
	{
		if (UNDLootDefinition* Definition = Entry.LoadSynchronous())
		{
			Pool.Add(Definition);
		}
	}
	const UNDContractDefinition* Contract = Director && Director->Expedition ? Director->Expedition->Contract.Get() : nullptr;
	HouseLoot.Append(FNDHouseLootPlanner::SpawnLoot(*Lot, Contract, Pool));

	// Camioneta en la marca de extracción (jardín delantero). Si el mapa ya tiene una, se mueve.
	const TArray<FTransform> Extractions = Lot->GetMarkerTransforms(ENDRoomMarkerType::Extraction);
	if (!Van)
	{
		TActorIterator<ANDExtractionVan> It(GetWorld());
		Van = It ? *It : nullptr;
	}
	const FVector VanLocation = Extractions.IsEmpty() ? Lot->GetActorLocation() : Extractions[0].GetLocation();
	// La camioneta va de lado, paralela a la fachada (su eje Y), con la caja hacia el centro del lote:
	// la zona de depósito queda delante de la puerta y la carrocería no tapa la entrada.
	const FIntPoint Front = NDHouseGen::DirectionToOffset(Lot->LotInfo.FrontDirection);
	const FVector ToStreet = Lot->GetActorRotation().RotateVector(FVector(Front.X, Front.Y, 0.f));
	FRotator VanRotation = ToStreet.Rotation();
	if (FVector::DotProduct(Lot->GetActorLocation() - VanLocation, FRotationMatrix(VanRotation).GetUnitAxis(EAxis::Y)) < 0.f)
	{
		VanRotation.Yaw += 180.f;
	}
	if (!Van)
	{
		Van = GetWorld()->SpawnActor<ANDExtractionVan>(ANDExtractionVan::StaticClass(), FTransform(VanRotation, VanLocation));
	}
	else
	{
		Van->SetActorLocationAndRotation(VanLocation, VanRotation);
	}
	if (Van && Director && Director->Expedition)
	{
		Director->Expedition->RegisterVan(Van);
		UE_LOG(LogNDHouseGen, Log, TEXT("Camioneta en la extraccion: %s"), *Van->GetActorLocation().ToCompactString());
	}

	// Entrada: cruzar la puerta de la casa arranca el reloj.
	if (Director)
	{
		const FNDHouseLayout& Layout = Lot->GetLayout();
		if (Layout.EntryRoom != INDEX_NONE && Layout.EntryExitSocket != INDEX_NONE)
		{
			const FVector Door = Lot->GetLotToWorld().TransformPosition(Layout.GetSocketLocation(Layout.EntryRoom, Layout.EntryExitSocket));
			Director->SetActorLocationAndRotation(Door, Lot->GetActorRotation());
			Director->HouseEntry->SetBoxExtent(FVector(150.f, 150.f, 150.f));
		}
	}
}

void ANDHouseGameMode::HandleHouseBuilt(ANDHouseLot* Lot)
{
	SetupHouseExpedition(Lot);

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
