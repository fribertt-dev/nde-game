// No Despiertes Eso

#include "HouseGen/NDHouseLayout.h"

#include "Core/NDLog.h"
#include "HouseGen/NDRoomCatalog.h"

FNDHouseGenParams FNDHouseGenParams::ForArchetype(FName Archetype)
{
	FNDHouseGenParams Params;
	if (Archetype != TEXT("H01"))
	{
		UE_LOG(LogNDHouseGen, Warning, TEXT("Arquetipo %s desconocido; se usan las reglas de H01"), *Archetype.ToString());
	}
	// H01: casa pequeña de una planta, 5-8 salas (GDD 14).
	Params.HouseArchetype = TEXT("H01");
	return Params;
}

bool FNDHouseLayout::FromManifest(const FNDHouseGenerationManifest& Manifest, FNDHouseLayout& OutLayout, FString& OutError)
{
	OutLayout = FNDHouseLayout();
	OutLayout.Lot = Manifest.Lot;
	if (Manifest.SchemaVersion != NDHouseGen::SchemaVersion)
	{
		OutError = FString::Printf(TEXT("Manifiesto con schema %d; se esperaba %d"), Manifest.SchemaVersion, NDHouseGen::SchemaVersion);
		return false;
	}

	for (int32 Index = 0; Index < Manifest.Rooms.Num(); ++Index)
	{
		const FNDHouseManifestRoom& Source = Manifest.Rooms[Index];
		const FNDRoomModuleDefinition* Def = FNDRoomCatalog::Find(Source.ModuleId);
		if (Source.InstanceId != Index || !Def)
		{
			OutError = FString::Printf(TEXT("Sala %d: id %d o modulo %s no valido"), Index, Source.InstanceId, *Source.ModuleId.ToString());
			return false;
		}

		FNDHouseLayoutRoom& Room = OutLayout.Rooms.AddDefaulted_GetRef();
		Room.InstanceId = Index;
		Room.Def = Def;
		Room.Grid = FIntPoint(Source.Grid.X, Source.Grid.Y);
		Room.YawSteps = NDHouseGen::YawToSteps(Source.Yaw);
		Room.Size = NDHouseGen::RotateSize(Def->FootprintCells, Room.YawSteps);
		Room.Depth = Source.Depth;
		for (const FNDRoomDoorSocket& DefSocket : Def->DoorSockets)
		{
			FNDHouseLayoutSocket& Socket = Room.Sockets.AddDefaulted_GetRef();
			Socket.SocketId = DefSocket.SocketId;
			Socket.Type = DefSocket.Type;
			Socket.Width = DefSocket.Width;
			Socket.LotCell = Room.Grid + NDHouseGen::RotateCell(DefSocket.Cell, Def->FootprintCells, Room.YawSteps);
			Socket.LotDirection = NDHouseGen::RotateDirection(DefSocket.Direction, Room.YawSteps);
		}

		if (Def->RoomType == ENDRoomType::R06_MasterBedroom && OutLayout.BedroomRoom == INDEX_NONE)
		{
			OutLayout.BedroomRoom = Index;
		}
		if (Def->RoomType == ENDRoomType::R09_Objective && OutLayout.ObjectiveRoom == INDEX_NONE)
		{
			OutLayout.ObjectiveRoom = Index;
		}
	}

	auto FindSocket = [&OutLayout](int32 RoomIndex, FName SocketId) -> int32
	{
		if (!OutLayout.Rooms.IsValidIndex(RoomIndex))
		{
			return INDEX_NONE;
		}
		return OutLayout.Rooms[RoomIndex].Sockets.IndexOfByPredicate([SocketId](const FNDHouseLayoutSocket& S) { return S.SocketId == SocketId; });
	};

	for (const FNDHouseManifestConnection& Source : Manifest.Connections)
	{
		const int32 SocketA = FindSocket(Source.RoomA, Source.SocketA);
		const int32 SocketB = FindSocket(Source.RoomB, Source.SocketB);
		if (SocketA == INDEX_NONE || SocketB == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Conexion %d.%s - %d.%s no valida"), Source.RoomA, *Source.SocketA.ToString(), Source.RoomB, *Source.SocketB.ToString());
			return false;
		}
		FNDHouseLayoutSocket& A = OutLayout.Rooms[Source.RoomA].Sockets[SocketA];
		FNDHouseLayoutSocket& B = OutLayout.Rooms[Source.RoomB].Sockets[SocketB];
		if (A.Connection != INDEX_NONE || B.Connection != INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Hueco usado dos veces en la conexion %d - %d"), Source.RoomA, Source.RoomB);
			return false;
		}

		const int32 LinkIndex = OutLayout.Connections.Num();
		FNDHouseLayoutConnection& Link = OutLayout.Connections.AddDefaulted_GetRef();
		Link.RoomA = Source.RoomA;
		Link.SocketA = SocketA;
		Link.RoomB = Source.RoomB;
		Link.SocketB = SocketB;
		Link.OpeningWidth = FMath::Min(A.Width, B.Width);
		Link.bIsLoop = Source.bIsLoop;
		A.Connection = LinkIndex;
		B.Connection = LinkIndex;
	}

	// La entrada es siempre la sala 0 y sale a la calle por su hueco Outdoor.
	if (OutLayout.Rooms.Num() > 0 && OutLayout.Rooms[0].Def->RoomType == ENDRoomType::R01_Entry)
	{
		OutLayout.EntryRoom = 0;
		OutLayout.EntryExitSocket = OutLayout.Rooms[0].Sockets.IndexOfByPredicate(
			[](const FNDHouseLayoutSocket& S) { return S.Type == ENDRoomSocketType::Outdoor; });
		if (OutLayout.EntryExitSocket != INDEX_NONE)
		{
			OutLayout.Rooms[0].Sockets[OutLayout.EntryExitSocket].bExit = true;
		}
	}
	if (Manifest.SecondaryExitRoom != INDEX_NONE)
	{
		const int32 SocketIndex = FindSocket(Manifest.SecondaryExitRoom, Manifest.SecondaryExitSocket);
		if (SocketIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Acceso secundario %d.%s no valido"), Manifest.SecondaryExitRoom, *Manifest.SecondaryExitSocket.ToString());
			return false;
		}
		OutLayout.Rooms[Manifest.SecondaryExitRoom].Sockets[SocketIndex].bExit = true;
	}

	// Puntos marcados de cada módulo, girados con su sala.
	for (int32 RoomIndex = 0; RoomIndex < OutLayout.Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = OutLayout.Rooms[RoomIndex];
		const FRotator RoomRotation(0.f, Room.YawSteps * 90.f, 0.f);
		const FVector Center = OutLayout.GetRoomCenter(RoomIndex);
		for (const FNDRoomMarker& DefMarker : Room.Def->Markers)
		{
			FNDHouseLayoutMarker& Marker = OutLayout.Markers.AddDefaulted_GetRef();
			Marker.Type = DefMarker.Type;
			Marker.Room = RoomIndex;
			Marker.LotTransform = FTransform(FRotator(0.f, RoomRotation.Yaw + DefMarker.Yaw, 0.f), Center + RoomRotation.RotateVector(DefMarker.LocalPosition));
		}
	}

	// Extracción: en el jardín delantero, justo fuera de la puerta de entrada, mirando a la calle.
	if (OutLayout.EntryExitSocket != INDEX_NONE)
	{
		const FNDHouseLayoutSocket& Exit = OutLayout.Rooms[0].Sockets[OutLayout.EntryExitSocket];
		FNDHouseLayoutMarker& Marker = OutLayout.Markers.AddDefaulted_GetRef();
		Marker.Type = ENDRoomMarkerType::Extraction;
		Marker.Room = 0;
		Marker.LotTransform = FTransform(FRotator(0.f, static_cast<int32>(Exit.LotDirection) * 90.f, 0.f), OutLayout.Lot.GetCellCenter(Exit.GetOutsideCell()));
	}
	return true;
}

