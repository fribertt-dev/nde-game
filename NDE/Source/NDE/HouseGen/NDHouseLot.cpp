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
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HouseGen/NDHouseGenDebug.h"
#include "HouseGen/NDHouseFurniture.h"
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

	/** nde.HouseGen.ViewRoom <sala> [yaw]: pone al jugador en lo alto de una sala, mirando hacia abajo (capturas de los muebles). */
	void ViewRoom(const TArray<FString>& Args, UWorld* World)
	{
		const ANDHouseLot* Lot = ANDHouseLot::FindLot(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		int32 RoomIndex = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		// También por tipo de sala, p. ej. "Living" o "Kitchen".
		if (Lot && Args.Num() > 0 && !Args[0].IsNumeric())
		{
			RoomIndex = Lot->GetLayout().Rooms.IndexOfByPredicate([&Args](const FNDHouseLayoutRoom& Room)
			{
				return NDHouseGen::GetRoomTypeName(Room.Def->RoomType).Contains(Args[0]);
			});
		}
		if (!Lot || !Lot->IsHouseBuilt() || !Pawn || !Pawn->HasAuthority() || !Lot->GetLayout().Rooms.IsValidIndex(RoomIndex))
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("nde.HouseGen.ViewRoom: uso nde.HouseGen.ViewRoom <sala> [yaw] (en el servidor, con la casa construida)"));
			return;
		}
		const FNDHouseLayout& Layout = Lot->GetLayout();
		const float Yaw = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		// Con "pie" se queda a la altura del jugador, como lo ve en la partida.
		const bool bEyeLevel = Args.Num() > 2 && Args[2].Equals(TEXT("pie"), ESearchCase::IgnoreCase);
		const FRotator View(bEyeLevel ? 8.f : -40.f, Yaw, 0.f);
		const FVector Center = Lot->GetLotToWorld().TransformPosition(Layout.GetRoomCenter(RoomIndex));
		// Detrás del centro y cerca del techo, para ver la sala entera (o a ras de suelo).
		const FVector Location = Center - FRotator(0.f, Yaw, 0.f).Vector() * (NDHouseGen::CellSize * (bEyeLevel ? 0.35f : 0.45f))
			+ FVector(0.f, 0.f, bEyeLevel ? 60.f : NDHouseGen::WallHeight - 60.f);
		// Como fantasma (volando y sin colisión), para que los muebles y la escalera no lo empujen fuera de la vista.
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			Character->SetActorEnableCollision(false);
		}
		Pawn->SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
		PC->SetControlRotation(View);
		UE_LOG(LogNDHouseGen, Log, TEXT("nde.HouseGen.ViewRoom: sala %d %s (%s), piso %d"), RoomIndex, *NDHouseGen::GetRoomTypeName(Layout.Rooms[RoomIndex].Def->RoomType),
			*Layout.Rooms[RoomIndex].Def->RoomModuleId.ToString(), Layout.Rooms[RoomIndex].Floor);
	}

	/** nde.HouseGen.ViewOutside [distancia en celdas]: el jugador en el camino, delante de la puerta de la calle, mirando la casa. */
	void ViewOutside(const TArray<FString>& Args, UWorld* World)
	{
		const ANDHouseLot* Lot = ANDHouseLot::FindLot(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const FNDHouseLayout* Layout = Lot && Lot->IsHouseBuilt() ? &Lot->GetLayout() : nullptr;
		if (!Layout || !Pawn || !Pawn->HasAuthority() || !Layout->Rooms.IsValidIndex(Layout->EntryRoom) || !Layout->Rooms[Layout->EntryRoom].Sockets.IsValidIndex(Layout->EntryExitSocket))
		{
			UE_LOG(LogNDHouseGen, Warning, TEXT("nde.HouseGen.ViewOutside: hace falta la casa construida (en el servidor)"));
			return;
		}
		const FNDHouseLayoutSocket& Exit = Layout->Rooms[Layout->EntryRoom].Sockets[Layout->EntryExitSocket];
		const FIntPoint Out = NDHouseGen::DirectionToOffset(Exit.LotDirection);
		const float Distance = (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.5f) * NDHouseGen::CellSize;
		const FVector Door = Layout->GetSocketLocation(Layout->EntryRoom, Layout->EntryExitSocket);
		const FVector LotLocation = Door + FVector(Out.X, Out.Y, 0.f) * Distance + FVector(0.f, 0.f, 60.f);
		const FVector WorldLocation = Lot->GetLotToWorld().TransformPosition(LotLocation);
		const FRotator Look = (Lot->GetLotToWorld().TransformPosition(Door + FVector(0.f, 0.f, 300.f)) - WorldLocation).Rotation();
		Pawn->TeleportTo(WorldLocation, FRotator(0.f, Look.Yaw, 0.f));
		PC->SetControlRotation(Look);
	}

	FAutoConsoleCommandWithWorldAndArgs ViewOutsideCommand(TEXT("nde.HouseGen.ViewOutside"),
		TEXT("Solo servidor: pone al jugador fuera, delante de la puerta de la calle, mirando la casa. Uso: nde.HouseGen.ViewOutside [distancia en celdas]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ViewOutside));

	FAutoConsoleCommandWithWorld CheckNavCommand(TEXT("nde.HouseGen.CheckNav"),
		TEXT("Solo servidor: repite la comprobación de la patrulla de la criatura sobre el NavMesh."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ANDHouseLot* Lot = ANDHouseLot::FindLot(World))
			{
				Lot->RecheckNavigation();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ViewRoomCommand(TEXT("nde.HouseGen.ViewRoom"),
		TEXT("Solo servidor: pone al jugador en lo alto de una sala mirando hacia abajo, para ver sus muebles (queda como fantasma: volando y sin colisión). Con pie, a la altura del jugador. Uso: nde.HouseGen.ViewRoom <sala o tipo, p. ej. 3 o Kitchen> [yaw] [pie]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ViewRoom));

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

	DecorationFallbackMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DecorationFallback"));
	DecorationFallbackMeshes->SetupAttachment(Root);
	DecorationFallbackMeshes->SetStaticMesh(CubeFinder.Object);
	DecorationFallbackMeshes->SetMobility(EComponentMobility::Movable);
	DecorationFallbackMeshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DecorationFallbackMeshes->SetCanEverAffectNavigation(false);

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
	NDHouseLot::SetColor(DecorationFallbackMeshes, BaseMaterial, this, FLinearColor(0.45f, 0.3f, 0.18f));
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
	DecorationFallbackMeshes->ClearInstances();
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Art : DecorationArtMeshes)
	{
		if (Art.Value)
		{
			Art.Value->ClearInstances();
		}
	}
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
		// Mismo nombre en todas las máquinas: así el servidor puede referirse a esta sala (p. ej. como suelo
		// sobre el que está un jugador) y el cliente la encuentra. La revisión evita choques al regenerar.
		SpawnParams.Name = FName(*FString::Printf(TEXT("NDRoom_%s_r%d_%02d"), *LotInfo.LotId.ToString(), Manifest.Revision, RoomIndex));
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
		// Paredes exteriores (sin sala al otro lado en el mismo piso): fachada lisa, fachada con ventana o ventana
		// real, elegidas con la semilla. En el sótano, ventanucos.
		TArray<uint8> WallStyles;
		WallStyles.SetNumZeroed(Cells.Num() * 4);
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			for (int32 LocalDir = 0; LocalDir < 4; ++LocalDir)
			{
				const ENDCellDirection LotDir = NDHouseGen::RotateDirection(static_cast<ENDCellDirection>(LocalDir), Room.YawSteps);
				const FIntPoint Outside = Cells[CellIndex] + NDHouseGen::DirectionToOffset(LotDir);
				if (!Occupancy.Contains(FIntVector(Outside.X, Outside.Y, Room.Floor)))
				{
					const uint32 Hash = HashCombineFast(GetTypeHash(Manifest.RunSeed), HashCombineFast(GetTypeHash(Cells[CellIndex]), GetTypeHash(static_cast<int32>(LotDir) + Room.Floor * 4)));
					const uint32 Pick = Hash % 10;
					// Alero de tejado sobre las paredes exteriores sin piso encima; frontón sobre la entrada, hacia la calle.
					if (bBuildCeiling && CeilingPerCell[CellIndex] && Room.Floor >= NDHouseGen::GroundFloor)
					{
						const FIntPoint OutStep = NDHouseGen::DirectionToOffset(LotDir);
						const FVector Normal(OutStep.X, OutStep.Y, 0.f);
						const FVector Tangent(-Normal.Y, Normal.X, 0.f);
						const FRotator RoofYaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)), 0.f);
						const FVector EdgeStart = Layout.Lot.GetCellCenter(Cells[CellIndex]) + Normal * (NDHouseGen::CellSize * 0.5f) - Tangent * (NDHouseGen::CellSize * 0.5f)
							+ FVector(0.f, 0.f, Room.Floor * NDHouseGen::FloorHeight + NDHouseGen::FloorHeight);
						const bool bFront = LotDir == Layout.Lot.FrontDirection && RoomIndex == Layout.EntryRoom;
						for (float Along = 0.f; Along < NDHouseGen::CellSize - 1.f; Along += NDHouseGen::ArtTileSize)
						{
							AddLotArt(FString::Printf(TEXT("/Game/_NDE_/Props/Structure/%s"), bFront ? TEXT("SM_Roof_Gable_400/SM_Roof_Gable_400.SM_Roof_Gable_400") : TEXT("SM_Roof_Edge_400/SM_Roof_Edge_400.SM_Roof_Edge_400")),
								FTransform(RoofYaw, EdgeStart + Tangent * Along));
						}
					}
					WallStyles[CellIndex * 4 + LocalDir] = static_cast<uint8>(Room.Floor < NDHouseGen::GroundFloor ? ENDWallStyle::Basement : Pick < 6 ? ENDWallStyle::Window : ENDWallStyle::Facade);
				}
			}
		}
		RoomActor->RoomInstanceId = RoomIndex;
		RoomActor->BuildGreybox(*Room.Def, Openings, bBuildCeiling, bShowMarkers, bLightRooms, &CeilingPerCell, &OpeningOffsets, &ExtraOpenings, &WallStyles);
		// Sin acoplar al lote: la sala es estática (no se mueve) y el lote no; el lote la borra por su lista.
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

	// Muebles: caja greybox con colisión (el cubo del motor mide 100 cm y se apoya en el suelo) y, encima, su
	// malla de arte; con arte la caja no se ve. nde.HouseGen.Furniture 0 los quita (depuración).
	const bool bArt = NDHouseArt::IsEnabled();
	DecorationMeshes->SetVisibility(!bArt);
	for (const FNDHouseDecoration& Decoration : NDHouseLot::CVarFurniture.GetValueOnGameThread() != 0 ? Layout.Decorations : TArray<FNDHouseDecoration>())
	{
		if (Layout.Rooms[Decoration.Room].Floor > MaxVisibleFloor)
		{
			continue;
		}
		const FTransform& Base = Decoration.LotTransform;
		const FTransform Box(Base.GetRotation(), Base.GetLocation() + FVector(0.f, 0.f, Decoration.HalfExtent.Z), Decoration.HalfExtent * 2.f / 100.f);
		const TArray<FNDFurnitureArt>& ArtList = NDHouseFurniture::GetArt();
		const FNDFurnitureArt* Art = ArtList.IsValidIndex(Decoration.Art) ? &ArtList[Decoration.Art] : nullptr;
		UStaticMesh* Mesh = bArt && Art ? NDHouseArt::LoadMesh(Art->MeshPath) : nullptr;
		if (Mesh)
		{
			TObjectPtr<UInstancedStaticMeshComponent>& Meshes = DecorationArtMeshes.FindOrAdd(Mesh);
			if (!Meshes)
			{
				Meshes = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
				Meshes->SetupAttachment(Root);
				Meshes->SetStaticMesh(Mesh);
				Meshes->SetMobility(EComponentMobility::Movable);
				Meshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				Meshes->SetCanEverAffectNavigation(false);
				Meshes->RegisterComponent();
			}
			Meshes->AddInstance(FTransform(FRotator(0.f, Art->MeshYaw, 0.f), FVector::ZeroVector, Art->MeshScale) * Base);
		}
		if (Decoration.bCollision)
		{
			DecorationMeshes->AddInstance(Box);
			if (bArt && !Mesh)
			{
				DecorationFallbackMeshes->AddInstance(Box);
			}
		}
	}

	BuildExterior(UsedCells);
	BuildHazardArt();
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
				// Escaleras: dónde se corta el camino (centro de cada tramo y rellano).
				const FNDHouseLayoutRoom& StuckRoom = Layout.Rooms[Node.Room];
				const FTransform RoomToWorld = FTransform(FRotator(0.f, StuckRoom.YawSteps * 90.f, 0.f), Layout.GetRoomCenter(Node.Room)) * LotToWorld;
				for (int32 SegmentIndex = 0; SegmentIndex < StuckRoom.Def->StairSegments.Num() * 3; ++SegmentIndex)
				{
					const FNDStairSegment& Segment = StuckRoom.Def->StairSegments[SegmentIndex / 3];
					const float Along = 0.1f + 0.4f * (SegmentIndex % 3);
					const FVector Probe = RoomToWorld.TransformPosition(FMath::Lerp(Segment.Start, Segment.End, Along) + FVector(0.f, 0.f, Segment.Height + 50.f));
					FNavLocation ProbeOnNav;
					const bool bProbeOnNav = NavSys->ProjectPointToNavigation(Probe, ProbeOnNav, Extent);
					UNavigationPath* ProbePath = bProbeOnNav ? UNavigationSystemV1::FindPathToLocationSynchronously(this, BedroomOnNav.Location, ProbeOnNav.Location) : nullptr;
					FHitResult Above;
					const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Above, Probe, Probe + FVector(0.f, 0.f, 250.f), ECC_Pawn);
					UE_LOG(LogNDHouseGen, Warning, TEXT("    encima: %s"), bBlocked ? *FString::Printf(TEXT("%s / %s a %.0f cm"), *GetNameSafe(Above.GetActor()), *GetNameSafe(Above.GetComponent()), Above.Distance) : TEXT("libre"));
					UE_LOG(LogNDHouseGen, Warning, TEXT("  tramo %d (%.0f%%) de %s: %s, %s (en %s)"), SegmentIndex / 3, Along * 100.f, *StuckRoom.Def->RoomModuleId.ToString(),
						bProbeOnNav ? TEXT("en el NavMesh") : TEXT("FUERA del NavMesh"),
						ProbePath && ProbePath->IsValid() && !ProbePath->IsPartial() ? TEXT("con camino") : TEXT("SIN camino"), *Probe.ToCompactString());
				}
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

void ANDHouseLot::AddLotArt(const FString& MeshPath, const FTransform& Transform)
{
	UStaticMesh* Mesh = NDHouseArt::IsEnabled() ? NDHouseArt::LoadMesh(MeshPath) : nullptr;
	if (!Mesh)
	{
		static TSet<FString> Reported;
		if (NDHouseArt::IsEnabled() && !Reported.Contains(MeshPath))
		{
			Reported.Add(MeshPath);
			UE_LOG(LogNDHouseGen, Warning, TEXT("Lote %s: no se pudo cargar %s"), *LotInfo.LotId.ToString(), *MeshPath);
		}
		return;
	}
	TObjectPtr<UInstancedStaticMeshComponent>& Meshes = DecorationArtMeshes.FindOrAdd(Mesh);
	if (!Meshes)
	{
		Meshes = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Meshes->SetupAttachment(Root);
		Meshes->SetStaticMesh(Mesh);
		Meshes->SetMobility(EComponentMobility::Movable);
		// Solo se ve: la colisión es la del greybox.
		Meshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Meshes->SetCanEverAffectNavigation(false);
		Meshes->RegisterComponent();
	}
	Meshes->AddInstance(Transform);
}

void ANDHouseLot::BuildExterior(const TSet<FIntPoint>& UsedCells)
{
	// Exterior: suelo greybox con colisión (oculto con arte) y, encima, el arte. Césped en el lote, camino de
	// piedra de la puerta a la calle, cerca con portón, arbustos junto a la casa y, fuera del lote, césped
	// alrededor y delante acera, bordillo, calzada y farolas.
	GroundMeshes->SetVisibility(!NDHouseArt::IsEnabled());
	const float Cell = NDHouseGen::CellSize;
	const float Tile = NDHouseGen::ArtTileSize;
	const int32 Tiles = FMath::Max(1, FMath::RoundToInt(Cell / Tile));
	auto PropPath = [](const TCHAR* Folder, const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/_NDE_/Props/%s/SM_%s/SM_%s.SM_%s"), Folder, Name, Name, Name);
	};
	// Celda de suelo: caja con colisión y baldosas de arte de 400 (pivote en una esquina, de 0 a 400 en X y de 0 a -400 en Y).
	auto AddGroundCell = [&](const FIntPoint& LotCell, const TCHAR* Folder, const TCHAR* Name, const FVector& TileScale)
	{
		const FVector Center = Layout.Lot.GetCellCenter(LotCell);
		GroundMeshes->AddInstance(FTransform(FRotator::ZeroRotator, Center - FVector(0.f, 0.f, NDHouseGen::WallThickness * 0.5f),
			FVector(Cell, Cell, NDHouseGen::WallThickness) / 100.f));
		for (int32 X = 0; X < Tiles; ++X)
		{
			for (int32 Y = 0; Y < Tiles; ++Y)
			{
				AddLotArt(PropPath(Folder, Name), FTransform(FRotator::ZeroRotator, Center + FVector(-Cell * 0.5f + X * Tile, Cell * 0.5f - Y * Tile, 0.f), TileScale));
			}
		}
	};
	auto CellHash = [this](const FIntPoint& LotCell, uint32 Salt)
	{
		return HashCombineFast(GetTypeHash(Manifest.RunSeed), HashCombineFast(GetTypeHash(LotCell), Salt));
	};

	// Camino: de la puerta de la casa hacia fuera, hasta salir del lote.
	TSet<FIntPoint> PathCells;
	FIntPoint GateCell(INDEX_NONE, INDEX_NONE);
	ENDCellDirection PathDirection = Layout.Lot.FrontDirection;
	if (Layout.Rooms.IsValidIndex(Layout.EntryRoom) && Layout.Rooms[Layout.EntryRoom].Sockets.IsValidIndex(Layout.EntryExitSocket))
	{
		const FNDHouseLayoutSocket& Exit = Layout.Rooms[Layout.EntryRoom].Sockets[Layout.EntryExitSocket];
		PathDirection = Exit.LotDirection;
		for (FIntPoint PathCell = Exit.GetOutsideCell(); Layout.Lot.IsInsideLot(PathCell); PathCell += NDHouseGen::DirectionToOffset(PathDirection))
		{
			PathCells.Add(PathCell);
			GateCell = PathCell;
		}
	}

	for (int32 X = 0; X < Layout.Lot.SizeCells.X; ++X)
	{
		for (int32 Y = 0; Y < Layout.Lot.SizeCells.Y; ++Y)
		{
			const FIntPoint LotCell(X, Y);
			if (UsedCells.Contains(LotCell))
			{
				continue;
			}
			const bool bPath = PathCells.Contains(LotCell);
			AddGroundCell(LotCell, TEXT("Exterior"), bPath ? TEXT("Path_Stone") : TEXT("Lot_Ground_400"), FVector::OneVector);
			// Arbustos pegados a la casa en algunas celdas de césped (fuera del jardín delantero, donde va la camioneta).
			const uint32 Hash = CellHash(LotCell, 1);
			if (bPath || Layout.Lot.IsFrontYard(LotCell) || Hash % 100 >= 45)
			{
				continue;
			}
			for (int32 Dir = 0; Dir < 4; ++Dir)
			{
				const FIntPoint Toward = NDHouseGen::DirectionToOffset(static_cast<ENDCellDirection>(Dir));
				if (UsedCells.Contains(LotCell + Toward))
				{
					const FVector Spot = Layout.Lot.GetCellCenter(LotCell) + FVector(Toward.X, Toward.Y, 0.f) * (Cell * 0.5f - 150.f)
						+ FVector(-Toward.Y, Toward.X, 0.f) * ((static_cast<int32>(Hash / 100 % 5) - 2) * 80.f);
					AddLotArt(PropPath(TEXT("Exterior"), Hash % 2 ? TEXT("Bush_A") : TEXT("Bush_B")), FTransform(FRotator(0.f, (Hash / 7 % 4) * 90.f, 0.f), Spot));
					break;
				}
			}
		}
	}

	// Cerca por el borde del lote (piezas de 400, de 0 a 400 en X), con portón donde sale el camino.
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		const ENDCellDirection Side = static_cast<ENDCellDirection>(Dir);
		const FIntPoint Out = NDHouseGen::DirectionToOffset(Side);
		const FVector Normal(Out.X, Out.Y, 0.f);
		const FVector Tangent(-Out.Y, Out.X, 0.f);
		const FRotator Yaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)), 0.f);
		for (int32 X = 0; X < Layout.Lot.SizeCells.X; ++X)
		{
			for (int32 Y = 0; Y < Layout.Lot.SizeCells.Y; ++Y)
			{
				const FIntPoint LotCell(X, Y);
				if (Layout.Lot.IsInsideLot(LotCell + Out) || UsedCells.Contains(LotCell))
				{
					continue;
				}
				const FVector EdgeStart = Layout.Lot.GetCellCenter(LotCell) + Normal * (Cell * 0.5f - 10.f) - Tangent * (Cell * 0.5f);
				const bool bGate = LotCell == GateCell && Side == PathDirection;
				for (int32 Piece = 0; Piece < Tiles; ++Piece)
				{
					const FVector PieceStart = EdgeStart + Tangent * (Piece * Tile);
					if (bGate)
					{
						// Hueco de 500 en el centro: un tramo corto a cada lado y el portón abierto hacia fuera.
						const bool bFirst = Piece == 0;
						AddLotArt(PropPath(TEXT("Exterior"), TEXT("Fence_Picket_400")), FTransform(Yaw, bFirst ? PieceStart : PieceStart + Tangent * (Tile - 150.f), FVector(150.f / Tile, 1.f, 1.f)));
						if (bFirst)
						{
							AddLotArt(PropPath(TEXT("Exterior"), TEXT("Fence_Gate")), FTransform(Yaw + FRotator(0.f, -100.f, 0.f), PieceStart + Tangent * 150.f));
						}
						continue;
					}
					AddLotArt(PropPath(TEXT("Exterior"), TEXT("Fence_Picket_400")), FTransform(Yaw, PieceStart));
				}
			}
		}
	}

	// Fuera del lote, un anillo de celdas: delante acera, calzada (dos filas) y acera; en los demás lados, césped.
	constexpr int32 Ring = 4;
	const FIntPoint FrontStep = NDHouseGen::DirectionToOffset(Layout.Lot.FrontDirection);
	for (int32 X = -Ring; X < Layout.Lot.SizeCells.X + Ring; ++X)
	{
		for (int32 Y = -Ring; Y < Layout.Lot.SizeCells.Y + Ring; ++Y)
		{
			const FIntPoint LotCell(X, Y);
			if (Layout.Lot.IsInsideLot(LotCell))
			{
				continue;
			}
			// Filas hacia la calle desde el borde delantero del lote: 1 acera, 2 y 3 calzada, 4 acera de enfrente.
			int32 Row = 0;
			switch (Layout.Lot.FrontDirection)
			{
			case ENDCellDirection::North: Row = X - Layout.Lot.SizeCells.X + 1; break;
			case ENDCellDirection::East: Row = Y - Layout.Lot.SizeCells.Y + 1; break;
			case ENDCellDirection::South: Row = -X; break;
			default: Row = -Y; break;
			}
			const FVector Center = Layout.Lot.GetCellCenter(LotCell);
			if (Row == 2 || Row == 3)
			{
				AddGroundCell(LotCell, TEXT("Street"), TEXT("Road_400"), FVector::OneVector);
				continue;
			}
			if (Row != 1 && Row != 4)
			{
				AddGroundCell(LotCell, TEXT("Exterior"), TEXT("Lot_Ground_400"), FVector::OneVector);
				continue;
			}
			// Acera (400 x 150, estirada a la celda) y bordillo del lado de la calzada.
			AddGroundCell(LotCell, TEXT("Street"), TEXT("Sidewalk_400"), FVector(1.f, Tile / 150.f, 1.f));
			const FVector ToRoad = FVector(FrontStep.X, FrontStep.Y, 0.f) * (Row == 1 ? 1.f : -1.f);
			const FVector Along(-ToRoad.Y, ToRoad.X, 0.f);
			const FRotator CurbYaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f);
			for (int32 Piece = 0; Piece < Tiles; ++Piece)
			{
				AddLotArt(PropPath(TEXT("Street"), TEXT("Curb_400")), FTransform(CurbYaw, Center + ToRoad * (Cell * 0.5f - 33.f) - Along * (Cell * 0.5f) + Along * (Piece * Tile)));
			}
			if (Row == 1 && (X + Y) % 3 == 0)
			{
				AddLotArt(PropPath(TEXT("Street"), TEXT("StreetLamp")), FTransform(CurbYaw, Center + ToRoad * (Cell * 0.5f - 150.f)));
			}
		}
	}
}

