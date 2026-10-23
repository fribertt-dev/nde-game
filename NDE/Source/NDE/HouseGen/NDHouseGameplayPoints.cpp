// No Despiertes Eso
// Puntos de juego de una casa resuelta: escondites, peligros, patrulla de la criatura y grafo de sonido.

#include "HouseGen/NDHouseLayout.h"

#include "HouseGen/NDHouseFurniture.h"

namespace NDHouseGameplay
{
	/** Ancho del bulto por defecto (FNDHouseGenParams::CarryEnvelopeWidth). */
	constexpr float DefaultCarryWidth = 120.f;
	/** Distancia del escondite a su punto de entrada. */
	constexpr float HidingEntryDistance = 70.f;
	/** Pasos anchos: a partir de aquí el sonido casi no se frena. */
	constexpr float WideOpeningWidth = 300.f;
	/** Puerta más estrecha por la que pasa la criatura en su patrulla (las Door_S, de 100 cm, no). */
	constexpr float CreatureMinDoorWidth = 140.f;

	ENDHidingSpotKind GetHidingKind(ENDRoomType RoomType)
	{
		switch (RoomType)
		{
		case ENDRoomType::R03_LivingRoom:
		case ENDRoomType::R06_MasterBedroom:
		case ENDRoomType::R10_KidsBedroom:
			return ENDHidingSpotKind::UnderFurniture;
		case ENDRoomType::R04_Kitchen:
		case ENDRoomType::R07_Bathroom:
		case ENDRoomType::R08_Storage:
		case ENDRoomType::R12_Pantry:
		case ENDRoomType::R13_Laundry:
			return ENDHidingSpotKind::InsideCabinet;
		default:
			return ENDHidingSpotKind::BehindFurniture;
		}
	}

	/**
	 * Puntos del hueco de una puerta: el centro de la pared y, si el hueco va desplazado (carriles de las
	 * escaleras en U), también los dos lados posibles del desplazamiento.
	 */
	void AddDoorPoints(const FNDHouseLayout& Layout, int32 RoomIndex, int32 SocketIndex, TArray<FVector>& OutPoints)
	{
		const FNDHouseLayoutSocket& Socket = Layout.Rooms[RoomIndex].Sockets[SocketIndex];
		const FVector Center = Layout.GetSocketLocation(RoomIndex, SocketIndex);
		OutPoints.Add(Center);
		float Offset = Socket.Offset;
		if (FMath::IsNearlyZero(Offset) && Socket.Connection != INDEX_NONE)
		{
			const FNDHouseLayoutConnection& Link = Layout.Connections[Socket.Connection];
			const bool bIsA = Link.RoomA == RoomIndex && Link.SocketA == SocketIndex;
			Offset = Layout.Rooms[bIsA ? Link.RoomB : Link.RoomA].Sockets[bIsA ? Link.SocketB : Link.SocketA].Offset;
		}
		if (!FMath::IsNearlyZero(Offset))
		{
			const FIntPoint Out = NDHouseGen::DirectionToOffset(Socket.LotDirection);
			const FVector Tangent(-Out.Y, Out.X, 0.f);
			OutPoints.Add(Center + Tangent * Offset);
			OutPoints.Add(Center - Tangent * Offset);
		}
	}

	/** Peligro típico de cada sala; las demás eligen con la semilla. */
	ENDHouseHazardType GetHazardType(ENDRoomType RoomType, FRandomStream& Rng)
	{
		switch (RoomType)
		{
		case ENDRoomType::R04_Kitchen:
		case ENDRoomType::R05_DiningRoom:
			return ENDHouseHazardType::FragileItem;
		case ENDRoomType::R08_Storage:
		case ENDRoomType::R12_Pantry:
		case ENDRoomType::R16_Basement:
			return ENDHouseHazardType::MouseTrap;
		case ENDRoomType::R02_Corridor:
		case ENDRoomType::R06_MasterBedroom:
			return ENDHouseHazardType::CreakyFloor;
		default:
			return static_cast<ENDHouseHazardType>(Rng.RandRange(0, 2));
		}
	}
}

void FNDHouseLayout::GetCarryRouteRooms(float CarryEnvelopeWidth, TSet<int32>& OutRooms) const
{
	TArray<int32> Route;
	if (FindRoute(EntryRoom, ObjectiveRoom, CarryEnvelopeWidth, &Route))
	{
		OutRooms.Append(Route);
	}
	if (Rooms.IsValidIndex(EntryRoom))
	{
		OutRooms.Add(EntryRoom);
	}
}

int32 FNDHouseLayout::GetAnchorRoom(const FNDContractHouseRequirements& Requirements) const
{
	switch (Requirements.ObjectiveAnchor)
	{
	case ENDContractAnchor::BedHeadboard:
		return BedroomRoom;
	case ENDContractAnchor::RequiredRoom:
		if (Requirements.RequiredRooms.Num() > 0)
		{
			const ENDRoomType Type = Requirements.RequiredRooms[0];
			return Rooms.IndexOfByPredicate([Type](const FNDHouseLayoutRoom& Room) { return Room.Def->RoomType == Type; });
		}
		return ObjectiveRoom;
	default:
		return ObjectiveRoom;
	}
}

