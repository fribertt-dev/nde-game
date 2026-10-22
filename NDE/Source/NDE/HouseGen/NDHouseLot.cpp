// No Despiertes Eso

#include "HouseGen/NDHouseLot.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/NDLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HouseGen/NDHouseGenDebug.h"
#include "HouseGen/NDHouseGenerator.h"
#include "HouseGen/NDRoomModuleActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Networking/NDExpeditionGameState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace NDHouseLot
{
	constexpr float NavCheckInterval = 1.f;
	constexpr int32 MaxNavChecks = 30;

	int64 MakeRandomSeed()
	{
		const FGuid Guid = FGuid::NewGuid();
		// Positiva y legible en el log.
		return ((static_cast<int64>(Guid.A) << 31) ^ static_cast<int64>(Guid.B)) & 0x7FFFFFFFFFFFLL;
	}

	void SetColor(UInstancedStaticMeshComponent* Meshes, UMaterialInterface* BaseMaterial, UObject* Outer, const FLinearColor& Color)
	{
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Meshes->SetMaterial(0, Material);
		}
	}

	void Regenerate(const TArray<FString>& Args, UWorld* World)
	{
		ANDHouseLot* Lot = ANDHouseLot::FindLot(World);
		if (!Lot)
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("nde.HouseGen.Regenerate: no hay ningun ANDHouseLot en el mapa"));
			return;
		}
		if (!Lot->HasAuthority())
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("nde.HouseGen.Regenerate: solo funciona en el servidor (ventana del host)"));
			return;
		}
		const int64 Seed = Args.Num() > 0 ? FCString::Atoi64(*Args[0]) : MakeRandomSeed();
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		if (Delay <= 0.f)
		{
			Lot->GenerateHouse(Seed);
			return;
		}
		// Con espera sirve para lanzarlo desde -ExecCmds y ver el cambio con clientes ya dentro.
		FTimerHandle Handle;
		TWeakObjectPtr<ANDHouseLot> WeakLot(Lot);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakLot, Seed]()
		{
			if (WeakLot.IsValid())
			{
				WeakLot->GenerateHouse(Seed);
			}
		}), Delay, false);
		UE_LOG(LogNDHouseGen, Log, TEXT("nde.HouseGen.Regenerate: semilla %lld dentro de %.0f s"), Seed, Delay);
	}

	FAutoConsoleCommandWithWorldAndArgs RegenerateCommand(
		TEXT("nde.HouseGen.Regenerate"),
		TEXT("Regenera la casa del lote con la semilla dada (al azar si no se da). Solo en el servidor. Uso: nde.HouseGen.Regenerate 1234 [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Regenerate));
}

ANDHouseLot::ANDHouseLot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	LotBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("LotBounds"));
	LotBounds->SetupAttachment(Root);
	LotBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LotBounds->SetCanEverAffectNavigation(false);
	LotBounds->SetHiddenInGame(true);
	LotBounds->ShapeColor = FColor::White;

	FrontArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FrontArrow"));
	FrontArrow->SetupAttachment(Root);
	FrontArrow->SetHiddenInGame(true);
	FrontArrow->ArrowSize = 8.f;
	FrontArrow->ArrowColor = FColor::Yellow;

	GroundMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ground"));
	GroundMeshes->SetupAttachment(Root);
	GroundMeshes->SetStaticMesh(CubeFinder.Object);
	GroundMeshes->SetMobility(EComponentMobility::Movable);
	GroundMeshes->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	ExtractionMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Extraction"));
	ExtractionMeshes->SetupAttachment(Root);
	ExtractionMeshes->SetStaticMesh(CubeFinder.Object);
	ExtractionMeshes->SetMobility(EComponentMobility::Movable);
	ExtractionMeshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ExtractionMeshes->SetCanEverAffectNavigation(false);

	UpdateEditorBounds();
}

void ANDHouseLot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDHouseLot, Manifest);
}

void ANDHouseLot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateEditorBounds();
}