void ANDHouseLot::BuildHazardArt()
{
	// Peligros con arte (solo se ven): tabla que cruje en el suelo, fila de botellas frágiles y juguete que chilla.
	auto PropPath = [](const TCHAR* Folder, const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/_NDE_/Props/%s/SM_%s/SM_%s.SM_%s"), Folder, Name, Name, Name);
	};
	const float Cell = NDHouseGen::CellSize;
	const float Tile = NDHouseGen::ArtTileSize;
	for (const FNDHouseHazard& Hazard : Layout.Hazards)
	{
		if (!Layout.Rooms.IsValidIndex(Hazard.Room) || Layout.Rooms[Hazard.Room].Floor > MaxVisibleFloor)
		{
			continue;
		}
		const FVector Location = Hazard.LotTransform.GetLocation();
		switch (Hazard.Type)
		{
		case ENDHouseHazardType::CreakyFloor:
		{
			// La baldosa de 400 que contiene el peligro, un poco por encima del suelo de la sala.
			const FIntPoint LotCell(FMath::FloorToInt(Location.X / Cell + Layout.Lot.SizeCells.X * 0.5f), FMath::FloorToInt(Location.Y / Cell + Layout.Lot.SizeCells.Y * 0.5f));
			const FVector CellMin = Layout.Lot.GetCellCenter(LotCell) - FVector(Cell * 0.5f, Cell * 0.5f, 0.f);
			const float TileX = CellMin.X + FMath::FloorToFloat((Location.X - CellMin.X) / Tile) * Tile;
			const float TileY = CellMin.Y + FMath::FloorToFloat((Location.Y - CellMin.Y) / Tile) * Tile;
			AddLotArt(PropPath(TEXT("Structure"), TEXT("Floor_Creaky_400")), FTransform(FVector(TileX, TileY + Tile, Location.Z + 0.5f)));
			break;
		}
		case ENDHouseHazardType::FragileItem:
			AddLotArt(PropPath(TEXT("Hazards"), TEXT("Bottles_Row")), Hazard.LotTransform);
			break;
		default:
			AddLotArt(PropPath(TEXT("Hazards"), TEXT("Toy_Squeaky")), Hazard.LotTransform);
			break;
		}
	}

	// Herramientas de la banda junto a la zona de depósito de la camioneta.
	static const TCHAR* Tools[] = { TEXT("Tool_FoldingCart"), TEXT("Tool_Flashlight"), TEXT("Tool_Lockpick_A"), TEXT("Tool_Lockpick_B"), TEXT("Tool_Lockpick_C"),
		TEXT("Tool_Lockpick_D"), TEXT("Tool_SilentFoam"), TEXT("Tool_SnoreMeter"), TEXT("Tool_WindupMouse") };
	for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
	{
		if (Marker.Type != ENDRoomMarkerType::Extraction)
		{
			continue;
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tools); ++Index)
		{
			const float Angle = FMath::DegreesToRadians(200.f + Index * 18.f);
			const FVector Offset(FMath::Cos(Angle) * 180.f, FMath::Sin(Angle) * 180.f, 0.f);
			AddLotArt(PropPath(TEXT("Tools"), Tools[Index]), FTransform(FRotator(0.f, Index * 40.f, 0.f), Marker.LotTransform.TransformPosition(Offset)));
		}
		break;
	}
}