FVector FNDHouseLayout::GetRoomCenter(int32 RoomIndex) const
{
	const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
	return FVector(
		(Room.Grid.X + Room.Size.X * 0.5f - Lot.SizeCells.X * 0.5f) * NDHouseGen::CellSize,
		(Room.Grid.Y + Room.Size.Y * 0.5f - Lot.SizeCells.Y * 0.5f) * NDHouseGen::CellSize,
		0.f);
}

FVector FNDHouseLayout::GetSocketLocation(int32 RoomIndex, int32 SocketIndex) const
{
	const FNDHouseLayoutSocket& Socket = Rooms[RoomIndex].Sockets[SocketIndex];
	const FIntPoint Offset = NDHouseGen::DirectionToOffset(Socket.LotDirection);
	return Lot.GetCellCenter(Socket.LotCell) + FVector(Offset.X, Offset.Y, 0.f) * (NDHouseGen::CellSize * 0.5f);
}

void FNDHouseLayout::GetRoomCells(int32 RoomIndex, TArray<FIntPoint>& OutCells) const
{
	const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
	for (int32 X = 0; X < Room.Size.X; ++X)
	{
		for (int32 Y = 0; Y < Room.Size.Y; ++Y)
		{
			OutCells.Add(Room.Grid + FIntPoint(X, Y));
		}
	}
}

