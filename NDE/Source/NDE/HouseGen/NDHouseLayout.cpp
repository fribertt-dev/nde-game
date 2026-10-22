// No Despiertes Eso

#include "HouseGen/NDHouseLayout.h"

#include "Core/NDLog.h"
#include "HouseGen/NDRoomCatalog.h"

FNDHouseGenParams FNDHouseGenParams::ForArchetype(FName Archetype, ENDHouseSize Size)
{
	FNDHouseGenParams Params;
	if (Archetype != TEXT("H01"))
	{
		UE_LOG(LogNDHouseGen, Warning, TEXT("Arquetipo %s desconocido; se usan las reglas de H01"), *Archetype.ToString());
	}
	// H01: casa de una planta (GDD 14). El tamaño fija salas, bucles y segunda salida (GDD 15.2).
	Params.HouseArchetype = TEXT("H01");
	Params.HouseSize = Size;
	switch (Size)
	{
	case ENDHouseSize::Medium:
		Params.MinRooms = 7;
		Params.MaxRooms = 11;
		Params.SecondaryExitChance = 0.6f;
		Params.UpperFloorChance = 0.5f;
		Params.BasementChance = 0.3f;
		break;
	case ENDHouseSize::Large:
		// Casa grande: siempre algún bucle y casi siempre dos caminos a la salida.
		Params.MinRooms = 10;
		Params.MaxRooms = 17;
		Params.MinLoops = 1;
		Params.SecondaryExitChance = 0.9f;
		Params.UpperFloorChance = 0.75f;
		Params.BasementChance = 0.4f;
		break;
	default:
		break;
	}
	Params.GrowLotToFit();
	return Params;
}

FIntPoint FNDHouseGenParams::GetMinLotSize(ENDHouseSize Size)
{
	switch (Size)
	{
	case ENDHouseSize::Medium: return FIntPoint(12, 12);
	case ENDHouseSize::Large: return FIntPoint(14, 14);
	default: return FIntPoint(10, 10);
	}
}

bool FNDHouseGenParams::GrowLotToFit()
{
	const FIntPoint MinSize = GetMinLotSize(HouseSize);
	const FIntPoint OldSize = Lot.SizeCells;
	Lot.SizeCells = FIntPoint(FMath::Max(OldSize.X, MinSize.X), FMath::Max(OldSize.Y, MinSize.Y));
	return Lot.SizeCells != OldSize;
}

void FNDHouseGenParams::SetLot(const FNDHouseLotInfo& InLot)
{
	Lot = InLot;
	if (GrowLotToFit())
	{
		UE_LOG(LogNDHouseGen, Log, TEXT("Lote %s: %dx%d celdas no basta para una casa %s; se usa %dx%d"),
			*Lot.LotId.ToString(), InLot.SizeCells.X, InLot.SizeCells.Y,
			*StaticEnum<ENDHouseSize>()->GetNameStringByValue(static_cast<int64>(HouseSize)), Lot.SizeCells.X, Lot.SizeCells.Y);
	}
}

void FNDHouseGenParams::OverrideRoomRange(int32 InMinRooms, int32 InMaxRooms)
{
	constexpr int32 Lowest = 5;
	constexpr int32 Highest = 17;
	if (InMinRooms > 0)
	{
		MinRooms = FMath::Clamp(InMinRooms, Lowest, Highest);
	}
	if (InMaxRooms > 0)
	{
		MaxRooms = FMath::Clamp(InMaxRooms, Lowest, Highest);
	}
	MaxRooms = FMath::Max(MinRooms, MaxRooms);
}

