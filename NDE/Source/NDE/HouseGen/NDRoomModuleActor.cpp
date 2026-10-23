// No Despiertes Eso

#include "HouseGen/NDRoomModuleActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "HouseGen/NDHouseFurniture.h"
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
		Meshes->SetMobility(EComponentMobility::Static);
		Meshes->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Meshes->SetCanEverAffectNavigation(bCollision);
		return Meshes;
	}

	/** Pared de arte para un hueco centrado: malla y tamaño real de su hueco (medido en la malla). */
	struct FWallArt
	{
		const TCHAR* Mesh = nullptr;
		float HoleWidth = 0.f;
		float HoleHeight = 0.f;
	};

	FWallArt GetWallArt(float OpeningWidth)
	{
		// Los nombres de las mallas no coinciden con su hueco: la "Door_L" es la más estrecha (134 cm)
		// y la "Door_S" la más ancha (244 cm). La colisión va 2 cm dentro del hueco de la malla.
		if (OpeningWidth <= 0.f)
		{
			return { TEXT("Wall_400") };
		}
		if (OpeningWidth <= 120.f)
		{
			return { TEXT("Wall_Door_L_400"), 130.f, 479.f };
		}
		if (OpeningWidth <= 190.f)
		{
			return { TEXT("Wall_Door_M_400"), 164.f, 500.f };
		}
		if (OpeningWidth <= 240.f)
		{
			return { TEXT("Wall_Door_S_400"), 240.f, 473.f };
		}
		if (OpeningWidth <= 270.f)
		{
			// Arco: se estrecha arriba; la colisión deja el paso recto de abajo.
			return { TEXT("Wall_Arch_L_400"), 252.f, 400.f };
		}
		return {};
	}

	/** Suelo de arte según la sala. */
	const TCHAR* GetFloorArt(ENDRoomType RoomType)
	{
		switch (RoomType)
		{
		case ENDRoomType::R04_Kitchen:
		case ENDRoomType::R07_Bathroom:
		case ENDRoomType::R12_Pantry:
		case ENDRoomType::R13_Laundry:
			return TEXT("Floor_Tile_400");
		case ENDRoomType::R08_Storage:
		case ENDRoomType::R16_Basement:
		case ENDRoomType::R18_Garden:
			return TEXT("Floor_Concrete_400");
		case ENDRoomType::R06_MasterBedroom:
		case ENDRoomType::R10_KidsBedroom:
		case ENDRoomType::R14_Library:
		case ENDRoomType::R15_Playroom:
			return TEXT("Floor_Carpet_400");
		default:
			return TEXT("Floor_Wood_400");
		}
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
	// Estática: no se mueve, y así no hace falta referirse a ella por red como "suelo que se mueve"
	// (un cliente que entra tarde podría no resolverla y su personaje se quedaría atascado).
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	FloorMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Floor"), Root, CubeFinder.Object, true);
	WallMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Walls"), Root, CubeFinder.Object, true);
	CeilingMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Ceiling"), Root, CubeFinder.Object, true);
	FallbackMeshes = NDRoomModuleActor::CreateMeshes(this, TEXT("Fallback"), Root, CubeFinder.Object, false);
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
	FallbackMeshes->ClearInstances();
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Art : ArtMeshes)
	{
		if (Art.Value)
		{
			Art.Value->ClearInstances();
		}
	}
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

void ANDRoomModuleActor::AddCollisionBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, bool bArtCovers, const FRotator& Rotation)
{
	AddBox(Target, Center, Size, Rotation);
	if (bUseArt && !bArtCovers)
	{
		AddBox(FallbackMeshes, Center, Size, Rotation);
	}
}

