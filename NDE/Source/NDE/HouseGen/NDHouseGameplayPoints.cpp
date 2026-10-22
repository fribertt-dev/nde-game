// No Despiertes Eso
// Puntos de juego de una casa resuelta: escondites, peligros, patrulla de la criatura y grafo de sonido.

#include "HouseGen/NDHouseLayout.h"

namespace NDHouseGameplay
{
	/** Ancho del bulto por defecto (FNDHouseGenParams::CarryEnvelopeWidth). */
	constexpr float DefaultCarryWidth = 120.f;
	/** Distancia del escondite a su punto de entrada. */
	constexpr float HidingEntryDistance = 70.f;
	/** Pasos anchos: a partir de aquí el sonido casi no se frena. */
	constexpr float WideOpeningWidth = 300.f;

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
		// La criatura no pasa por los pasos bajos.
		if (!IsLowClearance(Link))
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
				Doors.Add(GetSocketLocation(RoomIndex, SocketIndex));
			}
		}
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		const ENDHouseHazardType Type = GetHazardType(Room.Def->RoomType, Rng);
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FIntPoint Cell = Cells[Rng.RandRange(0, Cells.Num() - 1)];
			const FVector Location = Lot.GetCellCenter(Cell) + FVector(Rng.FRandRange(-100.f, 100.f), Rng.FRandRange(-100.f, 100.f), Room.Floor * NDHouseGen::FloorHeight);
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
}
