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

void ANDRoomModuleActor::AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size)
{
	// El cubo del motor mide 100 cm y tiene el pivote en el centro.
	Target->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
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

void ANDRoomModuleActor::BuildGreybox(const FNDRoomModuleDefinition& Definition, const TArray<float>& OpeningWidths, bool bBuildCeiling, bool bShowMarkers, bool bAddRoomLight)
{
	using namespace NDHouseGen;
	ClearGreybox();

	const FIntPoint Size = Definition.FootprintCells;
	const FVector Extent(Size.X * CellSize, Size.Y * CellSize, 0.f);
	SetColor(FloorMeshes, FLinearColor(GetRoomTypeColor(Definition.RoomType)) * 0.6f);
	SetColor(WallMeshes, NDRoomModuleActor::WallColor);
	SetColor(CeilingMeshes, NDRoomModuleActor::CeilingColor);

	// Suelo con la cara de arriba en Z = 0.
	AddBox(FloorMeshes, FVector(0.f, 0.f, -WallThickness * 0.5f), FVector(Extent.X, Extent.Y, WallThickness));
	if (bBuildCeiling)
	{
		AddBox(CeilingMeshes, FVector(0.f, 0.f, WallHeight + WallThickness * 0.5f), FVector(Extent.X, Extent.Y, WallThickness));
	}

	// Paredes en cada borde exterior de celda; con hueco si la casa abrió ese socket.
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			const FVector CellCenter((X + 0.5f) * CellSize - Extent.X * 0.5f, (Y + 0.5f) * CellSize - Extent.Y * 0.5f, 0.f);
			for (int32 DirIndex = 0; DirIndex < 4; ++DirIndex)
			{
				const ENDCellDirection Direction = static_cast<ENDCellDirection>(DirIndex);
				const FIntPoint Neighbor = FIntPoint(X, Y) + DirectionToOffset(Direction);
				if (Neighbor.X >= 0 && Neighbor.Y >= 0 && Neighbor.X < Size.X && Neighbor.Y < Size.Y)
				{
					continue;
				}
				const int32 SocketIndex = Definition.DoorSockets.IndexOfByPredicate([X, Y, Direction](const FNDRoomDoorSocket& S)
				{
					return S.Cell == FIntPoint(X, Y) && S.Direction == Direction;
				});
				const float Opening = OpeningWidths.IsValidIndex(SocketIndex) ? OpeningWidths[SocketIndex] : 0.f;
				const FIntPoint Offset = DirectionToOffset(Direction);
				BuildEdge(CellCenter + FVector(Offset.X, Offset.Y, 0.f) * (CellSize * 0.5f), Direction, Opening);
			}
		}
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