bool ANDRoomModuleActor::AddArt(const FString& MeshPath, const FTransform& Transform)
{
	UStaticMesh* Mesh = bUseArt ? NDHouseArt::LoadMesh(MeshPath) : nullptr;
	if (!Mesh)
	{
		return false;
	}
	TObjectPtr<UInstancedStaticMeshComponent>& Meshes = ArtMeshes.FindOrAdd(Mesh);
	if (!Meshes)
	{
		Meshes = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Meshes->SetupAttachment(Root);
		Meshes->SetStaticMesh(Mesh);
		Meshes->SetMobility(EComponentMobility::Static);
		// Solo se ve: la colisión y la navegación son del greybox invisible.
		Meshes->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Meshes->SetCanEverAffectNavigation(false);
		Meshes->RegisterComponent();
	}
	Meshes->AddInstance(Transform);
	return true;
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

void ANDRoomModuleActor::BuildEdge(const FVector& EdgeCenter, ENDCellDirection Direction, float OpeningWidth, float OpeningOffset,
	ENDWallStyle Style, float WindowCenter)
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

	// Arte: las piezas de pared miden 400 en X (a lo largo) y la colisión sigue el hueco real de la pieza con puerta.
	// El borde de la celda se llena con piezas enteras y el resto con una pieza estrechada.
	float DoorTop = DoorHeight;
	bool bArt = false;
	const FRotator Yaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)), 0.f);
	const FString SolidArt = NDHouseArt::GetStructurePath(TEXT("Wall_400"));
	// Pieza de arte de s a s + Length a lo largo de la pared (s va de -CellSize/2 a +CellSize/2).
	auto AddPiece = [this, &Yaw, &Tangent, &WallCenter](const FString& Mesh, float Start, float Length)
	{
		return AddArt(Mesh, FTransform(Yaw, WallCenter + Tangent * Start, FVector(Length / ArtTileSize, 1.f, 1.f)));
	};
	auto AddStrip = [&](float Start, float End)
	{
		bool bAll = true;
		for (float S = Start; S < End - 1.f; S += ArtTileSize)
		{
			const float Length = FMath::Min(ArtTileSize, End - S);
			// Ventana: en una pieza entera de una pared exterior.
			const bool bWindow = Style == ENDWallStyle::Window && Length >= ArtTileSize - 1.f && FMath::IsNearlyEqual(S + Length * 0.5f, WindowCenter, ArtTileSize * 0.5f);
			bAll &= AddPiece(bWindow ? NDHouseArt::GetStructurePath(TEXT("Wall_Window_400")) : SolidArt, S, Length);
			if (bWindow)
			{
				// Marco de 200 cm centrado en el hueco de la pieza (de 87 a 315 en X y de 243 a 476 en Z).
				AddArt(NDHouseArt::GetStructurePath(TEXT("Window_Frame")), FTransform(Yaw, WallCenter + Tangent * (S + 100.f) + FVector(0.f, 0.f, 235.f)));
			}
		}
		// Fachada por fuera: piezas macizas con la cara de fuera (-Y de la malla) hacia la calle.
		if (Style != ENDWallStyle::Interior)
		{
			const FVector Outside = EdgeCenter + Normal * (WallThickness + 5.f);
			for (float S = Start; S < End - 1.f; S += ArtTileSize)
			{
				const float Length = FMath::Min(ArtTileSize, End - S);
				const bool bFacadeWindow = Style == ENDWallStyle::FacadeWindow && Length >= ArtTileSize - 1.f;
				AddArt(NDHouseArt::GetStructurePath(bFacadeWindow ? TEXT("Facade_Window_400") : TEXT("Facade_Wall_400")),
					FTransform(Yaw, Outside + Tangent * S, FVector(Length / ArtTileSize, 1.f, 1.f)));
			}
		}
		return bAll;
	};
	const NDRoomModuleActor::FWallArt WallArt = NDRoomModuleActor::GetWallArt(OpeningWidth);
	if (WallArt.Mesh && OpeningWidth <= 0.f)
	{
		bArt = AddStrip(-CellSize * 0.5f, CellSize * 0.5f);
	}
	else if (WallArt.Mesh)
	{
		// Pieza con puerta centrada en el hueco y paredes a los lados.
		const float PieceStart = FMath::Clamp(OpeningOffset - ArtTileSize * 0.5f, -CellSize * 0.5f, CellSize * 0.5f - ArtTileSize);
		bArt = AddPiece(NDHouseArt::GetStructurePath(WallArt.Mesh), PieceStart, ArtTileSize);
		bArt &= AddStrip(-CellSize * 0.5f, PieceStart) && AddStrip(PieceStart + ArtTileSize, CellSize * 0.5f);
		if (bArt)
		{
			OpeningWidth = WallArt.HoleWidth;
			OpeningOffset = PieceStart + ArtTileSize * 0.5f;
			DoorTop = WallArt.HoleHeight;
		}
	}

	if (OpeningWidth <= 0.f)
	{
		AddCollisionBox(WallMeshes, WallCenter + FVector(0.f, 0.f, WallHeight * 0.5f), MakeSize(CellSize, WallHeight), bArt);
		return;
	}

	// Hueco de [Center - Door/2, Center + Door/2] a lo largo de la pared, que va de -CellSize/2 a +CellSize/2.
	const float Door = FMath::Min(OpeningWidth, CellSize - 2.f * WallThickness);
	const float Center = FMath::Clamp(OpeningOffset, -CellSize * 0.5f + Door * 0.5f, CellSize * 0.5f - Door * 0.5f);
	const float Starts[] = { -CellSize * 0.5f, Center + Door * 0.5f };
	const float Ends[] = { Center - Door * 0.5f, CellSize * 0.5f };
	for (int32 Piece = 0; Piece < 2; ++Piece)
	{
		const float Length = Ends[Piece] - Starts[Piece];
		if (Length > 1.f)
		{
			AddCollisionBox(WallMeshes, WallCenter + Tangent * ((Starts[Piece] + Ends[Piece]) * 0.5f) + FVector(0.f, 0.f, WallHeight * 0.5f), MakeSize(Length, WallHeight), bArt);
		}
	}
	const float LintelHeight = WallHeight - DoorTop;
	AddCollisionBox(WallMeshes, WallCenter + Tangent * Center + FVector(0.f, 0.f, DoorTop + LintelHeight * 0.5f), MakeSize(Door, LintelHeight), bArt);
}