void FNDHouseLayout::ComputeDepths(TArray<int32>& OutDepths) const
{
	OutDepths.Init(INDEX_NONE, Rooms.Num());
	if (!Rooms.IsValidIndex(EntryRoom))
	{
		return;
	}
	TArray<int32> Queue = { EntryRoom };
	OutDepths[EntryRoom] = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Current = Queue[Head];
		for (const FNDHouseLayoutConnection& Link : Connections)
		{
			const int32 Other = Link.RoomA == Current ? Link.RoomB : (Link.RoomB == Current ? Link.RoomA : INDEX_NONE);
			if (Other != INDEX_NONE && OutDepths[Other] == INDEX_NONE)
			{
				OutDepths[Other] = OutDepths[Current] + 1;
				Queue.Add(Other);
			}
		}
	}
}

bool FNDHouseLayout::FindRoute(int32 FromRoom, int32 ToRoom, float MinWidth, TArray<int32>* OutRooms) const
{
	if (!Rooms.IsValidIndex(FromRoom) || !Rooms.IsValidIndex(ToRoom))
	{
		return false;
	}
	auto RoomFits = [this, MinWidth](int32 RoomIndex) { return Rooms[RoomIndex].Def->GetMinClearWidth() >= MinWidth; };
	if (!RoomFits(FromRoom) || !RoomFits(ToRoom))
	{
		return false;
	}

	TArray<int32> Parent;
	Parent.Init(INDEX_NONE, Rooms.Num());
	Parent[FromRoom] = FromRoom;
	TArray<int32> Queue = { FromRoom };
	for (int32 Head = 0; Head < Queue.Num() && Parent[ToRoom] == INDEX_NONE; ++Head)
	{
		const int32 Current = Queue[Head];
		for (const FNDHouseLayoutConnection& Link : Connections)
		{
			const int32 Other = Link.RoomA == Current ? Link.RoomB : (Link.RoomB == Current ? Link.RoomA : INDEX_NONE);
			if (Other != INDEX_NONE && Parent[Other] == INDEX_NONE && Link.OpeningWidth >= MinWidth && RoomFits(Other))
			{
				Parent[Other] = Current;
				Queue.Add(Other);
			}
		}
	}
	if (Parent[ToRoom] == INDEX_NONE)
	{
		return false;
	}
	if (OutRooms)
	{
		OutRooms->Reset();
		for (int32 Step = ToRoom; Step != FromRoom; Step = Parent[Step])
		{
			OutRooms->Insert(Step, 0);
		}
		OutRooms->Insert(FromRoom, 0);
	}
	return true;
}