int32 FNDHouseLayout::CountAccesses(int32 RoomIndex) const
{
	if (!Rooms.IsValidIndex(RoomIndex))
	{
		return 0;
	}
	return Rooms[RoomIndex].Sockets.FilterByPredicate([](const FNDHouseLayoutSocket& Socket) { return Socket.IsOpen(); }).Num();
}

int32 FNDHouseLayout::FindRoomAt(const FVector& LotPosition) const
{
	const FIntPoint Cell(
		FMath::FloorToInt(LotPosition.X / NDHouseGen::CellSize + Lot.SizeCells.X * 0.5f),
		FMath::FloorToInt(LotPosition.Y / NDHouseGen::CellSize + Lot.SizeCells.Y * 0.5f));
	// Un poco por encima del suelo cuenta como ese piso.
	const int32 Floor = FMath::FloorToInt((LotPosition.Z + 50.f) / NDHouseGen::FloorHeight);
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		if (Rooms[RoomIndex].Floor != Floor)
		{
			continue;
		}
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		if (Cells.Contains(Cell))
		{
			return RoomIndex;
		}
	}
	return INDEX_NONE;
}

float FNDHouseLayout::GetSoundTransmission(const FVector& LotA, const FVector& LotB) const
{
	const int32 RoomA = FindRoomAt(LotA);
	const int32 RoomB = FindRoomAt(LotB);
	if (RoomA == INDEX_NONE || RoomB == INDEX_NONE)
	{
		return 0.f;
	}
	if (RoomA == RoomB)
	{
		return 1.f;
	}
	// Dijkstra que maximiza el producto (pocas salas: basta un barrido simple).
	TArray<float> Best;
	Best.Init(0.f, Rooms.Num());
	TArray<bool> Done;
	Done.Init(false, Rooms.Num());
	Best[RoomA] = 1.f;
	for (int32 Iteration = 0; Iteration < Rooms.Num(); ++Iteration)
	{
		int32 Current = INDEX_NONE;
		for (int32 Index = 0; Index < Rooms.Num(); ++Index)
		{
			if (!Done[Index] && Best[Index] > 0.f && (Current == INDEX_NONE || Best[Index] > Best[Current]))
			{
				Current = Index;
			}
		}
		if (Current == INDEX_NONE || Current == RoomB)
		{
			break;
		}
		Done[Current] = true;
		for (const FNDHouseAudioLink& Link : AudioLinks)
		{
			const int32 Other = Link.RoomA == Current ? Link.RoomB : (Link.RoomB == Current ? Link.RoomA : INDEX_NONE);
			if (Other != INDEX_NONE && !Done[Other])
			{
				Best[Other] = FMath::Max(Best[Other], Best[Current] * Link.Transmission);
			}
		}
	}
	return Best[RoomB];
}