void ANDHouseLot::UpdateEditorBounds()
{
	// Muestra el lote ya agrandado para el tamaño de casa elegido.
	const FIntPoint MinSize = FNDHouseGenParams::GetMinLotSize(HouseSize);
	const FIntPoint Size(FMath::Max(LotInfo.SizeCells.X, MinSize.X), FMath::Max(LotInfo.SizeCells.Y, MinSize.Y));
	const FVector Extent(Size.X * NDHouseGen::CellSize * 0.5f, Size.Y * NDHouseGen::CellSize * 0.5f, 50.f);
	LotBounds->SetBoxExtent(Extent);
	LotBounds->SetRelativeLocation(FVector(0.f, 0.f, Extent.Z));

	const FIntPoint Front = NDHouseGen::DirectionToOffset(LotInfo.FrontDirection);
	FrontArrow->SetRelativeLocationAndRotation(
		FVector(Front.X * Extent.X, Front.Y * Extent.Y, 50.f),
		FRotator(0.f, static_cast<int32>(LotInfo.FrontDirection) * 90.f, 0.f));
}

void ANDHouseLot::BeginPlay()
{
	Super::BeginPlay();

	NDHouseLot::SetColor(GroundMeshes, BaseMaterial, this, FLinearColor(0.18f, 0.35f, 0.12f));
	NDHouseLot::SetColor(ExtractionMeshes, BaseMaterial, this, FLinearColor(0.1f, 0.9f, 0.2f));
	if (HasAuthority())
	{
		if (bGenerateOnBeginPlay)
		{
			// Un tick después, para que el GameMode haya fijado contrato y semilla.
			GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::GenerateOnStart);
		}
	}
	else if (Manifest.IsValid() && !bHouseBuilt)
	{
		// El manifiesto llegó antes de BeginPlay.
		BuildFromManifest();
	}
}

void ANDHouseLot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(NavCheckTimer);
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		ClearHouse();
	}
	Super::EndPlay(EndPlayReason);
}

void ANDHouseLot::SetSeedOverride(int64 Seed)
{
	bHasSeedOverride = true;
	SeedOverride = Seed;
}

void ANDHouseLot::GenerateOnStart()
{
	if (Manifest.IsValid())
	{
		return;
	}
	GenerateHouse(bHasSeedOverride ? SeedOverride : (bUseFixedSeed ? FixedSeed : NDHouseLot::MakeRandomSeed()));
}

FNDHouseGenParams ANDHouseLot::MakeParams(int64 Seed) const
{
	FNDHouseGenParams Params = FNDHouseGenParams::ForArchetype(HouseArchetype, HouseSize);
	Params.Seed = Seed;
	Params.District = District;
	Params.OverrideRoomRange(MinRooms, MaxRooms);
	FNDHouseLotInfo Lot = LotInfo;
	Lot.LotTransform = GetActorTransform();
	Params.SetLot(Lot);
	if (const ANDExpeditionGameState* ExpState = GetWorld() ? GetWorld()->GetGameState<ANDExpeditionGameState>() : nullptr)
	{
		Params.ContractId = ExpState->GetContractId();
	}
	return Params;
}

void ANDHouseLot::GenerateHouse(int64 Seed)
{
	if (!HasAuthority())
	{
		return;
	}

	FNDHouseGenerationManifest NewManifest;
	TArray<FString> Errors;
	if (!FNDHouseGenerator::Generate(MakeParams(Seed), NewManifest, &Errors))
	{
		UE_LOG(LogNDHouseGen, Error, TEXT("Lote %s: no se pudo generar la casa con la semilla %lld"), *LotInfo.LotId.ToString(), Seed);
		return;
	}

	NewManifest.Revision = Manifest.Revision + 1;
	Manifest = MoveTemp(NewManifest);
	UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: casa %s semilla %lld, %d salas, %d intentos%s"),
		*LotInfo.LotId.ToString(), *Manifest.HouseArchetype.ToString(), Manifest.RunSeed, Manifest.Rooms.Num(), Manifest.Attempts,
		Manifest.bUsedFallback ? TEXT(" (casa de respaldo)") : TEXT(""));
	UE_LOG(LogNDHouseGen, Verbose, TEXT("%s"), *Manifest.ToDebugString());

	ForceNetUpdate();
	// En el servidor no salta el OnRep: se construye aquí.
	BuildFromManifest();
}