int32 FNDHouseLayout::CountMarkers(ENDRoomMarkerType Type) const
{
	return Markers.FilterByPredicate([Type](const FNDHouseLayoutMarker& M) { return M.Type == Type; }).Num();
}

bool FNDHouseValidator::Validate(const FNDHouseLayout& Layout, const FNDHouseGenParams& Params, TArray<FString>& OutErrors)
{
	const int32 ErrorsBefore = OutErrors.Num();
	const FNDHouseLotInfo& Lot = Layout.Lot;
	if (Layout.Rooms.Num() < Params.MinRooms || Layout.Rooms.Num() > Params.MaxRooms)
	{
		OutErrors.Add(FString::Printf(TEXT("%d salas; se piden %d-%d"), Layout.Rooms.Num(), Params.MinRooms, Params.MaxRooms));
	}

	// Entrada en la fachada: su puerta exterior mira a la calle y da al jardín delantero.
	if (Layout.EntryRoom != 0 || Layout.EntryExitSocket == INDEX_NONE)
	{
		OutErrors.Add(TEXT("La sala 0 no es una entrada con puerta exterior"));
	}
	else
	{
		const FNDHouseLayoutSocket& Exit = Layout.Rooms[0].Sockets[Layout.EntryExitSocket];
		if (Exit.LotDirection != Lot.FrontDirection || !Lot.IsInsideLot(Exit.GetOutsideCell()) || !Lot.IsFrontYard(Exit.GetOutsideCell()))
		{
			OutErrors.Add(TEXT("La puerta de entrada no da a la calle"));
		}
	}

	// Dentro del lote, fuera del jardín delantero y sin solapes.
	TMap<FIntPoint, int32> Occupancy;
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		TArray<FIntPoint> Cells;
		Layout.GetRoomCells(RoomIndex, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			if (!Lot.IsBuildable(Cell))
			{
				OutErrors.Add(FString::Printf(TEXT("Sala %d fuera del lote en (%d,%d)"), RoomIndex, Cell.X, Cell.Y));
			}
			if (const int32* Other = Occupancy.Find(Cell))
			{
				OutErrors.Add(FString::Printf(TEXT("Salas %d y %d se solapan en (%d,%d)"), *Other, RoomIndex, Cell.X, Cell.Y));
			}
			else
			{
				Occupancy.Add(Cell, RoomIndex);
			}
		}
	}

	// Cada puerta une dos huecos enfrentados y compatibles.
	for (const FNDHouseLayoutConnection& Link : Layout.Connections)
	{
		const FNDHouseLayoutSocket& A = Layout.Rooms[Link.RoomA].Sockets[Link.SocketA];
		const FNDHouseLayoutSocket& B = Layout.Rooms[Link.RoomB].Sockets[Link.SocketB];
		if (Link.RoomA == Link.RoomB || A.GetOutsideCell() != B.LotCell || B.LotDirection != NDHouseGen::OppositeDirection(A.LotDirection)
			|| !NDHouseGen::AreSocketsCompatible(A.Type, B.Type))
		{
			OutErrors.Add(FString::Printf(TEXT("Puerta %d.%s - %d.%s no encaja"), Link.RoomA, *A.SocketId.ToString(), Link.RoomB, *B.SocketId.ToString()));
		}
	}

	// Acceso secundario: solo a patio trasero o lateral libre dentro del lote.
	for (int32 RoomIndex = 1; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		for (const FNDHouseLayoutSocket& Socket : Layout.Rooms[RoomIndex].Sockets)
		{
			const FIntPoint Outside = Socket.GetOutsideCell();
			if (Socket.bExit && (!Lot.IsBuildable(Outside) || Occupancy.Contains(Outside) || Socket.Connection != INDEX_NONE))
			{
				OutErrors.Add(FString::Printf(TEXT("Acceso secundario %d.%s no da a un patio libre"), RoomIndex, *Socket.SocketId.ToString()));
			}
		}
	}

	// Conectividad desde la salida y profundidades.
	TArray<int32> Depths;
	Layout.ComputeDepths(Depths);
	for (int32 RoomIndex = 0; RoomIndex < Depths.Num(); ++RoomIndex)
	{
		if (Depths[RoomIndex] == INDEX_NONE)
		{
			OutErrors.Add(FString::Printf(TEXT("Sala %d no se alcanza desde la entrada"), RoomIndex));
		}
		else if (Depths[RoomIndex] != Layout.Rooms[RoomIndex].Depth)
		{
			OutErrors.Add(FString::Printf(TEXT("Sala %d: profundidad %d en el manifiesto, %d real"), RoomIndex, Layout.Rooms[RoomIndex].Depth, Depths[RoomIndex]));
		}
	}

	auto CheckUniqueRoom = [&](ENDRoomType Type, int32 RoomIndex, int32 MinDepth, int32 MaxDepth)
	{
		const int32 Count = Layout.Rooms.FilterByPredicate([Type](const FNDHouseLayoutRoom& R) { return R.Def->RoomType == Type; }).Num();
		if (Count != 1)
		{
			OutErrors.Add(FString::Printf(TEXT("%d salas %s; se pide 1"), Count, *NDHouseGen::GetRoomTypeName(Type)));
			return;
		}
		const int32 Depth = Depths[RoomIndex];
		if (Depth < MinDepth || Depth > MaxDepth)
		{
			OutErrors.Add(FString::Printf(TEXT("%s a profundidad %d; se pide %d-%d"), *NDHouseGen::GetRoomTypeName(Type), Depth, MinDepth, MaxDepth));
		}
	};
	CheckUniqueRoom(ENDRoomType::R06_MasterBedroom, Layout.BedroomRoom, Params.MinBedroomDepth, Params.MaxBedroomDepth);
	CheckUniqueRoom(ENDRoomType::R09_Objective, Layout.ObjectiveRoom, Params.MinObjectiveDepth, Params.MaxObjectiveDepth);

	// Puntos marcados.
	if (Layout.CountMarkers(ENDRoomMarkerType::LootSocket) < Params.MinLootSockets)
	{
		OutErrors.Add(FString::Printf(TEXT("Solo %d huecos de botin; se piden %d"), Layout.CountMarkers(ENDRoomMarkerType::LootSocket), Params.MinLootSockets));
	}
	if (Layout.CountMarkers(ENDRoomMarkerType::PlayerSpawn) < 1 || Layout.CountMarkers(ENDRoomMarkerType::Extraction) != 1)
	{
		OutErrors.Add(TEXT("Faltan spawns de jugador o el punto de extraccion"));
	}
	if (!Layout.Markers.ContainsByPredicate([&Layout](const FNDHouseLayoutMarker& M) { return M.Type == ENDRoomMarkerType::MonsterAnchor && M.Room == Layout.BedroomRoom; }))
	{
		OutErrors.Add(TEXT("El dormitorio no tiene anchor de criatura"));
	}

	// Ruta del bulto: de la salida al objetivo sin pasar por huecos más estrechos que el bulto.
	if (Layout.ObjectiveRoom != INDEX_NONE && !Layout.FindRoute(Layout.EntryRoom, Layout.ObjectiveRoom, Params.CarryEnvelopeWidth))
	{
		OutErrors.Add(FString::Printf(TEXT("No hay ruta de bulto de %.0f cm entre la entrada y el objetivo"), Params.CarryEnvelopeWidth));
	}
	return OutErrors.Num() == ErrorsBefore;
}
