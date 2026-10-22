// No Despiertes Eso

#include "HouseGen/NDRoomModuleActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "HouseGen/NDRoomCatalog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace NDRoomModuleActor
{
	const FLinearColor WallColor(0.72f, 0.72f, 0.70f);
	const FLinearColor CeilingColor(0.55f, 0.55f, 0.58f);

	FLinearColor GetMarkerColor(ENDRoomMarkerType Type)
	{
		switch (Type)
		{
		case ENDRoomMarkerType::LootSocket: return FLinearColor(1.f, 0.8f, 0.05f);
		case ENDRoomMarkerType::HideSpot: return FLinearColor(0.1f, 0.35f, 1.f);
		case ENDRoomMarkerType::MonsterAnchor: return FLinearColor(0.9f, 0.05f, 0.05f);
		case ENDRoomMarkerType::Extraction: return FLinearColor(0.1f, 0.9f, 0.2f);
		default: return FLinearColor::White;
		}
	}

	UInstancedStaticMeshComponent* CreateMeshes(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Cube, bool bCollision)
	{
		UInstancedStaticMeshComponent* Meshes = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Meshes->SetupAttachment(Parent);
		Meshes->SetStaticMesh(Cube);
		Meshes->SetMobility(EComponentMobility::Movable);
		Meshes->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Meshes->SetCanEverAffectNavigation(bCollision);
		return Meshes;
	}
}

ANDRoomModuleActor::ANDRoomModuleActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// Cada máquina construye su copia desde el manifiesto replicado.
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	FloorMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Floor"), Root, CubeFinder.Object, true);
	WallMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Walls"), Root, CubeFinder.Object, true);
	CeilingMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Ceiling"), Root, CubeFinder.Object, true);
}

void ANDRoomModuleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Previsualización al colocarlo a mano; la casa generada llama a BuildGreybox ella misma.
	const FNDRoomModuleDefinition* Definition = RoomInstanceId == INDEX_NONE ? FNDRoomCatalog::Find(PreviewModuleId) : nullptr;
	if (Definition)
	{
		TArray<float> Openings;
		for (const FNDRoomDoorSocket& Socket : Definition->DoorSockets)
		{
			Openings.Add(bPreviewOpenSockets ? Socket.Width : 0.f);
		}
		BuildGreybox(*Definition, Openings, false, true);
	}
}

void ANDRoomModuleActor::ClearGreybox()
{
	FloorMeshes->ClearInstances();
	WallMeshes->ClearInstances();
	CeilingMeshes->ClearInstances();
	for (UInstancedStaticMeshComponent* Meshes : MarkerMeshes)
	{
		if (Meshes)
		{
			Meshes->ClearInstances();
		}
	}
}

void ANDRoomModuleActor::AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	// El cubo del motor mide 100 cm y tiene el pivote en el centro.
	Target->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

void ANDRoomModuleActor::SetColor(UInstancedStaticMeshComponent* Target, const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return;
	}
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Target->GetMaterial(0));
	if (!Material || Material->Parent != BaseMaterial)
	{
		Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Target->SetMaterial(0, Material);
	}
	Material->SetVectorParameterValue(TEXT("Color"), Color);
}

UInstancedStaticMeshComponent* ANDRoomModuleActor::GetMarkerMeshes(ENDRoomMarkerType Type)
{
	const int32 Index = static_cast<int32>(Type);
	if (MarkerMeshes.Num() <= Index)
	{
		MarkerMeshes.SetNum(Index + 1);
	}
	if (!MarkerMeshes[Index])
	{
		UInstancedStaticMeshComponent* Meshes = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Meshes->SetupAttachment(Root);
		Meshes->SetStaticMesh(FloorMeshes->GetStaticMesh());
		Meshes->SetMobility(EComponentMobility::Movable);
		Meshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Meshes->SetCanEverAffectNavigation(false);
		Meshes->RegisterComponent();
		SetColor(Meshes, NDRoomModuleActor::GetMarkerColor(Type));
		MarkerMeshes[Index] = Meshes;
	}
	return MarkerMeshes[Index];
}

void ANDRoomModuleActor::BuildEdge(const FVector& EdgeCenter, ENDCellDirection Direction, float OpeningWidth)
{
	using namespace NDHouseGen;
	const FIntPoint Offset = DirectionToOffset(Direction);
	const FVector Normal(Offset.X, Offset.Y, 0.f);
	const FVector Tangent(-Normal.Y, Normal.X, 0.f);
	// La pared queda dentro de la sala; la vecina pone la suya al otro lado.
	const FVector WallCenter = EdgeCenter - Normal * (WallThickness * 0.5f);
	auto MakeSize = [&Normal, &Tangent](float Length, float Height)
	{
		return FVector(
			FMath::Abs(Normal.X) * WallThickness + FMath::Abs(Tangent.X) * Length,
			FMath::Abs(Normal.Y) * WallThickness + FMath::Abs(Tangent.Y) * Length,
			Height);
	};

	if (OpeningWidth <= 0.f)
	{
		AddBox(WallMeshes, WallCenter + FVector(0.f, 0.f, WallHeight * 0.5f), MakeSize(CellSize, WallHeight));
		return;
	}

	const float Door = FMath::Min(OpeningWidth, CellSize - 2.f * WallThickness);
	const float Side = (CellSize - Door) * 0.5f;
	for (const float Sign : { -1.f, 1.f })
	{
		AddBox(WallMeshes, WallCenter + Tangent * (Sign * (Door + Side) * 0.5f) + FVector(0.f, 0.f, WallHeight * 0.5f), MakeSize(Side, WallHeight));
	}
	const float LintelHeight = WallHeight - DoorHeight;
	AddBox(WallMeshes, WallCenter + FVector(0.f, 0.f, DoorHeight + LintelHeight * 0.5f), MakeSize(Door, LintelHeight));
}