void FNDHouseLayout::BuildGameplayPoints()
{
	using namespace NDHouseGameplay;
	HidingSpots.Reset();
	Hazards.Reset();
	PatrolNodes.Reset();
	PatrolRoute.Reset();
	AudioLinks.Reset();

	// Escondites: las marcas azules de cada módulo, con entrada hacia el centro de la sala.
	for (const FNDHouseLayoutMarker& Marker : Markers)
	{
		if (Marker.Type != ENDRoomMarkerType::HideSpot || !Rooms.IsValidIndex(Marker.Room))
		{
			continue;
		}
		FNDHouseHidingSpot& Spot = HidingSpots.AddDefaulted_GetRef();
		Spot.StableId = HidingSpots.Num() - 1;
		Spot.Room = Marker.Room;
		Spot.LotTransform = Marker.LotTransform;
		Spot.Kind = GetHidingKind(Rooms[Marker.Room].Def->RoomType);
		Spot.Capacity = Spot.Kind == ENDHidingSpotKind::UnderFurniture ? 2 : 1;
		const FVector Location = Marker.LotTransform.GetLocation();
		FVector ToCenter = GetRoomCenter(Marker.Room) - Location;
		ToCenter.Z = 0.f;
		const FVector Direction = ToCenter.IsNearlyZero() ? FVector::ForwardVector : ToCenter.GetSafeNormal();
		Spot.EntryPoint = Location + Direction * FMath::Min(HidingEntryDistance, ToCenter.Size());
	}

	// Grafo de sonido: una arista por puerta.
	for (const FNDHouseLayoutConnection& Link : Connections)
	{
		const FNDHouseLayoutSocket& Socket = Rooms[Link.RoomA].Sockets[Link.SocketA];
		FNDHouseAudioLink& Audio = AudioLinks.AddDefaulted_GetRef();
		Audio.RoomA = Link.RoomA;
		Audio.RoomB = Link.RoomB;
		Audio.Transmission = Socket.Type == ENDRoomSocketType::Stairs ? StairsTransmission
			: (Link.OpeningWidth >= WideOpeningWidth ? WideOpeningTransmission : DoorOpenTransmission);
	}

	// Patrulla: un nodo por sala, en la celda más cercana a su centro (las salas en L tienen celdas vacías).
	TArray<int32> NodeOfRoom;
	NodeOfRoom.Init(INDEX_NONE, Rooms.Num());
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		const FVector Center = GetRoomCenter(RoomIndex);
		FVector BestLocation = Center;
		float BestDistance = TNumericLimits<float>::Max();
		for (const FIntPoint& Cell : Cells)
		{
			const FVector CellCenter = Lot.GetCellCenter(Cell) + FVector(0.f, 0.f, Center.Z);
			const float Distance = FVector::DistSquared(CellCenter, Center);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestLocation = CellCenter;
			}
		}
		// Escaleras: el centro cae en la rampa o debajo; el nodo va en el umbral de su puerta de piso.
		const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
		if (Room.Def->StairPart != ENDStairPart::None)
		{
			for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
			{
				const FNDHouseLayoutSocket& Socket = Room.Sockets[SocketIndex];
				if (Socket.IsOpen() && Socket.Type != ENDRoomSocketType::Stairs)
				{
					const FVector Door = GetSocketLocation(RoomIndex, SocketIndex);
					BestLocation = Door;
					break;
				}
			}
		}
		FNDHousePatrolNode& Node = PatrolNodes.AddDefaulted_GetRef();
		Node.StableId = PatrolNodes.Num() - 1;
		Node.Room = RoomIndex;
		Node.Location = BestLocation;
		NodeOfRoom[RoomIndex] = Node.StableId;
	}
	for (const FNDHouseLayoutConnection& Link : Connections)
	{
		// La criatura no pasa por los pasos bajos ni por las puertas pequeñas (baños): esas salas quedan fuera de la patrulla.
		if (!IsLowClearance(Link) && Link.OpeningWidth >= CreatureMinDoorWidth)
		{
			PatrolNodes[NodeOfRoom[Link.RoomA]].Neighbors.AddUnique(NodeOfRoom[Link.RoomB]);
			PatrolNodes[NodeOfRoom[Link.RoomB]].Neighbors.AddUnique(NodeOfRoom[Link.RoomA]);
		}
	}
	// Recorrido: ida y vuelta en profundidad desde el dormitorio (o la entrada si no hay).
	const int32 StartRoom = Rooms.IsValidIndex(BedroomRoom) ? BedroomRoom : EntryRoom;
	if (Rooms.IsValidIndex(StartRoom))
	{
		TArray<bool> Visited;
		Visited.Init(false, PatrolNodes.Num());
		TFunction<void(int32)> Walk = [&](int32 Node)
		{
			Visited[Node] = true;
			PatrolRoute.Add(Node);
			for (const int32 Next : PatrolNodes[Node].Neighbors)
			{
				if (!Visited[Next])
				{
					Walk(Next);
					PatrolRoute.Add(Node);
				}
			}
		};
		Walk(NodeOfRoom[StartRoom]);
	}

	// Peligros: fuera de la ruta del bulto, de las escaleras y lejos de las puertas. Uno de cada dos salas libres.
	TSet<int32> RouteRooms;
	GetCarryRouteRooms(DefaultCarryWidth, RouteRooms);
	FRandomStream Rng(static_cast<int32>(HashCombineFast(GetTypeHash(RunSeed), GetTypeHash(FString(TEXT("Hazards"))))));
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
		if (RouteRooms.Contains(RoomIndex) || Room.Def->StairPart != ENDStairPart::None || Rng.FRand() < 0.5f)
		{
			continue;
		}
		TArray<FVector> Doors;
		for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
		{
			if (Room.Sockets[SocketIndex].IsOpen())
			{
				AddDoorPoints(*this, RoomIndex, SocketIndex, Doors);
			}
		}
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		const ENDHouseHazardType Type = GetHazardType(Room.Def->RoomType, Rng);
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FIntPoint Cell = Cells[Rng.RandRange(0, Cells.Num() - 1)];
			const FVector Location = Lot.GetCellCenter(Cell) + FVector(Rng.FRandRange(-100.f, 100.f) * NDHouseGen::CellScale, Rng.FRandRange(-100.f, 100.f) * NDHouseGen::CellScale, Room.Floor * NDHouseGen::FloorHeight);
			const bool bNearDoor = Doors.ContainsByPredicate([&Location](const FVector& Door) { return FVector::Dist2D(Door, Location) < HazardDoorClearance; });
			if (!bNearDoor)
			{
				FNDHouseHazard& Hazard = Hazards.AddDefaulted_GetRef();
				Hazard.StableId = Hazards.Num() - 1;
				Hazard.Room = RoomIndex;
				Hazard.Type = Type;
				Hazard.LotTransform = FTransform(FRotator(0.f, Rng.RandRange(0, 3) * 90.f, 0.f), Location);
				break;
			}
		}
	}

	BuildDecorations();
}

namespace NDHouseDecorations
{
	/** El arte de las paredes mide 30 cm y va 10 cm dentro de la sala: su cara interior queda a 25 cm del borde. */
	constexpr float WallInset = 27.f;
	/** Paso libre alrededor del centro de la sala (nodo de patrulla) y a cada lado de la ruta puerta-centro:
	 * más que el radio del agente del NavMesh (34 cm), o la patrulla de la criatura no pasa. */
	constexpr float HubClearance = 45.f;
	constexpr float RouteClearance = 45.f;
	/** Paso libre a los lados del hueco de una puerta. */
	constexpr float DoorClearance = 20.f;
	/** Distancia a botín, spawns, escondites y peligros. */
	constexpr float ReservedClearance = 35.f;
	/** Hueco entre dos muebles. */
	constexpr float Gap = 10.f;
	/** Radio con el que se comprueba el paso por la sala (algo más que el agente del NavMesh) y paso de la rejilla. */
	constexpr float PathAgent = 40.f;
	constexpr float PathStep = 25.f;