void ANDHouseLot::OnRep_Manifest()
{
	if (HasActorBegunPlay())
	{
		BuildFromManifest();
	}
}

void ANDHouseLot::ClearHouse()
{
	for (ANDRoomModuleActor* RoomActor : RoomActors)
	{
		if (IsValid(RoomActor))
		{
			RoomActor->Destroy();
		}
	}
	RoomActors.Reset();
	GroundMeshes->ClearInstances();
	ExtractionMeshes->ClearInstances();
	bHouseBuilt = false;
	bNavPathVerified = false;
}

void ANDHouseLot::BuildFromManifest()
{
	ClearHouse();

	FString Error;
	if (!FNDHouseLayout::FromManifest(Manifest, Layout, Error))
	{
		UE_LOG(LogNDHouseGen, Error, TEXT("Lote %s: manifiesto no valido: %s"), *LotInfo.LotId.ToString(), *Error);
		return;
	}

	UWorld* World = GetWorld();
	const FTransform LotToWorld = GetLotToWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TSet<FIntPoint> UsedCells;
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
		const FTransform RoomTransform = FTransform(FRotator(0.f, Room.YawSteps * 90.f, 0.f), Layout.GetRoomCenter(RoomIndex)) * LotToWorld;
		ANDRoomModuleActor* RoomActor = World->SpawnActor<ANDRoomModuleActor>(ANDRoomModuleActor::StaticClass(), RoomTransform, SpawnParams);
		if (!RoomActor)
		{
			continue;
		}

		// Los huecos del layout van en el mismo orden que en la definición.
		TArray<float> Openings;
		for (const FNDHouseLayoutSocket& Socket : Room.Sockets)
		{
			Openings.Add(Socket.Connection != INDEX_NONE ? Layout.Connections[Socket.Connection].OpeningWidth : (Socket.bExit ? Socket.Width : 0.f));
		}
		RoomActor->RoomInstanceId = RoomIndex;
		RoomActor->BuildGreybox(*Room.Def, Openings, bBuildCeiling, bShowMarkers, bLightRooms);
		RoomActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
		RoomActor->SetActorLabel(FString::Printf(TEXT("Room%02d_%s"), RoomIndex, *Room.Def->RoomModuleId.ToString()));
#endif
		RoomActors.Add(RoomActor);

		TArray<FIntPoint> Cells;
		Layout.GetRoomCells(RoomIndex, Cells);
		UsedCells.Append(Cells);
	}

	// Césped en el resto del lote (patios y jardín delantero), a ras de los suelos.
	for (int32 X = 0; X < Layout.Lot.SizeCells.X; ++X)
	{
		for (int32 Y = 0; Y < Layout.Lot.SizeCells.Y; ++Y)
		{
			if (!UsedCells.Contains(FIntPoint(X, Y)))
			{
				const FVector Center = Layout.Lot.GetCellCenter(FIntPoint(X, Y)) - FVector(0.f, 0.f, NDHouseGen::WallThickness * 0.5f);
				GroundMeshes->AddInstance(FTransform(FRotator::ZeroRotator, Center, FVector(NDHouseGen::CellSize, NDHouseGen::CellSize, NDHouseGen::WallThickness) / 100.f));
			}
		}
	}
	for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
	{
		if (Marker.Type == ENDRoomMarkerType::Extraction)
		{
			ExtractionMeshes->AddInstance(FTransform(Marker.LotTransform.GetRotation(), Marker.LotTransform.GetLocation() + FVector(0.f, 0.f, 3.f), FVector(2.4f, 2.4f, 0.06f)));
		}
	}

	bHouseBuilt = true;
	UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: casa construida (%s, revision %d, semilla %lld, %d salas, hash %08X)"),
		*LotInfo.LotId.ToString(), HasAuthority() ? TEXT("servidor") : TEXT("cliente"), Manifest.Revision, Manifest.RunSeed,
		Layout.Rooms.Num(), GetTypeHash(Manifest.ToDebugString()));

	if (HasAuthority())
	{
		if (bDrawDebugGraph)
		{
			FlushPersistentDebugLines(World);
			NDHouseGenDebug::DrawLayout(World, LotToWorld, Layout, MakeParams(Manifest.RunSeed).CarryEnvelopeWidth, true);
		}
		NavCheckTries = 0;
		GetWorldTimerManager().SetTimer(NavCheckTimer, this, &ThisClass::CheckNavigationPath, NDHouseLot::NavCheckInterval, true);
	}
	OnHouseBuilt.Broadcast(this);
}