void ANDRoomModuleActor::BuildGreybox(const FNDRoomModuleDefinition& Definition, const TArray<float>& OpeningWidths, bool bBuildCeiling, bool bShowMarkers,
	bool bAddRoomLight, const TArray<bool>* CeilingPerCell)
{
	using namespace NDHouseGen;
	ClearGreybox();

	const FIntPoint Size = Definition.FootprintCells;
	const FVector Extent(Size.X * CellSize, Size.Y * CellSize, 0.f);
	SetColor(FloorMeshes, FLinearColor(GetRoomTypeColor(Definition.RoomType)) * 0.6f);
	SetColor(WallMeshes, NDRoomModuleActor::WallColor);
	SetColor(CeilingMeshes, NDRoomModuleActor::CeilingColor);

	// Por celda (las salas en L no llenan su rectángulo): suelo con la cara de arriba en Z = 0,
	// techo opcional y paredes en cada borde que no da a otra celda de la sala.
	TArray<FIntPoint> Cells;
	Definition.GetLocalCells(Cells);
	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const FIntPoint& Cell = Cells[CellIndex];
		const FVector CellCenter((Cell.X + 0.5f) * CellSize - Extent.X * 0.5f, (Cell.Y + 0.5f) * CellSize - Extent.Y * 0.5f, 0.f);
		// La llegada de una escalera es un hueco en el forjado: no lleva suelo.
		if (Definition.StairPart != ENDStairPart::Top)
		{
			AddBox(FloorMeshes, CellCenter - FVector(0.f, 0.f, WallThickness * 0.5f), FVector(CellSize, CellSize, WallThickness));
		}
		const bool bCeilingHere = !CeilingPerCell || !CeilingPerCell->IsValidIndex(CellIndex) || (*CeilingPerCell)[CellIndex];
		if (bBuildCeiling && bCeilingHere)
		{
			AddBox(CeilingMeshes, CellCenter + FVector(0.f, 0.f, WallHeight + WallThickness * 0.5f), FVector(CellSize, CellSize, WallThickness));
		}

		for (int32 DirIndex = 0; DirIndex < 4; ++DirIndex)
		{
			const ENDCellDirection Direction = static_cast<ENDCellDirection>(DirIndex);
			if (Definition.ContainsCell(Cell + DirectionToOffset(Direction)))
			{
				continue;
			}
			// Con hueco si la casa abrió ese socket.
			const int32 SocketIndex = Definition.DoorSockets.IndexOfByPredicate([&Cell, Direction](const FNDRoomDoorSocket& S)
			{
				return S.Cell == Cell && S.Direction == Direction;
			});
			const float Opening = OpeningWidths.IsValidIndex(SocketIndex) ? OpeningWidths[SocketIndex] : 0.f;
			const FIntPoint Offset = DirectionToOffset(Direction);
			BuildEdge(CellCenter + FVector(Offset.X, Offset.Y, 0.f) * (CellSize * 0.5f), Direction, Opening);
		}
	}

	// Arranque de escalera: rampa a lo largo de X que sube un piso entero, de la puerta S0 (X mínima)
	// a la llegada del piso de arriba (X máxima). Ancho = celda menos paredes.
	if (Definition.StairPart == ENDStairPart::Bottom)
	{
		const float Run = Extent.X;
		const float Rise = FloorHeight;
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
		const FVector Up(-FMath::Sin(FMath::DegreesToRadians(Angle)), 0.f, FMath::Cos(FMath::DegreesToRadians(Angle)));
		// La cara de arriba pasa por (-Run/2, 0) y (Run/2, Rise).
		const FVector Center = FVector(0.f, 0.f, Rise * 0.5f) - Up * (WallThickness * 0.5f);
		AddBox(FloorMeshes, Center, FVector(FMath::Sqrt(Run * Run + Rise * Rise), Extent.Y - 2.f * WallThickness, WallThickness), FRotator(Angle, 0.f, 0.f));
	}

	if (bAddRoomLight)
	{
		if (!RoomLight)
		{
			RoomLight = NewObject<UPointLightComponent>(this, TEXT("RoomLight"), RF_Transient);
			RoomLight->SetupAttachment(Root);
			RoomLight->SetMobility(EComponentMobility::Movable);
			RoomLight->SetCastShadows(false);
			RoomLight->SetIntensityUnits(ELightUnits::Candelas);
			RoomLight->SetLightColor(FLinearColor(1.f, 0.85f, 0.68f));
			RoomLight->RegisterComponent();
		}
		RoomLight->SetRelativeLocation(FVector(0.f, 0.f, WallHeight - 60.f));
		RoomLight->SetIntensity(8.f * FMath::Max(Size.X, Size.Y));
		RoomLight->SetAttenuationRadius(FMath::Max(Extent.X, Extent.Y) * 0.9f);
	}
	if (RoomLight)
	{
		RoomLight->SetVisibility(bAddRoomLight);
	}

	if (bShowMarkers)
	{
		for (const FNDRoomMarker& Marker : Definition.Markers)
		{
			if (Marker.Type == ENDRoomMarkerType::PlayerSpawn)
			{
				continue;
			}
			const float PadSize = Marker.Type == ENDRoomMarkerType::MonsterAnchor ? 120.f : 40.f;
			AddBox(GetMarkerMeshes(Marker.Type), FVector(Marker.LocalPosition.X, Marker.LocalPosition.Y, 2.f), FVector(PadSize, PadSize, 4.f));
		}
	}
}
