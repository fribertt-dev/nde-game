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
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
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

	TAutoConsoleVariable<int32> CVarFurniture(
		TEXT("nde.HouseGen.Furniture"),
		1,
		TEXT("Construye los muebles greybox de la casa: 1 = si, 0 = no (depuracion; los puntos de juego no cambian). Se aplica al construir o regenerar."));

	void DrawPointsNow(UWorld* World, float Duration)
	{
		const ANDHouseLot* Lot = ANDHouseLot::FindLot(World);
		if (!Lot || !Lot->IsHouseBuilt())
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("nde.HouseGen.DebugPoints: no hay casa construida"));
			return;
		}
		const FNDHouseLayout& Layout = Lot->GetLayout();
		NDHouseGenDebug::DrawGameplayPoints(World, Lot->GetLotToWorld(), Layout, Duration);
		UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: %d escondites, %d peligros, %d muebles, %d nodos de patrulla (recorrido de %d pasos), %d puertas en el grafo de sonido"),
			*Lot->LotInfo.LotId.ToString(), Layout.HidingSpots.Num(), Layout.Hazards.Num(), Layout.Decorations.Num(), Layout.PatrolNodes.Num(), Layout.PatrolRoute.Num(), Layout.AudioLinks.Num());
	}

	void DebugPoints(const TArray<FString>& Args, UWorld* World)
	{
		const float Duration = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.f;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		if (Delay <= 0.f)
		{
			DrawPointsNow(World, Duration);
			return;
		}
		// Con espera, por el ticker del motor: sirve en un cliente que se conecta después (cambia de mundo).
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Duration](float)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
				{
					DrawPointsNow(Context.World(), Duration);
				}
			}
			return false;
		}), Delay);
	}

	FAutoConsoleCommandWithWorldAndArgs DebugPointsCommand(
		TEXT("nde.HouseGen.DebugPoints"),
		TEXT("Dibuja escondites (azul), peligros (naranja), patrulla (rojo) y paso del sonido por puerta (cian). Uso: nde.HouseGen.DebugPoints [segundos] [espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugPoints));

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

	DecorationMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Decorations"));
	DecorationMeshes->SetupAttachment(Root);
	DecorationMeshes->SetStaticMesh(CubeFinder.Object);
	DecorationMeshes->SetMobility(EComponentMobility::Movable);
	DecorationMeshes->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

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
	NDHouseLot::SetColor(DecorationMeshes, BaseMaterial, this, FLinearColor(0.45f, 0.3f, 0.18f));
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
	Params.ApplyFloorOptions(UpperFloor, Basement);
	FNDHouseLotInfo Lot = LotInfo;
	Lot.LotTransform = GetActorTransform();
	Params.SetLot(Lot);
	Params.ContractRequirements = ContractRequirements;
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
	DecorationMeshes->ClearInstances();
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
	TMap<FIntVector, int32> Occupancy;
	Layout.GetOccupancy(Occupancy);
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
		TArray<FIntPoint> Cells;
		Layout.GetRoomCells(RoomIndex, Cells);
		const FTransform RoomTransform = FTransform(FRotator(0.f, Room.YawSteps * 90.f, 0.f), Layout.GetRoomCenter(RoomIndex)) * LotToWorld;
		ANDRoomModuleActor* RoomActor = World->SpawnActor<ANDRoomModuleActor>(ANDRoomModuleActor::StaticClass(), RoomTransform, SpawnParams);
		if (!RoomActor)
		{
			continue;
		}

		// Los huecos del layout van en el mismo orden que en la definición.
		TArray<float> Openings;
		TArray<float> OpeningOffsets;
		// Puertas "Auto" (no están en el módulo): celda, lado, ancho y desplazamiento del hueco.
		TArray<FNDRoomDoorSocket> ExtraOpenings;
		for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
		{
			const FNDHouseLayoutSocket& Socket = Room.Sockets[SocketIndex];
			// La unión vertical de una escalera no abre hueco en la pared.
			const bool bWallOpening = Socket.Type != ENDRoomSocketType::Stairs;
			Openings.Add(!bWallOpening ? 0.f : (Socket.Connection != INDEX_NONE ? Layout.Connections[Socket.Connection].OpeningWidth : (Socket.bExit ? Socket.Width : 0.f)));
			// Hueco desplazado (escaleras en U): la pared vecina abre el suyo en el mismo sitio. Las paredes
			// enfrentadas recorren el borde en sentidos opuestos, así que el desplazamiento cambia de signo.
			float HoleOffset = Socket.Offset;
			if (Socket.Connection != INDEX_NONE && FMath::IsNearlyZero(HoleOffset))
			{
				const FNDHouseLayoutConnection& Link = Layout.Connections[Socket.Connection];
				const bool bIsA = Link.RoomA == RoomIndex && Link.SocketA == SocketIndex;
				HoleOffset = -Layout.Rooms[bIsA ? Link.RoomB : Link.RoomA].Sockets[bIsA ? Link.SocketB : Link.SocketA].Offset;
			}
			OpeningOffsets.Add(HoleOffset);
			if (Socket.bAuto)
			{
				FNDRoomDoorSocket& Extra = ExtraOpenings.AddDefaulted_GetRef();
				Extra.SocketId = Socket.SocketId;
				Extra.Cell = Socket.LocalCell;
				Extra.Direction = Socket.LocalDirection;
				Extra.Width = Openings.Last();
				Extra.Offset = HoleOffset;
			}
		}
		// Techo solo donde no hay otra sala encima: allí el suelo de arriba hace de techo.
		TArray<bool> CeilingPerCell;
		for (const FIntPoint& Cell : Cells)
		{
			CeilingPerCell.Add(!Occupancy.Contains(FIntVector(Cell.X, Cell.Y, Room.Floor + 1)));
		}
		RoomActor->RoomInstanceId = RoomIndex;
		RoomActor->BuildGreybox(*Room.Def, Openings, bBuildCeiling, bShowMarkers, bLightRooms, &CeilingPerCell, &OpeningOffsets, &ExtraOpenings);
		RoomActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		// Depuración: oculta los pisos de encima de MaxVisibleFloor para ver los de abajo desde arriba.
		if (Room.Floor > MaxVisibleFloor)
		{
			RoomActor->SetActorHiddenInGame(true);
			RoomActor->SetActorEnableCollision(false);
		}
#if WITH_EDITOR
		RoomActor->SetActorLabel(FString::Printf(TEXT("Room%02d_F%d_%s"), RoomIndex, Room.Floor, *Room.Def->RoomModuleId.ToString()));
#endif
		RoomActors.Add(RoomActor);

		if (Room.Floor == NDHouseGen::GroundFloor)
		{
			UsedCells.Append(Cells);
		}
	}

	// Muebles greybox: el cubo del motor mide 100 cm y se apoya en el suelo. nde.HouseGen.Furniture 0 los quita (depuración).
	for (const FNDHouseDecoration& Decoration : NDHouseLot::CVarFurniture.GetValueOnGameThread() != 0 ? Layout.Decorations : TArray<FNDHouseDecoration>())
	{
		const FTransform& Base = Decoration.LotTransform;
		DecorationMeshes->AddInstance(FTransform(Base.GetRotation(), Base.GetLocation() + FVector(0.f, 0.f, Decoration.HalfExtent.Z),
			Decoration.HalfExtent * 2.f / 100.f));
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
	// Ruta de la criatura: de la Entrada al dormitorio y, si existen, a una sala del piso de arriba y del
	// sótano (comprueba que el NavMesh sube y baja por las rampas).
	auto FindRoomOnFloor = [this](int32 Floor)
	{
		return Layout.Rooms.IndexOfByPredicate([Floor](const FNDHouseLayoutRoom& R) { return R.Floor == Floor && R.Def->StairPart == ENDStairPart::None; });
	};
	const TPair<const TCHAR*, int32> Targets[] = {
		{ TEXT("Dormitorio"), Layout.BedroomRoom },
		{ TEXT("Piso superior"), FindRoomOnFloor(NDHouseGen::UpperFloor) },
		{ TEXT("Sotano"), FindRoomOnFloor(NDHouseGen::BasementFloor) },
	};
	const FVector Extent(150.f, 150.f, 300.f);
	FNavLocation StartOnNav;
	const bool bStartOnNav = NavSys->ProjectPointToNavigation(LotToWorld.TransformPosition(Layout.GetRoomCenter(Layout.EntryRoom)), StartOnNav, Extent);
	for (const TPair<const TCHAR*, int32>& Target : Targets)
	{
		if (!Layout.Rooms.IsValidIndex(Target.Value))
		{
			continue;
		}
		FNavLocation EndOnNav;
		const bool bProjected = bStartOnNav && NavSys->ProjectPointToNavigation(LotToWorld.TransformPosition(Layout.GetRoomCenter(Target.Value)), EndOnNav, Extent);
		UNavigationPath* Path = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(this, StartOnNav.Location, EndOnNav.Location) : nullptr;
		const bool bPathOk = Path && Path->IsValid() && !Path->IsPartial();
		if (Target.Value == Layout.BedroomRoom)
		{
			bNavPathVerified = bPathOk;
		}
		if (bPathOk)
		{
			UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: PathExists Entrada -> %s OK (%.0f cm)"), *LotInfo.LotId.ToString(), Target.Key, Path->GetPathLength());
		}
		else if (Target.Value != Layout.BedroomRoom && Layout.Rooms[Target.Value].Floor == NDHouseGen::BasementFloor && bProjected)
		{
			// El sótano puede quedar unido solo por debajo de la rampa: pasan los Desvelados, no la criatura.
			UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: Sotano sin camino para la criatura (solo se entra por debajo de la rampa)"), *LotInfo.LotId.ToString());
		}
		else
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("Lote %s: PathExists Entrada -> %s FALLA (%s)"), *LotInfo.LotId.ToString(), Target.Key,
				bProjected ? TEXT("sin camino completo") : TEXT("fuera del NavMesh; revisa que RecastNavMesh tenga Runtime Generation = Dynamic"));
		}
	}

	// Patrulla: desde el dormitorio a cada nodo. Un nodo sin camino dejaría a la criatura atascada.
	PatrolNodesStuck = 0;
	FNavLocation BedroomOnNav;
	if (Layout.PatrolNodes.IsValidIndex(Layout.BedroomRoom)
		&& NavSys->ProjectPointToNavigation(LotToWorld.TransformPosition(Layout.PatrolNodes[Layout.BedroomRoom].Location), BedroomOnNav, Extent))
	{
		for (const FNDHousePatrolNode& Node : Layout.PatrolNodes)
		{
			// Fuera del recorrido (salas con puerta pequeña): la criatura no va.
			if (Node.Room == Layout.BedroomRoom || !Layout.PatrolRoute.Contains(Node.StableId))
			{
				continue;
			}
			FNavLocation NodeOnNav;
			const bool bProjected = NavSys->ProjectPointToNavigation(LotToWorld.TransformPosition(Node.Location), NodeOnNav, Extent);
			UNavigationPath* Path = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(this, BedroomOnNav.Location, NodeOnNav.Location) : nullptr;
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				++PatrolNodesStuck;
				UE_LOG(LogNDHouseGen, Warning, TEXT("Lote %s: nodo de patrulla %d (sala %d %s, piso %d) sin camino desde el dormitorio%s (%s, %d muebles, vecinos: %s, en %s)"),
					*LotInfo.LotId.ToString(), Node.StableId, Node.Room, *NDHouseGen::GetRoomTypeName(Layout.Rooms[Node.Room].Def->RoomType),
					Layout.Rooms[Node.Room].Floor, bProjected ? TEXT("") : TEXT(" (fuera del NavMesh)"), *Layout.Rooms[Node.Room].Def->RoomModuleId.ToString(),
					Layout.Decorations.FilterByPredicate([&Node](const FNDHouseDecoration& D) { return D.Room == Node.Room; }).Num(),
					*FString::JoinBy(Node.Neighbors, TEXT(", "), [this](int32 Other) { return Layout.Rooms[Other].Def->RoomModuleId.ToString(); }),
					*LotToWorld.TransformPosition(Node.Location).ToCompactString());
			}
		}
		UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: patrulla %d nodos, %d sin camino; recorrido de %d pasos"),
			*LotInfo.LotId.ToString(), Layout.PatrolNodes.Num(), PatrolNodesStuck, Layout.PatrolRoute.Num());
	}
}

float ANDHouseLot::GetSoundTransmission(const FVector& WorldA, const FVector& WorldB) const
{
	if (!bHouseBuilt)
	{
		return 1.f;
	}
	const FTransform LotToWorld = GetLotToWorld();
	return Layout.GetSoundTransmission(LotToWorld.InverseTransformPosition(WorldA), LotToWorld.InverseTransformPosition(WorldB));
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