void FNDHouseGenParams::ApplyFloorOptions(ENDFloorOption UpperFloor, ENDFloorOption Basement)
{
	auto Apply = [](ENDFloorOption Option, float& Chance)
	{
		if (Option != ENDFloorOption::Auto)
		{
			Chance = Option == ENDFloorOption::Always ? 1.f : 0.f;
		}
	};
	Apply(UpperFloor, UpperFloorChance);
	Apply(Basement, BasementChance);

	// Piso superior y sótano a la vez piden al menos 10 salas (planta baja mínima + escalera y sala en cada piso).
	constexpr int32 RoomsForBothFloors = 10;
	if (UpperFloor == ENDFloorOption::Always && Basement == ENDFloorOption::Always && MaxRooms < RoomsForBothFloors)
	{
		UE_LOG(LogNDHouseGen, Display, TEXT("Casa %s con piso superior y sotano: %d-%d salas no bastan; se usan %d-%d"),
			*StaticEnum<ENDHouseSize>()->GetNameStringByValue(static_cast<int64>(HouseSize)), MinRooms, MaxRooms,
			FMath::Max(MinRooms, RoomsForBothFloors), FMath::Max(MaxRooms, RoomsForBothFloors));
		MinRooms = FMath::Max(MinRooms, RoomsForBothFloors);
		MaxRooms = FMath::Max(MaxRooms, MinRooms);
	}
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
		Room.Floor = Source.Grid.Z;
		Room.YawSteps = NDHouseGen::YawToSteps(Source.Yaw);
		Room.Size = NDHouseGen::RotateSize(Def->FootprintCells, Room.YawSteps);
		Room.Depth = Source.Depth;
		for (const FNDRoomDoorSocket& DefSocket : Def->DoorSockets)
		{
			FNDHouseLayoutSocket& Socket = Room.Sockets.AddDefaulted_GetRef();
			Socket.SocketId = DefSocket.SocketId;
			Socket.Type = DefSocket.Type;
			Socket.Width = DefSocket.Width;
			Socket.Offset = DefSocket.Offset;
			Socket.bLowClearance = DefSocket.bLowClearance;
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
		FNDHouseLayoutRoom& Room = OutLayout.Rooms[RoomIndex];
		const int32 Existing = Room.Sockets.IndexOfByPredicate([SocketId](const FNDHouseLayoutSocket& S) { return S.SocketId == SocketId; });
		FIntPoint LocalCell;
		ENDCellDirection LocalDirection;
		if (Existing != INDEX_NONE || !NDHouseGen::ParseAutoSocketId(SocketId, LocalCell, LocalDirection)
			|| !Room.Def->ContainsCell(LocalCell) || Room.Def->ContainsCell(LocalCell + NDHouseGen::DirectionToOffset(LocalDirection)))
		{
			return Existing;
		}
		// Puerta "Auto": una Door_M en un borde exterior del módulo que no tenía hueco.
		FNDHouseLayoutSocket& Socket = Room.Sockets.AddDefaulted_GetRef();
		Socket.SocketId = SocketId;
		Socket.Type = ENDRoomSocketType::Door_M;
		Socket.Width = NDHouseGen::GetDefaultSocketWidth(ENDRoomSocketType::Door_M);
		Socket.bAuto = true;
		Socket.LocalCell = LocalCell;
		Socket.LocalDirection = LocalDirection;
		Socket.LotCell = Room.Grid + NDHouseGen::RotateCell(LocalCell, Room.Def->FootprintCells, Room.YawSteps);
		Socket.LotDirection = NDHouseGen::RotateDirection(LocalDirection, Room.YawSteps);
		return Room.Sockets.Num() - 1;
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

	OutLayout.RunSeed = Manifest.RunSeed;
	OutLayout.BuildGameplayPoints();
	return true;
}

FVector FNDHouseLayout::GetRoomCenter(int32 RoomIndex) const
{
	const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
	return FVector(
		(Room.Grid.X + Room.Size.X * 0.5f - Lot.SizeCells.X * 0.5f) * NDHouseGen::CellSize,
		(Room.Grid.Y + Room.Size.Y * 0.5f - Lot.SizeCells.Y * 0.5f) * NDHouseGen::CellSize,
		Room.Floor * NDHouseGen::FloorHeight);
}

FVector FNDHouseLayout::GetSocketLocation(int32 RoomIndex, int32 SocketIndex) const
{
	const FNDHouseLayoutSocket& Socket = Rooms[RoomIndex].Sockets[SocketIndex];
	const FIntPoint Offset = NDHouseGen::DirectionToOffset(Socket.LotDirection);
	return Lot.GetCellCenter(Socket.LotCell) + FVector(Offset.X * NDHouseGen::CellSize * 0.5f, Offset.Y * NDHouseGen::CellSize * 0.5f,
		Rooms[RoomIndex].Floor * NDHouseGen::FloorHeight);
}

void FNDHouseLayout::GetOccupancy(TMap<FIntVector, int32>& OutOccupancy) const
{
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		TArray<FIntPoint> Cells;
		GetRoomCells(RoomIndex, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			OutOccupancy.Add(FIntVector(Cell.X, Cell.Y, Rooms[RoomIndex].Floor), RoomIndex);
		}
	}
}