	struct FDoorPoint
	{
		FVector2D Point;
		float HalfWidth = 0.f;
		/** Hacia dentro de la sala. */
		FVector2D Inward = FVector2D::ZeroVector;
	};

	/** Tramo recto de pared de una sala, con los sitios donde probar un mueble. */
	struct FWallRun
	{
		FVector2D Along;
		FVector2D WallPoint;
		FIntPoint Out;
		float Start = 0.f;
		float End = 0.f;
		TArray<float> Stops;
	};

	/** Giro que lleva el respaldo (+Y local) hacia la pared de normal Out. */
	float GetWallYaw(const FIntPoint& Out)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(-static_cast<float>(Out.X), static_cast<float>(Out.Y)));
	}

	float DistanceToBox(const FBox2D& Box, const FVector2D& Point)
	{
		return FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(Point));
	}

	float DistanceToSegment(const FBox2D& Box, const FVector2D& A, const FVector2D& B)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 20.f));
		float Best = UE_BIG_NUMBER;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			Best = FMath::Min(Best, DistanceToBox(Box, FMath::Lerp(A, B, Step / static_cast<float>(Steps))));
		}
		return Best;
	}

	FBox2D MakeFootprint(const FVector& Center, float Yaw, const FVector& Size)
	{
		const bool bTurned = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Yaw))) > 0.5f;
		const FVector2D Half = bTurned ? FVector2D(Size.Y, Size.X) * 0.5f : FVector2D(Size.X, Size.Y) * 0.5f;
		return FBox2D(FVector2D(Center) - Half, FVector2D(Center) + Half);
	}
}