void ANDHouseLot::CheckNavigationPath()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	++NavCheckTries;
	if (!NavSys || !NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		if (NavCheckTries >= 3)
		{
			GetWorldTimerManager().ClearTimer(NavCheckTimer);
			UE_LOG(LogNDHouseGen, Warning, TEXT("Lote %s: no hay NavMesh. Pon un NavMeshBoundsVolume que cubra el lote."), *LotInfo.LotId.ToString());
		}
		return;
	}
	if (NavSys->IsNavigationBuildInProgress() && NavCheckTries < NDHouseLot::MaxNavChecks)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(NavCheckTimer);

	if (!Layout.Rooms.IsValidIndex(Layout.EntryRoom) || !Layout.Rooms.IsValidIndex(Layout.BedroomRoom))
	{
		return;
	}
	const FTransform LotToWorld = GetLotToWorld();
	const FVector Start = LotToWorld.TransformPosition(Layout.GetRoomCenter(Layout.EntryRoom));
	const FVector End = LotToWorld.TransformPosition(Layout.GetRoomCenter(Layout.BedroomRoom));
	const FVector Extent(150.f, 150.f, 300.f);
	FNavLocation StartOnNav;
	FNavLocation EndOnNav;
	const bool bProjected = NavSys->ProjectPointToNavigation(Start, StartOnNav, Extent) && NavSys->ProjectPointToNavigation(End, EndOnNav, Extent);
	UNavigationPath* Path = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(this, StartOnNav.Location, EndOnNav.Location) : nullptr;
	bNavPathVerified = Path && Path->IsValid() && !Path->IsPartial();

	if (bNavPathVerified)
	{
		UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: PathExists Entrada -> Dormitorio OK (%.0f cm)"), *LotInfo.LotId.ToString(), Path->GetPathLength());
	}
	else
	{
		UE_LOG(LogNDHouseGen, Warning, TEXT("Lote %s: PathExists Entrada -> Dormitorio FALLA (%s)"), *LotInfo.LotId.ToString(),
			bProjected ? TEXT("sin camino completo") : TEXT("entrada o dormitorio fuera del NavMesh; revisa que RecastNavMesh tenga Runtime Generation = Dynamic"));
	}
}

FTransform ANDHouseLot::GetPlayerSpawnTransform(int32 Index) const
{
	const TArray<FTransform> Spawns = GetMarkerTransforms(ENDRoomMarkerType::PlayerSpawn);
	if (Spawns.IsEmpty())
	{
		return FTransform(GetActorRotation(), GetActorLocation() + FVector(0.f, 0.f, 100.f));
	}
	return Spawns[FMath::Abs(Index) % Spawns.Num()];
}

TArray<FTransform> ANDHouseLot::GetMarkerTransforms(ENDRoomMarkerType Type) const
{
	TArray<FTransform> Result;
	const FTransform LotToWorld = GetLotToWorld();
	for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
	{
		if (Marker.Type == Type)
		{
			Result.Add(Marker.LotTransform * LotToWorld);
		}
	}
	return Result;
}

ANDHouseLot* ANDHouseLot::FindLot(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ANDHouseLot> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