void FNDHouseLayout::GetFloorRange(int32& OutLowest, int32& OutHighest) const
{
	OutLowest = 0;
	OutHighest = 0;
	for (const FNDHouseLayoutRoom& Room : Rooms)
	{
		OutLowest = FMath::Min(OutLowest, Room.Floor);
		OutHighest = FMath::Max(OutHighest, Room.Floor);
	}
}

void FNDHouseLayout::GetRoomCells(int32 RoomIndex, TArray<FIntPoint>& OutCells) const
{
	const FNDHouseLayoutRoom& Room = Rooms[RoomIndex];
	TArray<FIntPoint> LocalCells;
	Room.Def->GetLocalCells(LocalCells);
	for (const FIntPoint& Local : LocalCells)
	{
		OutCells.Add(Room.Grid + NDHouseGen::RotateCell(Local, Room.Def->FootprintCells, Room.YawSteps));
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

bool FNDHouseLayout::FindRoute(int32 FromRoom, int32 ToRoom, float MinWidth, TArray<int32>* OutRooms, bool bCreature) const
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
			if (Other != INDEX_NONE && Parent[Other] == INDEX_NONE && Link.OpeningWidth >= MinWidth && RoomFits(Other) && !(bCreature && IsLowClearance(Link)))
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

bool FNDHouseLayout::IsLowClearance(const FNDHouseLayoutConnection& Link) const
{
	return Rooms[Link.RoomA].Sockets[Link.SocketA].bLowClearance || Rooms[Link.RoomB].Sockets[Link.SocketB].bLowClearance;
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
		if (Layout.Rooms[0].Floor != NDHouseGen::GroundFloor || Exit.LotDirection != Lot.FrontDirection
			|| !Lot.IsInsideLot(Exit.GetOutsideCell()) || !Lot.IsFrontYard(Exit.GetOutsideCell()))
		{
			OutErrors.Add(TEXT("La puerta de entrada no da a la calle"));
		}
	}

	// Dentro del lote, fuera del jardín delantero y sin solapes en cada piso.
	TMap<FIntVector, int32> Occupancy;
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const int32 Floor = Layout.Rooms[RoomIndex].Floor;
		if (Floor < NDHouseGen::BasementFloor || Floor > NDHouseGen::UpperFloor)
		{
			OutErrors.Add(FString::Printf(TEXT("Sala %d en un piso que no existe (%d)"), RoomIndex, Floor));
		}
		TArray<FIntPoint> Cells;
		Layout.GetRoomCells(RoomIndex, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			if (!Lot.IsBuildable(Cell))
			{
				OutErrors.Add(FString::Printf(TEXT("Sala %d fuera del lote en (%d,%d)"), RoomIndex, Cell.X, Cell.Y));
			}
			const FIntVector Key(Cell.X, Cell.Y, Floor);
			if (const int32* Other = Occupancy.Find(Key))
			{
				OutErrors.Add(FString::Printf(TEXT("Salas %d y %d se solapan en (%d,%d) piso %d"), *Other, RoomIndex, Cell.X, Cell.Y, Floor));
			}
			else
			{
				Occupancy.Add(Key, RoomIndex);
			}
		}
	}

	// Piso superior apoyado y sótano bajo la casa: cada celda fuera de la planta baja tiene planta baja encima o debajo.
	for (const TPair<FIntVector, int32>& Cell : Occupancy)
	{
		if (Cell.Key.Z != NDHouseGen::GroundFloor && !Occupancy.Contains(FIntVector(Cell.Key.X, Cell.Key.Y, NDHouseGen::GroundFloor)))
		{
			OutErrors.Add(FString::Printf(TEXT("Sala %d en el piso %d sin planta baja en (%d,%d)"), Cell.Value, Cell.Key.Z, Cell.Key.X, Cell.Key.Y));
		}
	}

	// Cada puerta une dos huecos enfrentados y compatibles del mismo piso; cada escalera, sus dos mitades.
	for (const FNDHouseLayoutConnection& Link : Layout.Connections)
	{
		const FNDHouseLayoutRoom& RoomA = Layout.Rooms[Link.RoomA];
		const FNDHouseLayoutRoom& RoomB = Layout.Rooms[Link.RoomB];
		const FNDHouseLayoutSocket& A = RoomA.Sockets[Link.SocketA];
		const FNDHouseLayoutSocket& B = RoomB.Sockets[Link.SocketB];
		bool bFits;
		if (A.Type == ENDRoomSocketType::Stairs || B.Type == ENDRoomSocketType::Stairs)
		{
			const FNDHouseLayoutRoom& Lower = RoomA.Floor < RoomB.Floor ? RoomA : RoomB;
			const FNDHouseLayoutRoom& Upper = RoomA.Floor < RoomB.Floor ? RoomB : RoomA;
			bFits = A.Type == B.Type && Upper.Floor == Lower.Floor + 1 && Lower.Grid == Upper.Grid && Lower.YawSteps == Upper.YawSteps
				&& Lower.Def->StairPart == ENDStairPart::Bottom && Upper.Def->StairPart == ENDStairPart::Top;
		}
		else
		{
			bFits = Link.RoomA != Link.RoomB && RoomA.Floor == RoomB.Floor && A.GetOutsideCell() == B.LotCell
				&& B.LotDirection == NDHouseGen::OppositeDirection(A.LotDirection) && NDHouseGen::AreSocketsCompatible(A.Type, B.Type);
		}
		if (!bFits)
		{
			OutErrors.Add(FString::Printf(TEXT("Puerta %d.%s - %d.%s no encaja"), Link.RoomA, *A.SocketId.ToString(), Link.RoomB, *B.SocketId.ToString()));
		}
	}

	// Acceso secundario: solo en planta baja, a patio trasero o lateral libre dentro del lote.
	for (int32 RoomIndex = 1; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		for (const FNDHouseLayoutSocket& Socket : Layout.Rooms[RoomIndex].Sockets)
		{
			const FIntPoint Outside = Socket.GetOutsideCell();
			if (Socket.bExit && (Layout.Rooms[RoomIndex].Floor != NDHouseGen::GroundFloor || !Lot.IsBuildable(Outside)
				|| Occupancy.Contains(FIntVector(Outside.X, Outside.Y, NDHouseGen::GroundFloor)) || Socket.Connection != INDEX_NONE))
			{
				OutErrors.Add(FString::Printf(TEXT("Acceso secundario %d.%s no da a un patio libre"), RoomIndex, *Socket.SocketId.ToString()));
			}
		}
	}

	// Cada escalera completa: sus dos mitades unidas entre sí.
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
		const bool bStairLinked = Room.Sockets.ContainsByPredicate([](const FNDHouseLayoutSocket& S)
		{
			return S.Type == ENDRoomSocketType::Stairs && S.Connection != INDEX_NONE;
		});
		if (Room.Def->StairPart != ENDStairPart::None && !bStairLinked)
		{
			OutErrors.Add(FString::Printf(TEXT("Escalera %d sin su otra mitad"), RoomIndex));
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

	// Escondites: al menos dos, en salas a las que se llega y con la entrada dentro de su sala.
	constexpr int32 MinHidingSpots = 2;
	if (Layout.HidingSpots.Num() < MinHidingSpots)
	{
		OutErrors.Add(FString::Printf(TEXT("Solo %d escondites; se piden %d"), Layout.HidingSpots.Num(), MinHidingSpots));
	}
	for (const FNDHouseHidingSpot& Spot : Layout.HidingSpots)
	{
		if (Depths[Spot.Room] == INDEX_NONE || Layout.FindRoomAt(Spot.EntryPoint) != Spot.Room)
		{
			OutErrors.Add(FString::Printf(TEXT("Escondite %d (sala %d) sin entrada accesible"), Spot.StableId, Spot.Room));
		}
	}

	// Patrulla: un nodo por sala, ninguno aislado, y el recorrido pasa por todos.
	TSet<int32> Patrolled(Layout.PatrolRoute);
	if (Layout.PatrolNodes.Num() != Layout.Rooms.Num() || Patrolled.Num() != Layout.PatrolNodes.Num())
	{
		OutErrors.Add(FString::Printf(TEXT("La patrulla recorre %d de %d salas"), Patrolled.Num(), Layout.Rooms.Num()));
	}
	for (const FNDHousePatrolNode& Node : Layout.PatrolNodes)
	{
		if (Layout.PatrolNodes.Num() > 1 && Node.Neighbors.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("Nodo de patrulla %d aislado (sala %d)"), Node.StableId, Node.Room));
		}
	}

	// Peligros: nunca en la ruta del bulto ni pegados a una puerta.
	TSet<int32> RouteRooms;
	Layout.GetCarryRouteRooms(Params.CarryEnvelopeWidth, RouteRooms);
	for (const FNDHouseHazard& Hazard : Layout.Hazards)
	{
		if (RouteRooms.Contains(Hazard.Room))
		{
			OutErrors.Add(FString::Printf(TEXT("Peligro %d en la ruta del bulto (sala %d)"), Hazard.StableId, Hazard.Room));
		}
		const FNDHouseLayoutRoom& Room = Layout.Rooms[Hazard.Room];
		for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
		{
			if (Room.Sockets[SocketIndex].IsOpen()
				&& FVector::Dist2D(Layout.GetSocketLocation(Hazard.Room, SocketIndex), Hazard.LotTransform.GetLocation()) < FNDHouseLayout::HazardDoorClearance)
			{
				OutErrors.Add(FString::Printf(TEXT("Peligro %d tapa una puerta (sala %d)"), Hazard.StableId, Hazard.Room));
			}
		}
	}

	// Contrato: salas obligatorias, sitio del objetivo accesible, sus accesos y la anchura de la ruta.
	const FNDContractHouseRequirements& Contract = Params.ContractRequirements;
	for (const ENDRoomType Type : Contract.RequiredRooms)
	{
		if (!Layout.Rooms.ContainsByPredicate([Type](const FNDHouseLayoutRoom& Room) { return Room.Def->RoomType == Type; }))
		{
			OutErrors.Add(FString::Printf(TEXT("El contrato pide %s y la casa no la tiene"), *NDHouseGen::GetRoomTypeName(Type)));
		}
	}
	const int32 AnchorRoom = Layout.GetAnchorRoom(Contract);
	if (!Layout.Rooms.IsValidIndex(AnchorRoom) || Depths[AnchorRoom] == INDEX_NONE)
	{
		OutErrors.Add(TEXT("No se llega al sitio del objetivo del contrato"));
	}
	else
	{
		if (Layout.CountAccesses(AnchorRoom) < Contract.MinRoutes)
		{
			OutErrors.Add(FString::Printf(TEXT("El objetivo tiene %d accesos; el contrato pide %d"), Layout.CountAccesses(AnchorRoom), Contract.MinRoutes));
		}
		if (Contract.MinRouteWidth > 0.f && !Layout.FindRoute(Layout.EntryRoom, AnchorRoom, Contract.MinRouteWidth))
		{
			OutErrors.Add(FString::Printf(TEXT("No hay ruta de %.0f cm hasta el objetivo del contrato"), Contract.MinRouteWidth));
		}
	}
	if (Contract.bNeedsSafeRoom)
	{
		const bool bSafeRoom = Layout.HidingSpots.ContainsByPredicate([&Layout](const FNDHouseHidingSpot& Spot)
		{
			return Spot.Room != Layout.EntryRoom && !Layout.Hazards.ContainsByPredicate([&Spot](const FNDHouseHazard& Hazard) { return Hazard.Room == Spot.Room; });
		});
		if (!bSafeRoom)
		{
			OutErrors.Add(TEXT("El contrato pide una sala segura (con escondite y sin peligros)"));
		}
	}

	// La criatura tiene que poder llegar a toda la casa (sótano y piso de arriba incluidos) sin pasos bajos.
	for (int32 RoomIndex = 1; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		if (Depths[RoomIndex] != INDEX_NONE && !Layout.FindRoute(Layout.EntryRoom, RoomIndex, 0.f, nullptr, true))
		{
			OutErrors.Add(FString::Printf(TEXT("La criatura no llega a la sala %d (piso %d)"), RoomIndex, Layout.Rooms[RoomIndex].Floor));
			break;
		}
	}
	return OutErrors.Num() == ErrorsBefore;
}