void FNDHouseLayout::BuildDecorations()
{
	using namespace NDHouseGameplay;
	using namespace NDHouseDecorations;
	Decorations.Reset();
	FRandomStream Rng(static_cast<int32>(HashCombineFast(GetTypeHash(RunSeed), GetTypeHash(FString(TEXT("Decorations"))))));
	const TArray<FNDFurnitureArt>& ArtList = NDHouseFurniture::GetArt();

	// Puntos que un mueble no puede tapar: botín, spawns, criatura, escondites, peligros.
	TArray<FVector> Reserved;
	for (const FNDHouseLayoutMarker& Marker : Markers)
	{
		Reserved.Add(Marker.LotTransform.GetLocation());
		if (Marker.Type == ENDRoomMarkerType::MonsterAnchor && Rooms.IsValidIndex(Marker.Room))
		{
			// El borde de la cama (C05 pone ahí la medalla) también queda libre.
			const FVector ToCenter = (GetRoomCenter(Marker.Room) - Marker.LotTransform.GetLocation()).GetSafeNormal2D();
			Reserved.Add(Marker.LotTransform.GetLocation() + ToCenter * 90.f);
		}
	}
	for (const FNDHouseHidingSpot& Spot : HidingSpots)
	{
		Reserved.Add(Spot.EntryPoint);
	}
	for (const FNDHouseHazard& Hazard : Hazards)
	{
		Reserved.Add(Hazard.LotTransform.GetLocation());
	}

	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
		if (Room.Def->StairPart != ENDStairPart::None || Room.Def->RoomType == ENDRoomType::R02_Corridor)
		{
			continue;
		}
		const float FloorZ = Room.Floor * NDHouseGen::FloorHeight;
		const FVector2D Hub(PatrolNodes[RoomIndex].Location);
		// Puertas abiertas con su medio ancho; la ruta de cada una va al nodo de patrulla (el centro libre).
		TArray<FDoorPoint> Doors;
		for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
		{
			const FNDHouseLayoutSocket& Socket = Room.Sockets[SocketIndex];
			if (!Socket.IsOpen())
			{
				continue;
			}
			const float Width = Socket.Connection != INDEX_NONE ? Connections[Socket.Connection].OpeningWidth : Socket.Width;
			TArray<FVector> Points;
			AddDoorPoints(*this, RoomIndex, SocketIndex, Points);
			for (const FVector& Point : Points)
			{
				Doors.Add({ FVector2D(Point), Width * 0.5f, -FVector2D(NDHouseGen::DirectionToOffset(Socket.LotDirection)) });
			}
		}
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		const TSet<FIntPoint> RoomCells(Cells);
		auto IsInRoom = [this, &RoomCells](const FVector2D& Point)
		{
			return RoomCells.Contains(FIntPoint(
				FMath::FloorToInt(Point.X / NDHouseGen::CellSize + Lot.SizeCells.X * 0.5f),
				FMath::FloorToInt(Point.Y / NDHouseGen::CellSize + Lot.SizeCells.Y * 0.5f)));
		};
		// La huella, con el grosor de las paredes, cae entera dentro de la sala.
		auto FitsInRoom = [&IsInRoom](const FBox2D& Box)
		{
			const FBox2D Outer = Box.ExpandBy(WallInset);
			const FVector2D Size = Outer.GetSize();
			const int32 StepsX = FMath::Max(1, FMath::CeilToInt(Size.X / 50.f));
			const int32 StepsY = FMath::Max(1, FMath::CeilToInt(Size.Y / 50.f));
			for (int32 X = 0; X <= StepsX; ++X)
			{
				for (int32 Y = 0; Y <= StepsY; ++Y)
				{
					// Un poco hacia dentro: el borde exterior cae justo en la línea entre celdas.
					const FVector2D Point = Outer.Min + FVector2D(Size.X * X / StepsX, Size.Y * Y / StepsY);
					if (!IsInRoom(FMath::Lerp(Point, Outer.GetCenter(), 0.001f)))
					{
						return false;
					}
				}
			}
			return true;
		};
		// Paso de la criatura y del bulto: con el radio del agente del NavMesh, desde el centro de la sala se llega
		// a cada puerta sin chocar con paredes ni muebles. Se mira en una rejilla; lo que ya no se alcanzaba sin
		// muebles (p. ej. una puerta tras un murete) no cuenta.
		FVector2D GridMin(UE_BIG_NUMBER), GridMax(-UE_BIG_NUMBER);
		for (const FIntPoint& Cell : Cells)
		{
			const FVector2D Center(Lot.GetCellCenter(Cell));
			GridMin = FVector2D::Min(GridMin, Center - FVector2D(NDHouseGen::CellSize * 0.5f));
			GridMax = FVector2D::Max(GridMax, Center + FVector2D(NDHouseGen::CellSize * 0.5f));
		}
		const int32 GridX = FMath::CeilToInt((GridMax.X - GridMin.X) / PathStep);
		const int32 GridY = FMath::CeilToInt((GridMax.Y - GridMin.Y) / PathStep);
		auto GridPoint = [&GridMin](int32 X, int32 Y) { return GridMin + FVector2D(X + 0.5f, Y + 0.5f) * PathStep; };
		auto GridIndex = [&GridMin, GridX, GridY](const FVector2D& Point)
		{
			const int32 X = FMath::FloorToInt((Point.X - GridMin.X) / PathStep);
			const int32 Y = FMath::FloorToInt((Point.Y - GridMin.Y) / PathStep);
			return X >= 0 && Y >= 0 && X < GridX && Y < GridY ? X + Y * GridX : INDEX_NONE;
		};
		// Sin muebles: dentro de la sala y lejos de sus paredes.
		TArray<bool> WallFree;
		WallFree.SetNumUninitialized(GridX * GridY);
		const float WallReach = PathAgent + NDHouseGen::WallThickness;
		for (int32 Y = 0; Y < GridY; ++Y)
		{
			for (int32 X = 0; X < GridX; ++X)
			{
				const FVector2D Point = GridPoint(X, Y);
				WallFree[X + Y * GridX] = IsInRoom(Point) && IsInRoom(Point + FVector2D(WallReach, 0.f)) && IsInRoom(Point - FVector2D(WallReach, 0.f))
					&& IsInRoom(Point + FVector2D(0.f, WallReach)) && IsInRoom(Point - FVector2D(0.f, WallReach));
			}
		}
		// Puertas a las que hay que llegar: delante del hueco, ya lejos de la pared.
		TArray<int32> DoorTargets;
		for (const FDoorPoint& Door : Doors)
		{
			DoorTargets.Add(GridIndex(Door.Point + Door.Inward * (WallReach + PathStep)));
		}
		auto ReachedDoors = [&](const FBox2D* Extra)
		{
			TArray<FBox2D> Blocks;
			for (const FNDHouseDecoration& Other : Decorations)
			{
				if (Other.Room == RoomIndex && Other.bCollision)
				{
					Blocks.Add(Other.GetFootprint());
				}
			}
			if (Extra)
			{
				Blocks.Add(*Extra);
			}
			auto IsWalkable = [&](int32 Index)
			{
				const FVector2D Point = GridPoint(Index % GridX, Index / GridX);
				return WallFree[Index] && !Blocks.ContainsByPredicate([&Point](const FBox2D& Block) { return DistanceToBox(Block, Point) <= PathAgent; });
			};
			TArray<bool> Visited;
			Visited.SetNumZeroed(GridX * GridY);
			TArray<int32> Open;
			const int32 Start = GridIndex(Hub);
			if (Start != INDEX_NONE && IsWalkable(Start))
			{
				Visited[Start] = true;
				Open.Add(Start);
			}
			while (Open.Num() > 0)
			{
				const int32 Index = Open.Pop(EAllowShrinking::No);
				const int32 X = Index % GridX;
				const int32 Y = Index / GridX;
				for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
				{
					const int32 NextX = X + Step.X;
					const int32 NextY = Y + Step.Y;
					const int32 Next = NextX + NextY * GridX;
					if (NextX >= 0 && NextY >= 0 && NextX < GridX && NextY < GridY && !Visited[Next] && IsWalkable(Next))
					{
						Visited[Next] = true;
						Open.Add(Next);
					}
				}
			}
			TArray<bool> Reached;
			for (const int32 Target : DoorTargets)
			{
				Reached.Add(Target != INDEX_NONE && Visited[Target]);
			}
			return Reached;
		};
		const TArray<bool> ReachedWithoutFurniture = ReachedDoors(nullptr);
		auto KeepsPaths = [&](const FBox2D& Box)
		{
			const TArray<bool> Reached = ReachedDoors(&Box);
			for (int32 Index = 0; Index < Reached.Num(); ++Index)
			{
				if (ReachedWithoutFurniture[Index] && !Reached[Index])
				{
					return false;
				}
			}
			return true;
		};
		auto OverlapsFurniture = [this, RoomIndex](const FBox2D& Box)
		{
			return Decorations.ContainsByPredicate([&Box, RoomIndex](const FNDHouseDecoration& Other)
			{
				return Other.Room == RoomIndex && Other.Kind != ENDDecorationKind::Rug && Box.ExpandBy(Gap).Intersect(Other.GetFootprint());
			});
		};
		// Sitio libre para un mueble con colisión: no tapa puertas, rutas, el centro ni puntos reservados.
		auto IsFree = [&](const FBox2D& Box)
		{
			if (!FitsInRoom(Box) || OverlapsFurniture(Box) || DistanceToBox(Box, Hub) < HubClearance)
			{
				return false;
			}
			for (const FDoorPoint& Door : Doors)
			{
				if (DistanceToBox(Box, Door.Point) < Door.HalfWidth + DoorClearance || DistanceToSegment(Box, Door.Point, Hub) < RouteClearance)
				{
					return false;
				}
			}
			return !Reserved.ContainsByPredicate([&Box, FloorZ](const FVector& Point)
			{
				return FMath::Abs(Point.Z - FloorZ) < 200.f && DistanceToBox(Box, FVector2D(Point)) < ReservedClearance;
			}) && KeepsPaths(Box);
		};

		// Tramos rectos de pared: celdas seguidas con pared al mismo lado.
		TArray<FWallRun> WallRuns;
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const FIntPoint Out = NDHouseGen::DirectionToOffset(static_cast<ENDCellDirection>(Dir));
			const FIntPoint AlongCell(FMath::Abs(Out.Y), FMath::Abs(Out.X));
			TArray<FIntPoint> Edge = Cells.FilterByPredicate([&RoomCells, &Out](const FIntPoint& Cell) { return !RoomCells.Contains(Cell + Out); });
			// Por la línea de la pared y luego a lo largo.
			auto SortKey = [&AlongCell](const FIntPoint& Cell)
			{
				return (Cell.X * AlongCell.Y + Cell.Y * AlongCell.X) * 1000 + Cell.X * AlongCell.X + Cell.Y * AlongCell.Y;
			};
			Edge.Sort([&SortKey](const FIntPoint& A, const FIntPoint& B) { return SortKey(A) < SortKey(B); });
			for (int32 Index = 0; Index < Edge.Num();)
			{
				int32 Last = Index;
				while (Last + 1 < Edge.Num() && Edge[Last + 1] == Edge[Last] + AlongCell)
				{
					++Last;
				}
				FWallRun& Run = WallRuns.AddDefaulted_GetRef();
				Run.Along = FVector2D(AlongCell.X, AlongCell.Y);
				Run.Out = Out;
				Run.WallPoint = FVector2D(Lot.GetCellCenter(Edge[Index])) + FVector2D(Out.X, Out.Y) * (NDHouseGen::CellSize * 0.5f);
				Run.Start = -NDHouseGen::CellSize * 0.5f + WallInset;
				Run.End = (Last - Index + 0.5f) * NDHouseGen::CellSize - WallInset;
				for (int32 Cell = 0; Cell <= Last - Index; ++Cell)
				{
					Run.Stops.Add(Cell * NDHouseGen::CellSize);
				}
				Index = Last + 1;
			}
		}
		// Pegado a la pared: al principio y al final de cada tramo (esquinas), en medio y en cada celda.
		auto AddWallCandidates = [&WallRuns, FloorZ](const FVector& Size, TArray<FTransform>& Out)
		{
			for (const FWallRun& Run : WallRuns)
			{
				if (Run.End - Run.Start < Size.X)
				{
					continue;
				}
				TArray<float> Stops = Run.Stops;
				const float HalfWidth = static_cast<float>(Size.X) * 0.5f;
				Stops.Append({ Run.Start + HalfWidth, Run.End - HalfWidth, (Run.Start + Run.End) * 0.5f });
				const float Yaw = GetWallYaw(Run.Out);
				for (const float Stop : Stops)
				{
					if (Stop - Size.X * 0.5f < Run.Start - 0.5f || Stop + Size.X * 0.5f > Run.End + 0.5f)
					{
						continue;
					}
					const FVector2D Center = Run.WallPoint + Run.Along * Stop - FVector2D(Run.Out.X, Run.Out.Y) * (WallInset + Size.Y * 0.5f);
					Out.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Center, FloorZ)));
				}
			}
		};
		// Centro: el centro de la sala y desplazado en pasos, para dejar libre el nodo de patrulla.
		FVector2D Min(UE_BIG_NUMBER), Max(-UE_BIG_NUMBER);
		for (const FIntPoint& Cell : Cells)
		{
			const FVector2D Center(Lot.GetCellCenter(Cell));
			Min = FVector2D::Min(Min, Center);
			Max = FVector2D::Max(Max, Center);
		}
		const FVector2D RoomCenter = (Min + Max) * 0.5f;
		auto AddCenterCandidates = [&RoomCenter, FloorZ](TArray<FTransform>& Out)
		{
			for (const float X : { 0.f, -75.f, 75.f, -150.f, 150.f, -225.f, 225.f, -300.f, 300.f })
			{
				for (const float Y : { 0.f, -75.f, 75.f, -150.f, 150.f, -225.f, 225.f, -300.f, 300.f })
				{
					for (const float Yaw : { 0.f, 90.f })
					{
						Out.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(RoomCenter + FVector2D(X, Y), FloorZ)));
					}
				}
			}
		};
		auto Shuffle = [&Rng](TArray<FTransform>& Candidates)
		{
			for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
			{
				Candidates.Swap(Index, Rng.RandRange(0, Index));
			}
		};
		auto AddDecoration = [this, RoomIndex, &ArtList](int32 ArtIndex, const FTransform& Transform, bool bCollision) -> FNDHouseDecoration&
		{
			FNDHouseDecoration& Decoration = Decorations.AddDefaulted_GetRef();
			Decoration.StableId = Decorations.Num() - 1;
			Decoration.Room = RoomIndex;
			Decoration.Art = ArtIndex;
			Decoration.Kind = bCollision ? ArtList[ArtIndex].Kind : ENDDecorationKind::Rug;
			Decoration.HalfExtent = ArtList[ArtIndex].Size * 0.5f;
			Decoration.LotTransform = Transform;
			Decoration.bCollision = bCollision;
			return Decoration;
		};
		// Algunos muebles son escondites (cuentan como cobertura); la entrada va delante, dentro de la sala.
		auto AddHidingSpot = [this, RoomIndex](int32 DecorationIndex, const FVector& Front)
		{
			const FNDHouseDecoration& Decoration = Decorations[DecorationIndex];
			int32 Capacity = 0;
			ENDHidingSpotKind HideKind = ENDHidingSpotKind::BehindFurniture;
			switch (Decoration.Kind)
			{
			case ENDDecorationKind::Table:
			case ENDDecorationKind::Bed: Capacity = 2; HideKind = ENDHidingSpotKind::UnderFurniture; break;
			case ENDDecorationKind::Wardrobe: Capacity = 1; HideKind = ENDHidingSpotKind::InsideCabinet; break;
			case ENDDecorationKind::Armchair: Capacity = 1; HideKind = ENDHidingSpotKind::BehindFurniture; break;
			default: break;
			}
			const FVector2D Extent = Decoration.GetFootprint().GetExtent();
			const float Reach = FMath::Abs(Front.X) * Extent.X + FMath::Abs(Front.Y) * Extent.Y;
			const FVector Entry = Decoration.LotTransform.GetLocation() + Front * (Reach + 40.f);
			if (Capacity == 0 || FindRoomAt(Entry) != RoomIndex)
			{
				return;
			}
			FNDHouseHidingSpot& Hide = HidingSpots.AddDefaulted_GetRef();
			Hide.StableId = HidingSpots.Num() - 1;
			Hide.Room = RoomIndex;
			Hide.LotTransform = Decoration.LotTransform;
			Hide.Kind = HideKind;
			Hide.Capacity = Capacity;
			Hide.EntryPoint = Entry;
		};

		// Cama de la criatura (dormitorio principal): pegada a la pared, sobre su ancla. Sin colisión hasta
		// que la criatura tenga su arte (el borde de la cama guarda el objetivo de C05).
		if (Room.Def->RoomType == ENDRoomType::R06_MasterBedroom)
		{
			const int32 BedArt = NDHouseFurniture::FindArt(NDHouseFurniture::GetCreatureBedId());
			const FNDHouseLayoutMarker* Anchor = Markers.FindByPredicate([RoomIndex](const FNDHouseLayoutMarker& Marker)
			{
				return Marker.Type == ENDRoomMarkerType::MonsterAnchor && Marker.Room == RoomIndex;
			});
			TArray<FTransform> Candidates;
			if (Anchor && ArtList.IsValidIndex(BedArt))
			{
				AddWallCandidates(ArtList[BedArt].Size, Candidates);
			}
			for (const FTransform& Candidate : Candidates)
			{
				const FBox2D Box = MakeFootprint(Candidate.GetLocation(), Candidate.Rotator().Yaw, ArtList[BedArt].Size);
				const bool bClearsDoors = !Doors.ContainsByPredicate([&Box](const FDoorPoint& Door) { return DistanceToBox(Box, Door.Point) < Door.HalfWidth + DoorClearance; });
				if (Box.IsInside(FVector2D(Anchor->LotTransform.GetLocation())) && FitsInRoom(Box) && bClearsDoors)
				{
					AddDecoration(BedArt, Candidate, false).Kind = ENDDecorationKind::Bed;
					break;
				}
			}
		}

		// Muebles de la sala, por orden de prioridad. Los que comparten malla comparten el máximo.
		const int32 MaxPieces = FMath::Min(5 + 3 * (Cells.Num() - 1), 12);
		int32 Pieces = 0;
		TMap<FString, int32> PlacedByMesh;
		for (const FNDRoomFurniture& Entry : NDHouseFurniture::GetRoomFurniture(Room.Def->RoomType))
		{
			const int32 ArtIndex = NDHouseFurniture::FindArt(Entry.Id);
			if (!ArtList.IsValidIndex(ArtIndex))
			{
				continue;
			}
			const FNDFurnitureArt& Art = ArtList[ArtIndex];
			const bool bFlat = Entry.Placement == ENDFurniturePlacement::Flat;
			for (int32 Copy = 0; Copy < Entry.MaxCount; ++Copy)
			{
				if (PlacedByMesh.FindRef(Art.MeshPath) >= Entry.MaxCount || (!bFlat && Pieces >= MaxPieces) || Rng.FRand() > Entry.Chance)
				{
					break;
				}
				TArray<FTransform> Candidates;
				if (Entry.Placement == ENDFurniturePlacement::Wall)
				{
					AddWallCandidates(Art.Size, Candidates);
				}
				else if (bFlat)
				{
					for (const FIntPoint& Cell : Cells)
					{
						Candidates.Add(FTransform(FRotator(0.f, Rng.RandRange(0, 1) * 90.f, 0.f), Lot.GetCellCenter(Cell) + FVector(0.f, 0.f, FloorZ)));
					}
					Candidates.Add(FTransform(FRotator(0.f, Rng.RandRange(0, 1) * 90.f, 0.f), FVector(RoomCenter, FloorZ)));
				}
				else if (Cells.Num() > 1)
				{
					AddCenterCandidates(Candidates);
				}
				Shuffle(Candidates);
				const FTransform* Chosen = Candidates.FindByPredicate([&](const FTransform& Candidate)
				{
					const FBox2D Box = MakeFootprint(Candidate.GetLocation(), Candidate.Rotator().Yaw, Art.Size);
					if (bFlat)
					{
						// En el suelo: dentro de la sala y sin pisar otra pieza del suelo; las que abultan (mantas, cojines)
						// tampoco se meten en un mueble.
						return FitsInRoom(Box) && (Art.Size.Z <= 30.f || !OverlapsFurniture(Box)) && !Decorations.ContainsByPredicate([&Box, RoomIndex, FloorZ](const FNDHouseDecoration& Other)
						{
							return Other.Room == RoomIndex && !Other.bCollision && FMath::IsNearlyEqual(Other.LotTransform.GetLocation().Z, FloorZ, 1.f)
								&& Box.Intersect(Other.GetFootprint());
						});
					}
					return IsFree(Box);
				});
				if (!Chosen)
				{
					break;
				}
				const FTransform PieceTransform = *Chosen;
				AddDecoration(ArtIndex, PieceTransform, !bFlat);
				++PlacedByMesh.FindOrAdd(Art.MeshPath);
				if (bFlat)
				{
					continue;
				}
				++Pieces;
				const FVector Front = Entry.Placement == ENDFurniturePlacement::Center
					? FVector(Hub - FVector2D(PieceTransform.GetLocation()), 0.f).GetSafeNormal2D()
					: PieceTransform.GetRotation().RotateVector(FVector(0.f, -1.f, 0.f));
				AddHidingSpot(Decorations.Num() - 1, Front);

				// Acompañantes: sillas (con colisión, si caben) y adornos encima (sin colisión).
				for (const FNDFurnitureCompanion& Companion : Art.Companions)
				{
					const int32 CompanionArt = NDHouseFurniture::FindArt(Companion.Id);
					if (!ArtList.IsValidIndex(CompanionArt))
					{
						continue;
					}
					const FTransform Placed = FTransform(FRotator(0.f, Companion.Yaw, 0.f), Companion.Offset) * PieceTransform;
					if (Companion.bDecor)
					{
						AddDecoration(CompanionArt, Placed, false);
					}
					else if (IsFree(MakeFootprint(Placed.GetLocation(), Placed.Rotator().Yaw, ArtList[CompanionArt].Size)))
					{
						AddDecoration(CompanionArt, Placed, true);
						AddHidingSpot(Decorations.Num() - 1, Placed.GetRotation().RotateVector(FVector(0.f, -1.f, 0.f)));
					}
				}
			}
		}

		// Felpudo junto a la puerta de la casa, por dentro.
		const int32 MatArt = NDHouseFurniture::FindArt(TEXT("Doormat"));
		if (RoomIndex == EntryRoom && Room.Sockets.IsValidIndex(EntryExitSocket) && ArtList.IsValidIndex(MatArt))
		{
			const FIntPoint Out = NDHouseGen::DirectionToOffset(Room.Sockets[EntryExitSocket].LotDirection);
			const FVector Door = GetSocketLocation(RoomIndex, EntryExitSocket);
			AddDecoration(MatArt, FTransform(FRotator(0.f, GetWallYaw(Out), 0.f), Door - FVector(Out.X, Out.Y, 0.f) * (WallInset + 60.f)), false);
		}
	}
}