void ANDRoomModuleActor::BuildGreybox(const FNDRoomModuleDefinition& Definition, const TArray<float>& OpeningWidths, bool bBuildCeiling, bool bShowMarkers,
	bool bAddRoomLight, const TArray<bool>* CeilingPerCell, const TArray<float>* OpeningOffsets, const TArray<FNDRoomDoorSocket>* ExtraOpenings, const TArray<uint8>* WallStyles)
{
	using namespace NDHouseGen;
	ClearGreybox();

	const FIntPoint Size = Definition.FootprintCells;
	const FVector Extent(Size.X * CellSize, Size.Y * CellSize, 0.f);
	SetColor(FloorMeshes, FLinearColor(GetRoomTypeColor(Definition.RoomType)) * 0.6f);
	SetColor(WallMeshes, NDRoomModuleActor::WallColor);
	SetColor(CeilingMeshes, NDRoomModuleActor::CeilingColor);
	SetColor(FallbackMeshes, NDRoomModuleActor::WallColor);
	// Con arte, el greybox sigue dando colisión y navegación, pero no se ve.
	bUseArt = NDHouseArt::IsEnabled();
	FloorMeshes->SetVisibility(!bUseArt);
	WallMeshes->SetVisibility(!bUseArt);
	CeilingMeshes->SetVisibility(!bUseArt);
	const FString FloorArt = NDHouseArt::GetStructurePath(NDRoomModuleActor::GetFloorArt(Definition.RoomType));
	const FString CeilingArt = NDHouseArt::GetStructurePath(TEXT("Ceiling_400"));
	// Baldosas de arte (pivote en una esquina: de 0 a 400 en X y de 0 a -400 en Y), varias por celda.
	const int32 Tiles = FMath::Max(1, FMath::RoundToInt(CellSize / ArtTileSize));
	auto AddTiles = [this, Tiles](const FString& Mesh, const FVector& CellCenter, float Z)
	{
		bool bAll = true;
		for (int32 X = 0; X < Tiles; ++X)
		{
			for (int32 Y = 0; Y < Tiles; ++Y)
			{
				bAll &= AddArt(Mesh, FTransform(CellCenter + FVector(-CellSize * 0.5f + X * ArtTileSize, CellSize * 0.5f - Y * ArtTileSize, Z)));
			}
		}
		return bAll;
	};

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
			const bool bArt = AddTiles(FloorArt, CellCenter, 0.f);
			AddCollisionBox(FloorMeshes, CellCenter - FVector(0.f, 0.f, WallThickness * 0.5f), FVector(CellSize, CellSize, WallThickness), bArt);
		}
		const bool bCeilingHere = !CeilingPerCell || !CeilingPerCell->IsValidIndex(CellIndex) || (*CeilingPerCell)[CellIndex];
		if (bBuildCeiling && bCeilingHere)
		{
			const bool bArt = AddTiles(CeilingArt, CellCenter, WallHeight);
			AddCollisionBox(CeilingMeshes, CellCenter + FVector(0.f, 0.f, WallHeight + WallThickness * 0.5f), FVector(CellSize, CellSize, WallThickness), bArt);
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
			float Opening = OpeningWidths.IsValidIndex(SocketIndex) ? OpeningWidths[SocketIndex] : 0.f;
			float HoleOffset = 0.f;
			if (OpeningOffsets && OpeningOffsets->IsValidIndex(SocketIndex))
			{
				HoleOffset = (*OpeningOffsets)[SocketIndex];
			}
			else if (Definition.DoorSockets.IsValidIndex(SocketIndex))
			{
				HoleOffset = Definition.DoorSockets[SocketIndex].Offset;
			}
			// Puerta "Auto" en un borde sin hueco en el módulo.
			const FNDRoomDoorSocket* Extra = ExtraOpenings
				? ExtraOpenings->FindByPredicate([&Cell, Direction](const FNDRoomDoorSocket& S) { return S.Cell == Cell && S.Direction == Direction; })
				: nullptr;
			if (SocketIndex == INDEX_NONE && Extra)
			{
				Opening = Extra->Width;
				HoleOffset = Extra->Offset;
			}
			const FIntPoint Offset = DirectionToOffset(Direction);
			// Pared exterior (fachada y ventanas) según el lote; la ventana cae en una u otra pieza.
			const int32 StyleIndex = CellIndex * 4 + DirIndex;
			const ENDWallStyle Style = WallStyles && WallStyles->IsValidIndex(StyleIndex) ? static_cast<ENDWallStyle>((*WallStyles)[StyleIndex]) : ENDWallStyle::Interior;
			const float WindowCenter = ((CellIndex + DirIndex) % 2 == 0 ? -1.f : 1.f) * CellSize * 0.25f;
			BuildEdge(CellCenter + FVector(Offset.X, Offset.Y, 0.f) * (CellSize * 0.5f), Direction, Opening, HoleOffset, Style, WindowCenter);
		}
	}

	// Arranque de escalera: rampas y rellanos (losas cuya cara de arriba va de Start a End) y muretes.
	for (const FNDStairSegment& Segment : Definition.StairSegments)
	{
		const FVector Delta = Segment.End - Segment.Start;
		const float Horizontal = FVector2D(Delta.X, Delta.Y).Size();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		const FVector Middle = (Segment.Start + Segment.End) * 0.5f;
		if (Segment.Height > 0.f)
		{
			const float Bottom = FMath::Min(Segment.Start.Z, Segment.End.Z);
			const FRotator SegmentYaw(0.f, Yaw, 0.f);
			bool bArt = false;
			if (Segment.Width <= WallThickness + 1.f)
			{
				// Murete: pared de arte estrechada (va de 0 a 400 en X, 30 de grueso y 600 de alto).
				bArt = AddArt(NDHouseArt::GetStructurePath(TEXT("Wall_400")), FTransform(SegmentYaw, FVector(Segment.Start.X, Segment.Start.Y, Bottom),
					FVector(Horizontal / ArtTileSize, Segment.Width / 30.f, Segment.Height / 600.f)));
			}
			else
			{
				// Rellano: tarima de madera encima; los lados siguen siendo greybox.
				AddArt(NDHouseArt::GetStructurePath(TEXT("Floor_Wood_400")), FTransform(SegmentYaw,
					FVector(Middle.X, Middle.Y, Bottom + Segment.Height) - SegmentYaw.RotateVector(FVector(Horizontal * 0.5f, -Segment.Width * 0.5f, 0.f)),
					FVector(Horizontal / ArtTileSize, Segment.Width / ArtTileSize, 1.f)));
			}
			AddCollisionBox(FloorMeshes, FVector(Middle.X, Middle.Y, Bottom + Segment.Height * 0.5f), FVector(Horizontal, Segment.Width, Segment.Height), bArt, SegmentYaw);
		}
		else
		{
			// Tramo: la escalera de arte (400 de ancho, sube 600 en 800, de 0 a -800 en Y) ajustada al tramo.
			const FRotator ArtYaw(0.f, FMath::RadiansToDegrees(FMath::Atan2(Delta.X, -Delta.Y)), 0.f);
			const bool bArt = AddArt(NDHouseArt::GetStructurePath(TEXT("Stairs_Giant")), FTransform(ArtYaw,
				Segment.Start - ArtYaw.RotateVector(FVector(Segment.Width * 0.5f, 0.f, 0.f)),
				FVector(Segment.Width / ArtTileSize, Horizontal / 800.f, Delta.Z / FloorHeight)));
			const FRotator Rotation(FMath::RadiansToDegrees(FMath::Atan2(Delta.Z, Horizontal)), Yaw, 0.f);
			const FVector Center = Middle - Rotation.RotateVector(FVector::UpVector) * (WallThickness * 0.5f);
			AddCollisionBox(FloorMeshes, Center, FVector(Delta.Size(), Segment.Width, WallThickness), bArt, Rotation);
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
