// No Despiertes Eso

#include "HouseGen/NDHouseGenerator.h"

#include "Core/NDLog.h"
#include "HouseGen/NDRoomCatalog.h"
#include "Math/RandomStream.h"

namespace NDHouseGenerator
{
	const FName EntryModuleId(TEXT("D01_Entry_A"));
	const FName HubModuleId(TEXT("D01_Corridor_Straight"));
	const FName FallbackLivingModuleId(TEXT("D01_LivingRoom_A"));
	const FName FallbackKitchenModuleId(TEXT("D01_Kitchen_A"));
	/** Huecos de las escaleras (ver NDRoomCatalog.cpp). */
	const FName StairsFootSocket(TEXT("S0"));

	struct FWorkRoom
	{
		const FNDRoomModuleDefinition* Def = nullptr;
		FIntPoint Grid = FIntPoint::ZeroValue;
		int32 Floor = NDHouseGen::GroundFloor;
		int32 YawSteps = 0;
		TArray<bool> SocketUsed;
		/** Puertas "Auto" abiertas en esta sala (celda y lado del módulo sin girar). */
		TArray<TPair<FIntPoint, ENDCellDirection>> AutoSockets;
	};

	int32 FindStairsSocket(const FNDRoomModuleDefinition& Def)
	{
		return Def.DoorSockets.IndexOfByPredicate([](const FNDRoomDoorSocket& S) { return S.Type == ENDRoomSocketType::Stairs; });
	}

	int32 CountConnectableSockets(const FNDRoomModuleDefinition& Def)
	{
		return Def.DoorSockets.FilterByPredicate([](const FNDRoomDoorSocket& S) { return NDHouseGen::IsConnectableSocket(S.Type); }).Num();
	}

	/** Estado de un intento. Sin Stream todo se decide por orden (casa de respaldo). */
	class FBuilder
	{
	public:
		FBuilder(const FNDHouseGenParams& InParams, FRandomStream* InStream)
			: Params(InParams), Stream(InStream)
		{
		}

		const FNDHouseGenParams& Params;
		FRandomStream* Stream;
		TArray<FWorkRoom> Rooms;
		/** (X, Y, piso) → sala. */
		TMap<FIntVector, int32> Occupancy;
		TArray<FNDHouseManifestConnection> Links;
		int32 SecondaryExitRoom = INDEX_NONE;
		FName SecondaryExitSocket;

		int32 Rand(int32 Min, int32 Max) const
		{
			return Stream ? Stream->RandRange(Min, Max) : Min;
		}

		template <typename T>
		void Shuffle(TArray<T>& Items) const
		{
			if (!Stream)
			{
				return;
			}
			for (int32 Index = Items.Num() - 1; Index > 0; --Index)
			{
				Items.Swap(Index, Stream->RandRange(0, Index));
			}
		}

		FIntPoint GetSocketCell(int32 RoomIndex, int32 SocketIndex) const
		{
			const FWorkRoom& Room = Rooms[RoomIndex];
			return Room.Grid + NDHouseGen::RotateCell(Room.Def->DoorSockets[SocketIndex].Cell, Room.Def->FootprintCells, Room.YawSteps);
		}

		ENDCellDirection GetSocketDirection(int32 RoomIndex, int32 SocketIndex) const
		{
			const FWorkRoom& Room = Rooms[RoomIndex];
			return NDHouseGen::RotateDirection(Room.Def->DoorSockets[SocketIndex].Direction, Room.YawSteps);
		}

		bool IsSocketFree(int32 RoomIndex, int32 SocketIndex) const
		{
			return !Rooms[RoomIndex].SocketUsed[SocketIndex];
		}

		/** Celdas del lote que ocuparía el módulo colocado en Grid con ese giro. */
		static void GetPlacedCells(const FNDRoomModuleDefinition& Def, const FIntPoint& Grid, int32 YawSteps, TArray<FIntPoint>& OutCells)
		{
			TArray<FIntPoint> LocalCells;
			Def.GetLocalCells(LocalCells);
			for (const FIntPoint& Local : LocalCells)
			{
				OutCells.Add(Grid + NDHouseGen::RotateCell(Local, Def.FootprintCells, YawSteps));
			}
		}

		/**
		 * true si el módulo cabe en ese piso. Fuera de la planta baja además tiene que haber planta baja
		 * en cada celda (piso apoyado, sótano bajo la casa), salvo bRequireSupport = false: la mitad de
		 * escalera que se coloca a la vez que su pareja ya trae su propio apoyo.
		 */
		bool IsAreaFree(const FNDRoomModuleDefinition& Def, const FIntPoint& Grid, int32 YawSteps, int32 Floor = NDHouseGen::GroundFloor, bool bRequireSupport = true) const
		{
			if (Floor < NDHouseGen::BasementFloor || Floor > NDHouseGen::UpperFloor)
			{
				return false;
			}
			TArray<FIntPoint> Cells;
			GetPlacedCells(Def, Grid, YawSteps, Cells);
			for (const FIntPoint& Cell : Cells)
			{
				if (!Params.Lot.IsBuildable(Cell) || Occupancy.Contains(FIntVector(Cell.X, Cell.Y, Floor)))
				{
					return false;
				}
				if (Floor != NDHouseGen::GroundFloor && bRequireSupport && !Occupancy.Contains(FIntVector(Cell.X, Cell.Y, NDHouseGen::GroundFloor)))
				{
					return false;
				}
			}
			return true;
		}

		bool AreConnected(int32 A, int32 B) const
		{
			return Links.ContainsByPredicate([A, B](const FNDHouseManifestConnection& L)
			{
				return (L.RoomA == A && L.RoomB == B) || (L.RoomA == B && L.RoomB == A);
			});
		}

		int32 AddRoom(const FNDRoomModuleDefinition& Def, const FIntPoint& Grid, int32 YawSteps, int32 Floor = NDHouseGen::GroundFloor)
		{
			const int32 Index = Rooms.Num();
			FWorkRoom& Room = Rooms.AddDefaulted_GetRef();
			Room.Def = &Def;
			Room.Grid = Grid;
			Room.Floor = Floor;
			Room.YawSteps = YawSteps;
			Room.SocketUsed.Init(false, Def.DoorSockets.Num());
			TArray<FIntPoint> Cells;
			GetPlacedCells(Def, Grid, YawSteps, Cells);
			for (const FIntPoint& Cell : Cells)
			{
				Occupancy.Add(FIntVector(Cell.X, Cell.Y, Floor), Index);
			}
			return Index;
		}

		void Connect(int32 RoomA, int32 SocketA, int32 RoomB, int32 SocketB, bool bIsLoop)
		{
			Rooms[RoomA].SocketUsed[SocketA] = true;
			Rooms[RoomB].SocketUsed[SocketB] = true;
			FNDHouseManifestConnection& Link = Links.AddDefaulted_GetRef();
			Link.RoomA = RoomA;
			Link.SocketA = Rooms[RoomA].Def->DoorSockets[SocketA].SocketId;
			Link.RoomB = RoomB;
			Link.SocketB = Rooms[RoomB].Def->DoorSockets[SocketB].SocketId;
			Link.bIsLoop = bIsLoop;
		}

		/** Coloca la entrada en la fachada, con su puerta exterior mirando a la calle. */
		bool PlaceEntry(const FNDRoomModuleDefinition& Def)
		{
			const int32 ExitSocket = Def.DoorSockets.IndexOfByPredicate([](const FNDRoomDoorSocket& S) { return S.Type == ENDRoomSocketType::Outdoor; });
			if (ExitSocket == INDEX_NONE)
			{
				return false;
			}
			const FNDRoomDoorSocket& Exit = Def.DoorSockets[ExitSocket];
			const FNDHouseLotInfo& Lot = Params.Lot;
			const int32 YawSteps = (static_cast<int32>(Lot.FrontDirection) - static_cast<int32>(Exit.Direction) + 4) % 4;
			const bool bFrontAlongX = Lot.FrontDirection == ENDCellDirection::North || Lot.FrontDirection == ENDCellDirection::South;
			const int32 FrontLength = bFrontAlongX ? Lot.SizeCells.Y : Lot.SizeCells.X;

			// Posiciones a lo largo de la fachada, de la central hacia fuera; la primera algo al azar.
			TArray<int32> Positions;
			for (int32 T = 0; T < FrontLength; ++T)
			{
				Positions.Add(T);
			}
			const int32 Center = FrontLength / 2;
			Positions.Sort([Center](int32 A, int32 B) { return FMath::Abs(A - Center) < FMath::Abs(B - Center) || (FMath::Abs(A - Center) == FMath::Abs(B - Center) && A < B); });
			const int32 Preferred = FMath::Clamp(Center + Rand(-2, 1), 0, FrontLength - 1);
			Positions.Remove(Preferred);
			Positions.Insert(Preferred, 0);

			for (const int32 T : Positions)
			{
				FIntPoint ExitCell;
				switch (Lot.FrontDirection)
				{
				case ENDCellDirection::North: ExitCell = FIntPoint(Lot.SizeCells.X - 1 - Lot.FrontYardCells, T); break;
				case ENDCellDirection::East: ExitCell = FIntPoint(T, Lot.SizeCells.Y - 1 - Lot.FrontYardCells); break;
				case ENDCellDirection::South: ExitCell = FIntPoint(Lot.FrontYardCells, T); break;
				default: ExitCell = FIntPoint(T, Lot.FrontYardCells); break;
				}
				const FIntPoint Grid = ExitCell - NDHouseGen::RotateCell(Exit.Cell, Def.FootprintCells, YawSteps);
				if (IsAreaFree(Def, Grid, YawSteps))
				{
					const int32 Index = AddRoom(Def, Grid, YawSteps);
					Rooms[Index].SocketUsed[ExitSocket] = true;
					return true;
				}
			}
			return false;
		}

		/**
		 * Pega un módulo nuevo a un hueco libre de Parent, en su mismo piso. INDEX_NONE si no cabe.
		 * Con PairDef también exige sitio para él en las mismas celdas del piso PairFloor (escaleras).
		 */
		int32 Attach(int32 ParentIndex, const FNDRoomModuleDefinition& Def, const FNDRoomModuleDefinition* PairDef = nullptr, int32 PairFloor = 0,
			FName RequiredNewSocket = NAME_None, FName RequiredParentSocket = NAME_None)
		{
			if (!Rooms.IsValidIndex(ParentIndex))
			{
				return INDEX_NONE;
			}
			const FWorkRoom& Parent = Rooms[ParentIndex];
			const int32 Floor = Parent.Floor;
			TArray<FIntPoint> Candidates; // X = hueco del padre, Y = hueco del módulo nuevo
			for (int32 ParentSocket = 0; ParentSocket < Parent.Def->DoorSockets.Num(); ++ParentSocket)
			{
				if (!IsSocketFree(ParentIndex, ParentSocket)
					|| (!RequiredParentSocket.IsNone() && Parent.Def->DoorSockets[ParentSocket].SocketId != RequiredParentSocket))
				{
					continue;
				}
				for (int32 NewSocket = 0; NewSocket < Def.DoorSockets.Num(); ++NewSocket)
				{
					if (!RequiredNewSocket.IsNone() && Def.DoorSockets[NewSocket].SocketId != RequiredNewSocket)
					{
						continue;
					}
					if (NDHouseGen::AreSocketsCompatible(Parent.Def->DoorSockets[ParentSocket].Type, Def.DoorSockets[NewSocket].Type))
					{
						Candidates.Add(FIntPoint(ParentSocket, NewSocket));
					}
				}
			}
			Shuffle(Candidates);

			for (const FIntPoint& Candidate : Candidates)
			{
				const ENDCellDirection ParentDir = GetSocketDirection(ParentIndex, Candidate.X);
				const FIntPoint TargetCell = GetSocketCell(ParentIndex, Candidate.X) + NDHouseGen::DirectionToOffset(ParentDir);
				const ENDCellDirection NeededDir = NDHouseGen::OppositeDirection(ParentDir);
				const FNDRoomDoorSocket& NewSocket = Def.DoorSockets[Candidate.Y];
				const int32 YawSteps = (static_cast<int32>(NeededDir) - static_cast<int32>(NewSocket.Direction) + 4) % 4;
				const FIntPoint Grid = TargetCell - NDHouseGen::RotateCell(NewSocket.Cell, Def.FootprintCells, YawSteps);
				// La mitad de escalera que va con su pareja no necesita apoyo propio: se lo da la otra.
				const bool bRequireSupport = PairDef == nullptr;
				if (!IsAreaFree(Def, Grid, YawSteps, Floor, bRequireSupport)
					|| (PairDef && !IsAreaFree(*PairDef, Grid, YawSteps, PairFloor, false)))
				{
					continue;
				}
				const int32 NewIndex = AddRoom(Def, Grid, YawSteps, Floor);
				Connect(ParentIndex, Candidate.X, NewIndex, Candidate.Y, false);
				return NewIndex;
			}
			return INDEX_NONE;
		}

		/** Copia del estado para deshacer un intento parcial (p. ej. una escalera tras la que no cabe nada). */
		struct FState
		{
			TArray<FWorkRoom> Rooms;
			TMap<FIntVector, int32> Occupancy;
			TArray<FNDHouseManifestConnection> Links;
		};

		FState SaveState() const
		{
			return FState{ Rooms, Occupancy, Links };
		}

		void RestoreState(const FState& State)
		{
			Rooms = State.Rooms;
			Occupancy = State.Occupancy;
			Links = State.Links;
		}

		/** Profundidad en el árbol (puertas desde la entrada), antes de añadir bucles. */
		int32 GetDepth(int32 RoomIndex) const
		{
			TArray<int32> Depths;
			Depths.Init(INDEX_NONE, Rooms.Num());
			TArray<int32> Queue = { 0 };
			Depths[0] = 0;
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				for (const FNDHouseManifestConnection& Link : Links)
				{
					const int32 Other = Link.RoomA == Queue[Head] ? Link.RoomB : (Link.RoomB == Queue[Head] ? Link.RoomA : INDEX_NONE);
					if (Other != INDEX_NONE && Depths[Other] == INDEX_NONE)
					{
						Depths[Other] = Depths[Queue[Head]] + 1;
						Queue.Add(Other);
					}
				}
			}
			return Depths.IsValidIndex(RoomIndex) ? Depths[RoomIndex] : INDEX_NONE;
		}

		/** Arranques de escalera del catálogo para el distrito (recta, en U, en L...), en orden al azar. */
		TArray<const FNDRoomModuleDefinition*> GetStairBottoms() const
		{
			TArray<const FNDRoomModuleDefinition*> Bottoms;
			for (const FNDRoomModuleDefinition& Def : FNDRoomCatalog::GetModules())
			{
				if (Def.StairPart == ENDStairPart::Bottom && Def.IsAllowedIn(Params.District) && FNDRoomCatalog::Find(Def.StairPairModuleId))
				{
					Bottoms.Add(&Def);
				}
			}
			Shuffle(Bottoms);
			return Bottoms;
		}

		/** La única puerta de la llegada de una escalera. */
		static FName GetTopExitSocket(const FNDRoomModuleDefinition& Top)
		{
			const FNDRoomDoorSocket* Exit = Top.DoorSockets.FindByPredicate([](const FNDRoomDoorSocket& S) { return NDHouseGen::IsConnectableSocket(S.Type); });
			return Exit ? Exit->SocketId : NAME_None;
		}

		/** Prueba unas cuantas salas de PickRoom colgadas de Parent (por RequiredParentSocket si se da). */
		bool TryAttachSome(int32 ParentIndex, TFunctionRef<const FNDRoomModuleDefinition*()> PickRoom, FName RequiredParentSocket = NAME_None)
		{
			constexpr int32 RoomTries = 3;
			for (int32 Try = 0; Try < RoomTries; ++Try)
			{
				const FNDRoomModuleDefinition* Room = PickRoom();
				if (Room && Attach(ParentIndex, *Room, nullptr, 0, NAME_None, RequiredParentSocket) != INDEX_NONE)
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Escalera desde alguna sala de la planta baja (con profundidad <= MaxParentDepth) y, al otro lado,
		 * la primera sala del otro piso elegida por PickFarRoom. Prueba sala por sala y tipo de escalera por
		 * tipo (la que quepa), deshaciendo si no cabe. Devuelve la mitad de escalera del otro piso, o INDEX_NONE.
		 */
		int32 AddFloorAccess(bool bGoingUp, int32 MaxParentDepth, TFunctionRef<const FNDRoomModuleDefinition*()> PickFarRoom,
			TFunctionRef<const FNDRoomModuleDefinition*()> PickSupportRoom)
		{
			for (const int32 Parent : GetParents(NDHouseGen::GroundFloor))
			{
				if (Rooms[Parent].Def->StairPart != ENDStairPart::None || GetDepth(Parent) > MaxParentDepth)
				{
					continue;
				}
				for (const FNDRoomModuleDefinition* Bottom : GetStairBottoms())
				{
					const FState Saved = SaveState();
					const int32 FarIndex = AttachStairs(Parent, bGoingUp, *Bottom);
					if (FarIndex == INDEX_NONE)
					{
						RestoreState(Saved);
						continue;
					}
					const int32 BottomIndex = bGoingUp ? FarIndex - 1 : FarIndex;
					const bool bPlaced = bGoingUp
						? (TryAttachSome(FarIndex, PickFarRoom) || AttachWithSupportBelowArrival(BottomIndex, FarIndex, PickFarRoom, PickSupportRoom))
						: AttachBelowFoot(BottomIndex, PickFarRoom, PickSupportRoom);
					if (bPlaced)
					{
						return FarIndex;
					}
					RestoreState(Saved);
				}
			}
			return INDEX_NONE;
		}

		/**
		 * Escalera recta: si a la llegada de arriba no le queda planta baja debajo, pone una sala de planta baja
		 * por el paso bajo el final de la rampa. Esa sala tiene que unirse además a otra sala de planta baja
		 * (la criatura no cabe bajo la rampa); luego cuelga la sala de arriba.
		 */
		bool AttachWithSupportBelowArrival(int32 BottomIndex, int32 TopIndex, TFunctionRef<const FNDRoomModuleDefinition*()> PickFarRoom,
			TFunctionRef<const FNDRoomModuleDefinition*()> PickSupportRoom)
		{
			const int32 UnderRamp = Rooms[BottomIndex].Def->DoorSockets.IndexOfByPredicate([](const FNDRoomDoorSocket& S) { return S.bLowClearance; });
			const FNDRoomModuleDefinition* SupportRoom = UnderRamp != INDEX_NONE ? PickSupportRoom() : nullptr;
			if (!SupportRoom)
			{
				return false;
			}
			const int32 SupportIndex = Attach(BottomIndex, *SupportRoom, nullptr, 0, NAME_None, Rooms[BottomIndex].Def->DoorSockets[UnderRamp].SocketId);
			return SupportIndex != INDEX_NONE && AddLoops(1, SupportIndex) > 0 && TryAttachSome(TopIndex, PickFarRoom);
		}

		/** Pega un módulo a alguna sala de planta baja de forma que ocupe Cell (por una puerta que dé a esa celda). */
		int32 AttachCoveringCell(const FIntPoint& Cell, const FNDRoomModuleDefinition& Def)
		{
			for (int32 RoomIndex = 1; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				if (Rooms[RoomIndex].Floor != NDHouseGen::GroundFloor || Rooms[RoomIndex].Def->StairPart != ENDStairPart::None)
				{
					continue;
				}
				for (int32 SocketIndex = 0; SocketIndex < Rooms[RoomIndex].Def->DoorSockets.Num(); ++SocketIndex)
				{
					const FIntPoint Outside = GetSocketCell(RoomIndex, SocketIndex) + NDHouseGen::DirectionToOffset(GetSocketDirection(RoomIndex, SocketIndex));
					if (Outside == Cell && IsSocketFree(RoomIndex, SocketIndex))
					{
						const int32 NewIndex = Attach(RoomIndex, Def, nullptr, 0, NAME_None, Rooms[RoomIndex].Def->DoorSockets[SocketIndex].SocketId);
						if (NewIndex != INDEX_NONE)
						{
							return NewIndex;
						}
					}
				}
			}
			return INDEX_NONE;
		}

		/**
		 * Primera sala del sótano por el pie de la rampa (la criatura tiene que poder bajar). Si sobre esa celda
		 * no hay planta baja, antes pone una sala de planta baja que la cubra.
		 */
		bool AttachBelowFoot(int32 BottomIndex, TFunctionRef<const FNDRoomModuleDefinition*()> PickRoom, TFunctionRef<const FNDRoomModuleDefinition*()> PickSupportRoom)
		{
			const int32 FootSocket = Rooms[BottomIndex].Def->DoorSockets.IndexOfByPredicate([](const FNDRoomDoorSocket& S) { return S.SocketId == StairsFootSocket; });
			if (FootSocket == INDEX_NONE)
			{
				return false;
			}
			if (TryAttachSome(BottomIndex, PickRoom, StairsFootSocket))
			{
				return true;
			}
			const FIntPoint Outside = GetSocketCell(BottomIndex, FootSocket) + NDHouseGen::DirectionToOffset(GetSocketDirection(BottomIndex, FootSocket));
			const FNDRoomModuleDefinition* SupportRoom = PickSupportRoom();
			return SupportRoom && AttachCoveringCell(Outside, *SupportRoom) != INDEX_NONE && TryAttachSome(BottomIndex, PickRoom, StairsFootSocket);
		}

		/**
		 * Segunda escalera al piso de arriba unida allí al resto (bucle entre pisos): su llegada da directamente
		 * a una sala de arriba o a una sala nueva que se une a otra. true si lo consigue; si no, deja todo igual.
		 */
		bool AddConnectedSecondStairs(TFunctionRef<const FNDRoomModuleDefinition*()> PickUpperRoom, TFunctionRef<const FNDRoomModuleDefinition*()> PickGroundRoom,
			bool bAllowExtraRoom)
		{
			// Une la puerta libre de una mitad de escalera al resto de su piso: directamente con una sala vecina,
			// o con una sala nueva (antes los pasillos, que tienen muchas puertas) que se une a otra.
			auto TryJoin = [this, bAllowExtraRoom](int32 StairIndex, FName RequiredSocket, TFunctionRef<const FNDRoomModuleDefinition*()> PickRoom)
			{
				if (AddLoops(1, StairIndex) > 0 || AddAutoLoop(StairIndex))
				{
					return true;
				}
				if (!bAllowExtraRoom)
				{
					return false;
				}
				const FNDRoomModuleDefinition* Connectors[] = {
					FNDRoomCatalog::Find(HubModuleId),
					FNDRoomCatalog::Find(TEXT("D01_Corridor_T")),
					FNDRoomCatalog::Find(TEXT("D01_Corridor_Corner")),
					PickRoom(),
				};
				for (const FNDRoomModuleDefinition* Connector : Connectors)
				{
					if (!Connector)
					{
						continue;
					}
					const FState Saved = SaveState();
					const int32 RoomIndex = Attach(StairIndex, *Connector, nullptr, 0, NAME_None, RequiredSocket);
					if (RoomIndex != INDEX_NONE && (AddLoops(1, RoomIndex) > 0 || AddAutoLoop(RoomIndex)))
					{
						return true;
					}
					RestoreState(Saved);
				}
				return false;
			};

			// Primero colgada de una sala de arriba (ya unida al resto del piso): basta con unir abajo su pie
			// a la planta baja, que tiene muchas salas.
			for (const int32 Parent : GetParents(NDHouseGen::UpperFloor))
			{
				if (Rooms[Parent].Def->StairPart != ENDStairPart::None)
				{
					continue;
				}
				for (const FNDRoomModuleDefinition* Bottom : GetStairBottoms())
				{
					const FState Saved = SaveState();
					const int32 BottomIndex = AttachStairs(Parent, false, *Bottom);
					if (BottomIndex != INDEX_NONE && TryJoin(BottomIndex, StairsFootSocket, PickGroundRoom))
					{
						return true;
					}
					RestoreState(Saved);
				}
			}
			// Si no, desde la planta baja: la llegada da a una sala de arriba o a una sala nueva que se une a otra.
			for (const int32 Parent : GetParents(NDHouseGen::GroundFloor))
			{
				if (Rooms[Parent].Def->StairPart != ENDStairPart::None)
				{
					continue;
				}
				for (const FNDRoomModuleDefinition* Bottom : GetStairBottoms())
				{
					const FState Saved = SaveState();
					const int32 TopIndex = AttachStairs(Parent, true, *Bottom);
					if (TopIndex != INDEX_NONE && TryJoin(TopIndex, NAME_None, PickUpperRoom))
					{
						return true;
					}
					RestoreState(Saved);
				}
			}
			return false;
		}

		/** Pega el módulo a alguna sala de ese piso con profundidad entre MinDepth y MaxDepth. */
		int32 AttachAtDepth(const FNDRoomModuleDefinition& Def, int32 MinDepth, int32 MaxDepth, int32 Floor = NDHouseGen::GroundFloor)
		{
			for (const int32 Parent : GetParents(Floor))
			{
				const int32 Depth = GetDepth(Parent);
				if (Depth >= MinDepth && Depth <= MaxDepth)
				{
					const int32 NewIndex = Attach(Parent, Def);
					if (NewIndex != INDEX_NONE)
					{
						return NewIndex;
					}
				}
			}
			return INDEX_NONE;
		}

		/** Salas del piso donde se puede colgar algo nuevo (nunca la entrada, que solo da al distribuidor), en orden al azar. */
		TArray<int32> GetParents(int32 Floor) const
		{
			TArray<int32> Parents;
			for (int32 Index = 1; Index < Rooms.Num(); ++Index)
			{
				if (Rooms[Index].Floor == Floor)
				{
					Parents.Add(Index);
				}
			}
			Shuffle(Parents);
			return Parents;
		}

		/** Pega el módulo a cualquier sala de ese piso. */
		int32 AttachAnywhere(const FNDRoomModuleDefinition& Def, int32 Floor = NDHouseGen::GroundFloor)
		{
			for (const int32 Parent : GetParents(Floor))
			{
				const int32 NewIndex = Attach(Parent, Def);
				if (NewIndex != INDEX_NONE)
				{
					return NewIndex;
				}
			}
			return INDEX_NONE;
		}

		/**
		 * Escalera Bottom (con su pareja) colgada de Parent: hacia arriba (arranque en el piso de Parent, se entra
		 * por el pie S0) o hacia abajo (llegada en el piso de Parent, se entra por su puerta). Devuelve la mitad
		 * del otro piso, de donde se cuelgan las salas de ese piso; INDEX_NONE si no cabe.
		 */
		int32 AttachStairs(int32 ParentIndex, bool bGoingUp, const FNDRoomModuleDefinition& Bottom)
		{
			const FNDRoomModuleDefinition* Top = FNDRoomCatalog::Find(Bottom.StairPairModuleId);
			if (!Top || !Rooms.IsValidIndex(ParentIndex))
			{
				return INDEX_NONE;
			}
			const FNDRoomModuleDefinition& Near = bGoingUp ? Bottom : *Top;
			const FNDRoomModuleDefinition& Far = bGoingUp ? *Top : Bottom;
			const int32 FarFloor = Rooms[ParentIndex].Floor + (bGoingUp ? 1 : -1);
			const int32 NearIndex = Attach(ParentIndex, Near, &Far, FarFloor, bGoingUp ? StairsFootSocket : GetTopExitSocket(*Top));
			if (NearIndex == INDEX_NONE)
			{
				return INDEX_NONE;
			}
			const int32 FarIndex = AddRoom(Far, Rooms[NearIndex].Grid, Rooms[NearIndex].YawSteps, FarFloor);
			const int32 BottomIndex = bGoingUp ? NearIndex : FarIndex;
			const int32 TopIndex = bGoingUp ? FarIndex : NearIndex;
			Connect(BottomIndex, FindStairsSocket(Bottom), TopIndex, FindStairsSocket(*Top), false);
			return FarIndex;
		}

		int32 CountRoomsOnFloor(int32 Floor) const
		{
			return Rooms.FilterByPredicate([Floor](const FWorkRoom& R) { return R.Floor == Floor; }).Num();
		}

		/** Elige un módulo del grupo, evitando repetir tipo de sala salvo pasillos y almacenes. */
		const FNDRoomModuleDefinition* Pick(const TArray<const FNDRoomModuleDefinition*>& Pool) const
		{
			TArray<const FNDRoomModuleDefinition*> Fresh = Pool.FilterByPredicate([this](const FNDRoomModuleDefinition* Def)
			{
				const bool bRepeatable = Def->RoomType == ENDRoomType::R02_Corridor || Def->RoomType == ENDRoomType::R08_Storage;
				return bRepeatable || !Rooms.ContainsByPredicate([Def](const FWorkRoom& R) { return R.Def->RoomType == Def->RoomType; });
			});
			const TArray<const FNDRoomModuleDefinition*>& Source = Fresh.Num() > 0 ? Fresh : Pool;
			return Source.Num() > 0 ? Source[Rand(0, Source.Num() - 1)] : nullptr;
		}

		/**
		 * Bucles: abre huecos enfrentados entre salas vecinas del mismo piso aún no unidas (nunca la entrada).
		 * Con OnlyRoom solo prueba puertas de esa sala.
		 */
		int32 AddLoops(int32 Count, int32 OnlyRoom = INDEX_NONE)
		{
			struct FLoopCandidate { int32 RoomA, SocketA, RoomB, SocketB; };
			TArray<FLoopCandidate> Candidates;
			for (int32 RoomA = 1; RoomA < Rooms.Num(); ++RoomA)
			{
				for (int32 SocketA = 0; SocketA < Rooms[RoomA].Def->DoorSockets.Num(); ++SocketA)
				{
					if (!IsSocketFree(RoomA, SocketA))
					{
						continue;
					}
					const ENDCellDirection DirA = GetSocketDirection(RoomA, SocketA);
					const FIntPoint Outside = GetSocketCell(RoomA, SocketA) + NDHouseGen::DirectionToOffset(DirA);
					const int32* RoomB = Occupancy.Find(FIntVector(Outside.X, Outside.Y, Rooms[RoomA].Floor));
					if (!RoomB || *RoomB <= RoomA || (OnlyRoom != INDEX_NONE && RoomA != OnlyRoom && *RoomB != OnlyRoom))
					{
						continue;
					}
					for (int32 SocketB = 0; SocketB < Rooms[*RoomB].Def->DoorSockets.Num(); ++SocketB)
					{
						if (IsSocketFree(*RoomB, SocketB) && GetSocketCell(*RoomB, SocketB) == Outside
							&& GetSocketDirection(*RoomB, SocketB) == NDHouseGen::OppositeDirection(DirA)
							&& NDHouseGen::AreSocketsCompatible(Rooms[RoomA].Def->DoorSockets[SocketA].Type, Rooms[*RoomB].Def->DoorSockets[SocketB].Type))
						{
							Candidates.Add({ RoomA, SocketA, *RoomB, SocketB });
						}
					}
				}
			}
			Shuffle(Candidates);

			int32 Added = 0;
			for (const FLoopCandidate& Candidate : Candidates)
			{
				if (Added >= Count)
				{
					break;
				}
				if (IsSocketFree(Candidate.RoomA, Candidate.SocketA) && IsSocketFree(Candidate.RoomB, Candidate.SocketB)
					&& !AreConnected(Candidate.RoomA, Candidate.RoomB))
				{
					Connect(Candidate.RoomA, Candidate.SocketA, Candidate.RoomB, Candidate.SocketB, true);
					++Added;
				}
			}
			return Added;
		}

		/** Sala donde va el objetivo del contrato (como FNDHouseLayout::GetAnchorRoom). */
		int32 FindContractAnchorRoom() const
		{
			const FNDContractHouseRequirements& Contract = Params.ContractRequirements;
			ENDRoomType Type = ENDRoomType::R09_Objective;
			if (Contract.ObjectiveAnchor == ENDContractAnchor::BedHeadboard)
			{
				Type = ENDRoomType::R06_MasterBedroom;
			}
			else if (Contract.ObjectiveAnchor == ENDContractAnchor::RequiredRoom && Contract.RequiredRooms.Num() > 0)
			{
				Type = Contract.RequiredRooms[0];
			}
			return Rooms.IndexOfByPredicate([Type](const FWorkRoom& Room) { return Room.Def->RoomType == Type; });
		}

		/** Abre bucles hacia la sala del objetivo hasta tener los accesos que pide el contrato. */
		bool AddContractAccesses()
		{
			const int32 Anchor = FindContractAnchorRoom();
			if (Anchor == INDEX_NONE)
			{
				return Params.ContractRequirements.MinRoutes <= 1;
			}
			auto CountAccesses = [this, Anchor]()
			{
				return Links.FilterByPredicate([Anchor](const FNDHouseManifestConnection& Link) { return Link.RoomA == Anchor || Link.RoomB == Anchor; }).Num();
			};
			for (int32 Try = 0; Try < 3 && CountAccesses() < Params.ContractRequirements.MinRoutes; ++Try)
			{
				if (AddLoops(1, Anchor) == 0 && !AddAutoLoop(Anchor))
				{
					break;
				}
			}
			return CountAccesses() >= Params.ContractRequirements.MinRoutes;
		}

		/**
		 * Une RoomIndex con una sala vecina abriendo en esta una puerta "Auto" frente a un hueco libre de RoomIndex,
		 * aunque su módulo no tuviera hueco en esa pared (nunca en la entrada ni en otra escalera). true si lo hace.
		 */
		bool AddAutoLoop(int32 RoomIndex)
		{
			const FWorkRoom& Room = Rooms[RoomIndex];
			for (int32 SocketIndex = 0; SocketIndex < Room.Def->DoorSockets.Num(); ++SocketIndex)
			{
				if (!IsSocketFree(RoomIndex, SocketIndex) || Room.Def->DoorSockets[SocketIndex].Type == ENDRoomSocketType::Vent || Room.Def->DoorSockets[SocketIndex].bLowClearance
					|| !NDHouseGen::IsConnectableSocket(Room.Def->DoorSockets[SocketIndex].Type))
				{
					continue;
				}
				const ENDCellDirection Direction = GetSocketDirection(RoomIndex, SocketIndex);
				const FIntPoint Outside = GetSocketCell(RoomIndex, SocketIndex) + NDHouseGen::DirectionToOffset(Direction);
				const int32* OtherPtr = Occupancy.Find(FIntVector(Outside.X, Outside.Y, Room.Floor));
				if (!OtherPtr || *OtherPtr == 0 || *OtherPtr == RoomIndex || Rooms[*OtherPtr].Def->StairPart != ENDStairPart::None
					|| AreConnected(RoomIndex, *OtherPtr))
				{
					continue;
				}
				FWorkRoom& Other = Rooms[*OtherPtr];
				TArray<FIntPoint> LocalCells;
				Other.Def->GetLocalCells(LocalCells);
				const FIntPoint* LocalCell = LocalCells.FindByPredicate([&](const FIntPoint& Local)
				{
					return Other.Grid + NDHouseGen::RotateCell(Local, Other.Def->FootprintCells, Other.YawSteps) == Outside;
				});
				const ENDCellDirection LocalDirection = NDHouseGen::RotateDirection(NDHouseGen::OppositeDirection(Direction), 4 - Other.YawSteps);
				const bool bModuleHasSocket = LocalCell && Other.Def->DoorSockets.ContainsByPredicate([&](const FNDRoomDoorSocket& S)
				{
					return S.Cell == *LocalCell && S.Direction == LocalDirection;
				});
				if (!LocalCell || bModuleHasSocket || Other.AutoSockets.Contains(TPair<FIntPoint, ENDCellDirection>(*LocalCell, LocalDirection)))
				{
					continue;
				}
				Other.AutoSockets.Add(TPair<FIntPoint, ENDCellDirection>(*LocalCell, LocalDirection));
				Rooms[RoomIndex].SocketUsed[SocketIndex] = true;
				FNDHouseManifestConnection& Link = Links.AddDefaulted_GetRef();
				Link.RoomA = RoomIndex;
				Link.SocketA = Room.Def->DoorSockets[SocketIndex].SocketId;
				Link.RoomB = *OtherPtr;
				Link.SocketB = NDHouseGen::MakeAutoSocketId(*LocalCell, LocalDirection);
				Link.bIsLoop = true;
				return true;
			}
			return false;
		}

		/** Acceso secundario: un hueco libre de la planta baja que da a patio trasero o lateral dentro del lote. */
		void PickSecondaryExit()
		{
			TArray<FIntPoint> Candidates; // X = sala, Y = hueco
			for (int32 RoomIndex = 1; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				if (Rooms[RoomIndex].Floor != NDHouseGen::GroundFloor)
				{
					continue;
				}
				for (int32 SocketIndex = 0; SocketIndex < Rooms[RoomIndex].Def->DoorSockets.Num(); ++SocketIndex)
				{
					const ENDRoomSocketType Type = Rooms[RoomIndex].Def->DoorSockets[SocketIndex].Type;
					if (!IsSocketFree(RoomIndex, SocketIndex) || !NDHouseGen::IsConnectableSocket(Type) || Type == ENDRoomSocketType::Vent)
					{
						continue;
					}
					const FIntPoint Outside = GetSocketCell(RoomIndex, SocketIndex) + NDHouseGen::DirectionToOffset(GetSocketDirection(RoomIndex, SocketIndex));
					if (Params.Lot.IsBuildable(Outside) && !Occupancy.Contains(FIntVector(Outside.X, Outside.Y, NDHouseGen::GroundFloor)))
					{
						Candidates.Add(FIntPoint(RoomIndex, SocketIndex));
					}
				}
			}
			if (Candidates.Num() > 0)
			{
				const FIntPoint Choice = Candidates[Rand(0, Candidates.Num() - 1)];
				Rooms[Choice.X].SocketUsed[Choice.Y] = true;
				SecondaryExitRoom = Choice.X;
				SecondaryExitSocket = Rooms[Choice.X].Def->DoorSockets[Choice.Y].SocketId;
			}
		}

		void WriteManifest(FNDHouseGenerationManifest& Out) const
		{
			Out = FNDHouseGenerationManifest();
			Out.GenerationVersion = NDHouseGen::GenerationVersion;
			Out.RunSeed = Params.Seed;
			Out.ContractId = Params.ContractId;
			Out.HouseArchetype = Params.HouseArchetype;
			Out.District = Params.District;
			Out.HouseSize = Params.HouseSize;
			Out.Lot = Params.Lot;
			Out.Connections = Links;
			Out.SecondaryExitRoom = SecondaryExitRoom;
			Out.SecondaryExitSocket = SecondaryExitSocket;

			// Profundidad = puertas desde la entrada por el camino más corto (los bucles pueden acortarlo).
			TArray<int32> Depths;
			Depths.Init(INDEX_NONE, Rooms.Num());
			TArray<int32> Queue = { 0 };
			Depths[0] = 0;
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				for (const FNDHouseManifestConnection& Link : Links)
				{
					const int32 Other = Link.RoomA == Queue[Head] ? Link.RoomB : (Link.RoomB == Queue[Head] ? Link.RoomA : INDEX_NONE);
					if (Other != INDEX_NONE && Depths[Other] == INDEX_NONE)
					{
						Depths[Other] = Depths[Queue[Head]] + 1;
						Queue.Add(Other);
					}
				}
			}

			for (int32 Index = 0; Index < Rooms.Num(); ++Index)
			{
				FNDHouseManifestRoom& Room = Out.Rooms.AddDefaulted_GetRef();
				Room.InstanceId = Index;
				Room.ModuleId = Rooms[Index].Def->RoomModuleId;
				Room.Grid = FIntVector(Rooms[Index].Grid.X, Rooms[Index].Grid.Y, Rooms[Index].Floor);
				Room.Yaw = Rooms[Index].YawSteps * 90;
				Room.Depth = Depths[Index];
			}
		}
	};

	/** Módulos de los tipos dados, válidos en el distrito y con al menos MinSockets huecos de unión. */
	TArray<const FNDRoomModuleDefinition*> MakePool(FName District, std::initializer_list<ENDRoomType> Types, int32 MinSockets)
	{
		TArray<const FNDRoomModuleDefinition*> Pool;
		for (const FNDRoomModuleDefinition& Def : FNDRoomCatalog::GetModules())
		{
			bool bTypeMatches = false;
			for (const ENDRoomType Type : Types)
			{
				bTypeMatches |= Def.RoomType == Type;
			}
			if (bTypeMatches && Def.IsAllowedIn(District) && CountConnectableSockets(Def) >= MinSockets)
			{
				Pool.Add(&Def);
			}
		}
		return Pool;
	}
}

bool FNDHouseGenerator::TryGenerateAttempt(const FNDHouseGenParams& Params, int32 AttemptSeed, FNDHouseGenerationManifest& OutManifest, FString* OutReason)
{
	using namespace NDHouseGenerator;
	auto Fail = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};

	const TArray<const FNDRoomModuleDefinition*> Entries = MakePool(Params.District, { ENDRoomType::R01_Entry }, 1);
	const FNDRoomModuleDefinition* Hub = FNDRoomCatalog::Find(HubModuleId);
	const TArray<const FNDRoomModuleDefinition*> Bedrooms = MakePool(Params.District, { ENDRoomType::R06_MasterBedroom }, 1);
	const TArray<const FNDRoomModuleDefinition*> Objectives = MakePool(Params.District, { ENDRoomType::R09_Objective }, 1);
	// Salas de paso: necesitan otro hueco para seguir el camino.
	const TArray<const FNDRoomModuleDefinition*> Intermediates = MakePool(Params.District,
		{ ENDRoomType::R02_Corridor, ENDRoomType::R03_LivingRoom, ENDRoomType::R04_Kitchen, ENDRoomType::R05_DiningRoom, ENDRoomType::R08_Storage,
		  ENDRoomType::R11_Study, ENDRoomType::R13_Laundry, ENDRoomType::R14_Library, ENDRoomType::R15_Playroom }, 2);
	const TArray<const FNDRoomModuleDefinition*> Fillers = MakePool(Params.District,
		{ ENDRoomType::R02_Corridor, ENDRoomType::R03_LivingRoom, ENDRoomType::R04_Kitchen, ENDRoomType::R05_DiningRoom, ENDRoomType::R07_Bathroom,
		  ENDRoomType::R08_Storage, ENDRoomType::R10_KidsBedroom, ENDRoomType::R11_Study, ENDRoomType::R12_Pantry, ENDRoomType::R13_Laundry,
		  ENDRoomType::R14_Library, ENDRoomType::R15_Playroom }, 1);
	// Arriba, dormitorios y salas tranquilas; abajo, sótano, taller, almacén y lavandería.
	const TArray<const FNDRoomModuleDefinition*> UpperRooms = MakePool(Params.District,
		{ ENDRoomType::R02_Corridor, ENDRoomType::R07_Bathroom, ENDRoomType::R08_Storage, ENDRoomType::R10_KidsBedroom, ENDRoomType::R11_Study,
		  ENDRoomType::R14_Library, ENDRoomType::R15_Playroom }, 1);
	const TArray<const FNDRoomModuleDefinition*> BasementRooms = MakePool(Params.District,
		{ ENDRoomType::R16_Basement, ENDRoomType::R08_Storage, ENDRoomType::R13_Laundry }, 1);
	if (Entries.IsEmpty() || !Hub || Bedrooms.IsEmpty() || Objectives.IsEmpty() || Intermediates.IsEmpty() || Fillers.IsEmpty()
		|| UpperRooms.IsEmpty() || BasementRooms.IsEmpty())
	{
		UE_LOG(LogNDHouseGen, Error, TEXT("Faltan modulos en el catalogo para el distrito %s"), *Params.District.ToString());
		return false;
	}

	FRandomStream Stream(AttemptSeed);
	FBuilder Builder(Params, &Stream);
	using NDHouseGen::GroundFloor;
	using NDHouseGen::UpperFloor;
	using NDHouseGen::BasementFloor;

	const int32 RoomCount = Builder.Rand(Params.MinRooms, FMath::Max(Params.MinRooms, Params.MaxRooms));
	const FNDRoomModuleDefinition* Entry = Entries[Builder.Rand(0, Entries.Num() - 1)];

	// Pisos extra: cada uno cuesta una escalera (dos salas, una por piso) y al menos una sala más.
	// Las casas grandes con piso superior guardan sitio para una segunda escalera unida arriba.
	constexpr int32 RoomsPerExtraFloor = 3;
	// Segunda escalera: sus dos mitades y una sala más por si hace falta para unirla arriba o abajo.
	constexpr int32 SecondStairsRooms = 3;
	bool bUpper = Stream.FRand() < Params.UpperFloorChance;
	bool bBasement = Stream.FRand() < Params.BasementChance;
	bool bBedroomUpstairs = bUpper && Stream.FRand() < Params.UpstairsBedroomChance;
	bool bSecondStairs = false;
	auto GroundBudget = [&]()
	{
		return RoomCount - (bUpper ? RoomsPerExtraFloor : 0) - (bBasement ? RoomsPerExtraFloor : 0) - (bSecondStairs ? SecondStairsRooms : 0);
	};
	// Planta baja mínima: entrada, distribuidor, objetivo y el dormitorio si no está arriba.
	auto GroundMinimum = [&]() { return bBedroomUpstairs ? 3 : 4; };
	if (bBasement && GroundBudget() < GroundMinimum())
	{
		bBasement = false;
	}
	if (bUpper && GroundBudget() < GroundMinimum())
	{
		bUpper = false;
		bBedroomUpstairs = false;
	}
	// Segunda escalera solo si a la planta baja le siguen quedando salas de sobra.
	bSecondStairs = bUpper && Params.HouseSize == ENDHouseSize::Large && GroundBudget() - SecondStairsRooms >= GroundMinimum();
	// Salas de más para los pisos extra (además de la primera de cada uno), sin dejar la planta baja pelada.
	int32 ExtraFloorRooms = 0;
	if (bUpper || bBasement)
	{
		const int32 Spare = GroundBudget() - 5;
		ExtraFloorRooms = Spare > 0 ? Builder.Rand(0, Spare / 2) : 0;
	}
	auto GroundRooms = [&]() { return GroundBudget() - ExtraFloorRooms; };

	// Camino de planta baja desde el distribuidor. Con el dormitorio abajo, el camino acaba en él.
	// Con el dormitorio arriba, el camino solo lleva al objetivo y la escalera sale luego de una sala poco
	// profunda: sala (<= MaxBedroomDepth - 3) → arranque → llegada → dormitorio.
	constexpr int32 StairsToBedroomDoors = 3;
	int32 SpineLength = 0; // salas del camino (distribuidor incluido), sin el dormitorio
	if (bBedroomUpstairs)
	{
		const int32 MaxLength = FMath::Min(Params.MaxBedroomDepth - StairsToBedroomDoors + 1, GroundRooms() - 2);
		if (MaxLength < 1)
		{
			return Fail(TEXT("camino demasiado corto"));
		}
		SpineLength = Builder.Rand(1, MaxLength);
	}
	else
	{
		int32 MaxBedroomDepth = Params.MaxBedroomDepth;
		while (MaxBedroomDepth >= Params.MinBedroomDepth && 1 + MaxBedroomDepth + 1 > GroundRooms())
		{
			--MaxBedroomDepth;
		}
		if (MaxBedroomDepth < Params.MinBedroomDepth)
		{
			return Fail(TEXT("sin profundidad para el dormitorio"));
		}
		SpineLength = Builder.Rand(Params.MinBedroomDepth, MaxBedroomDepth) - 1;
	}
	const int32 GroundSpineRooms = SpineLength + (bBedroomUpstairs ? 0 : 1);
	// El objetivo cuelga de una sala del camino (distribuidor incluido, dormitorio no).
	const int32 BranchDepth = Builder.Rand(1, SpineLength);
	const int32 MinObjectiveDepth = FMath::Max(Params.MinObjectiveDepth, BranchDepth + 1);
	const int32 MaxObjectiveDepth = FMath::Min(Params.MaxObjectiveDepth, BranchDepth + GroundRooms() - 1 - GroundSpineRooms);
	if (MaxObjectiveDepth < MinObjectiveDepth)
	{
		return Fail(TEXT("sin profundidad para el objetivo"));
	}
	const int32 ObjectiveDepth = Builder.Rand(MinObjectiveDepth, MaxObjectiveDepth);

	if (!Builder.PlaceEntry(*Entry))
	{
		return Fail(TEXT("la entrada no cabe en la fachada"));
	}
	TArray<int32> Spine = { Builder.Attach(0, *Hub) };
	while (Spine.Num() < SpineLength && Spine.Last() != INDEX_NONE)
	{
		Spine.Add(Builder.Attach(Spine.Last(), *Builder.Pick(Intermediates)));
	}
	if (Spine.Contains(INDEX_NONE) || (!bBedroomUpstairs && Builder.Attach(Spine.Last(), *Builder.Pick(Bedrooms)) == INDEX_NONE))
	{
		return Fail(TEXT("el camino o el dormitorio no caben"));
	}

	int32 Previous = Spine[BranchDepth - 1];
	for (int32 Depth = BranchDepth + 1; Depth < ObjectiveDepth && Previous != INDEX_NONE; ++Depth)
	{
		Previous = Builder.Attach(Previous, *Builder.Pick(Intermediates));
	}
	// Si no cabe tras la sala prevista, cualquier otra de la misma profundidad vale.
	if (Previous == INDEX_NONE || Builder.Attach(Previous, *Builder.Pick(Objectives)) == INDEX_NONE)
	{
		if (Builder.AttachAtDepth(*Builder.Pick(Objectives), MinObjectiveDepth - 1, MaxObjectiveDepth - 1) == INDEX_NONE)
		{
			return Fail(TEXT("el objetivo no cabe"));
		}
	}

	// Salas que pide el contrato (p. ej. la cocina en C02), antes del relleno.
	for (const ENDRoomType Type : Params.ContractRequirements.RequiredRooms)
	{
		if (Builder.Rooms.ContainsByPredicate([Type](const FWorkRoom& Room) { return Room.Def->RoomType == Type; }))
		{
			continue;
		}
		const TArray<const FNDRoomModuleDefinition*> Pool = MakePool(Params.District, { Type }, 1);
		if (Pool.IsEmpty() || Builder.AttachAnywhere(*Builder.Pick(Pool)) == INDEX_NONE)
		{
			return Fail(TEXT("una sala del contrato no cabe"));
		}
	}

	// Resto de la planta baja. Los pisos extra van después, con toda la planta baja debajo para apoyarse.
	while (Builder.Rooms.Num() < GroundRooms())
	{
		if (Builder.AttachAnywhere(*Builder.Pick(Fillers)) == INDEX_NONE)
		{
			return Fail(TEXT("la planta baja no cabe"));
		}
	}

	// Piso superior: escalera y, al llegar, el dormitorio (si va arriba) u otra sala.
	if (bUpper)
	{
		auto PickSupport = [&Builder, &Fillers]() { return Builder.Pick(Fillers); };
		bool bUpperPlaced = false;
		if (bBedroomUpstairs)
		{
			auto PickBedroom = [&Builder, &Bedrooms]() { return Builder.Pick(Bedrooms); };
			// Dormitorio justo al llegar arriba...
			bUpperPlaced = Builder.AddFloorAccess(true, Params.MaxBedroomDepth - StairsToBedroomDoors, PickBedroom, PickSupport) != INDEX_NONE;
			bool bBedroomPlaced = bUpperPlaced;
			// ...o una sala más allá: escalera desde el distribuidor, una sala arriba y el dormitorio pegado a ella.
			if (!bUpperPlaced)
			{
				bUpperPlaced = Builder.AddFloorAccess(true, Params.MaxBedroomDepth - StairsToBedroomDoors - 1,
					[&Builder, &UpperRooms]() { return Builder.Pick(UpperRooms); }, PickSupport) != INDEX_NONE;
				bBedroomPlaced = bUpperPlaced
					&& Builder.AttachAtDepth(*Builder.Pick(Bedrooms), Params.MinBedroomDepth - 1, Params.MaxBedroomDepth - 1, UpperFloor) != INDEX_NONE;
			}
			// El dormitorio no cabe arriba: se queda en planta baja a su profundidad.
			if (!bBedroomPlaced && Builder.AttachAtDepth(*Builder.Pick(Bedrooms), Params.MinBedroomDepth - 1, Params.MaxBedroomDepth - 1) == INDEX_NONE)
			{
				return Fail(TEXT("el dormitorio no cabe ni arriba ni abajo"));
			}
		}
		if (!bUpperPlaced && Builder.AddFloorAccess(true, MAX_int32, [&Builder, &UpperRooms]() { return Builder.Pick(UpperRooms); }, PickSupport) == INDEX_NONE)
		{
			return Fail(TEXT("el piso superior no cabe"));
		}
	}
	// Sótano: bajo la planta baja, sin salida a la calle; se baja por una escalera desde la planta baja.
	if (bBasement && Builder.AddFloorAccess(false, MAX_int32, [&Builder, &BasementRooms]() { return Builder.Pick(BasementRooms); }, [&Builder, &Fillers]() { return Builder.Pick(Fillers); }) == INDEX_NONE)
	{
		return Fail(TEXT("el sotano no cabe"));
	}
	// Resto de salas: repartidas entre los pisos extra y, si no caben, en planta baja. Con sitio guardado
	// para la segunda escalera, primero se llena hasta ese hueco, luego va la escalera y después lo que quede.
	const int32 RoomsBeforeSecondStairs = RoomCount - (bSecondStairs ? SecondStairsRooms : 0);
	bool bSecondStairsTried = !bSecondStairs;
	while (Builder.Rooms.Num() < RoomCount)
	{
		if (!bSecondStairsTried && Builder.Rooms.Num() >= RoomsBeforeSecondStairs)
		{
			// Casa grande: segunda escalera al piso de arriba, unida allí al resto (bucle entre pisos).
			bSecondStairsTried = true;
			Builder.AddConnectedSecondStairs([&Builder, &UpperRooms]() { return Builder.Pick(UpperRooms); },
				[&Builder, &Fillers]() { return Builder.Pick(Fillers); }, RoomCount - Builder.Rooms.Num() >= SecondStairsRooms);
			continue;
		}
		const float Roll = Stream.FRand();
		int32 Floor = GroundFloor;
		if (bUpper && bBasement)
		{
			Floor = Roll < 0.6f ? UpperFloor : BasementFloor;
		}
		else if (bUpper || bBasement)
		{
			Floor = bUpper ? UpperFloor : BasementFloor;
		}
		if (Floor != GroundFloor && Builder.AttachAnywhere(*Builder.Pick(Floor == UpperFloor ? UpperRooms : BasementRooms), Floor) != INDEX_NONE)
		{
			continue;
		}
		if (Builder.AttachAnywhere(*Builder.Pick(Fillers)) == INDEX_NONE)
		{
			return Fail(TEXT("una sala extra no cabe"));
		}
	}

	// Casas grandes piden algún bucle: si no hay sitio para él, se prueba otra semilla.
	// Cuentan los bucles que ya hay (p. ej. la segunda escalera unida arriba).
	const int32 MinLoops = FMath::Min(Params.MinLoops, Params.MaxLoops);
	const int32 ExistingLoops = Builder.Links.FilterByPredicate([](const FNDHouseManifestConnection& L) { return L.bIsLoop; }).Num();
	int32 Loops = ExistingLoops + Builder.AddLoops(FMath::Max(0, Builder.Rand(MinLoops, Params.MaxLoops) - ExistingLoops));
	// Sin huecos enfrentados: una puerta "Auto" entre dos salas vecinas.
	for (const int32 RoomIndex : Builder.GetParents(GroundFloor))
	{
		if (Loops >= MinLoops)
		{
			break;
		}
		if (Builder.Rooms[RoomIndex].Def->StairPart == ENDStairPart::None && Builder.AddAutoLoop(RoomIndex))
		{
			++Loops;
		}
	}
	if (Loops < MinLoops)
	{
		return Fail(TEXT("faltan bucles"));
	}
	if (!Builder.AddContractAccesses())
	{
		return Fail(TEXT("faltan accesos al objetivo del contrato"));
	}
	if (Stream.FRand() < Params.SecondaryExitChance)
	{
		Builder.PickSecondaryExit();
	}
	Builder.WriteManifest(OutManifest);
	return true;
}

bool FNDHouseGenerator::BuildFallback(const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest)
{
	using namespace NDHouseGenerator;

	// Entrada, distribuidor y cuatro salas colgadas de él: dormitorio y objetivo a profundidad 2.
	const TArray<const FNDRoomModuleDefinition*> Modules = {
		FNDRoomCatalog::Find(HubModuleId),
		FNDRoomCatalog::Find(TEXT("D01_MasterBedroom_A")),
		FNDRoomCatalog::Find(TEXT("D01_Objective_A")),
		FNDRoomCatalog::Find(FallbackLivingModuleId),
		FNDRoomCatalog::Find(FallbackKitchenModuleId),
	};
	const FNDRoomModuleDefinition* Entry = FNDRoomCatalog::Find(EntryModuleId);
	FBuilder Builder(Params, nullptr);
	if (!Entry || !Builder.PlaceEntry(*Entry))
	{
		return false;
	}
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (!Modules[Index] || Builder.Attach(Index == 0 ? 0 : 1, *Modules[Index]) == INDEX_NONE)
		{
			return false;
		}
	}
	// Casas medianas y grandes: pasillos y almacenes donde quepan hasta llegar al mínimo de salas.
	const TArray<const FNDRoomModuleDefinition*> Fillers = {
		FNDRoomCatalog::Find(HubModuleId),
		FNDRoomCatalog::Find(TEXT("D01_Storage_A")),
	};
	for (int32 Index = 0; Builder.Rooms.Num() < Params.MinRooms; ++Index)
	{
		const FNDRoomModuleDefinition* Filler = Fillers[Index % Fillers.Num()];
		if (!Filler || Builder.AttachAnywhere(*Filler) == INDEX_NONE)
		{
			return false;
		}
	}
	// Accesos que pida el contrato (p. ej. dos puertas al tarro de C06).
	Builder.AddContractAccesses();
	Builder.WriteManifest(OutManifest);
	OutManifest.bUsedFallback = true;
	return true;
}

bool FNDHouseGenerator::Generate(const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest, TArray<FString>* OutErrors)
{
	TArray<FString> Errors;
	// Por qué falló cada intento, para el aviso si hace falta la casa de respaldo.
	TMap<FString, int32> FailureCounts;
	for (int32 Attempt = 0; Attempt < Params.MaxAttempts; ++Attempt)
	{
		// Cada intento es "otra semilla" derivada de la de la partida, así que el resultado sigue siendo determinista.
		const int32 AttemptSeed = static_cast<int32>(HashCombineFast(GetTypeHash(Params.Seed), GetTypeHash(Attempt)));
		FNDHouseGenerationManifest Candidate;
		FString Reason;
		Errors.Reset();
		if (!TryGenerateAttempt(Params, AttemptSeed, Candidate, &Reason))
		{
			Errors.Add(FString::Printf(TEXT("No cupieron las salas en el lote: %s"), *Reason));
			++FailureCounts.FindOrAdd(Errors.Last());
			continue;
		}

		FNDHouseLayout Layout;
		FString LayoutError;
		if (!FNDHouseLayout::FromManifest(Candidate, Layout, LayoutError))
		{
			Errors.Add(LayoutError);
			++FailureCounts.FindOrAdd(LayoutError);
			continue;
		}
		if (FNDHouseValidator::Validate(Layout, Params, Errors))
		{
			Candidate.Attempts = Attempt + 1;
			OutManifest = MoveTemp(Candidate);
			return true;
		}
		++FailureCounts.FindOrAdd(Errors.Num() > 0 ? Errors[0] : FString(TEXT("?")));
	}

	TArray<FString> Summary;
	for (const TPair<FString, int32>& Failure : FailureCounts)
	{
		Summary.Add(FString::Printf(TEXT("%s x%d"), *Failure.Key, Failure.Value));
	}
	UE_LOG(LogNDHouseGen, Warning, TEXT("Semilla %lld: %d intentos sin casa valida (%s); se usa la casa de respaldo"),
		Params.Seed, Params.MaxAttempts, *FString::Join(Summary, TEXT(" | ")));
	if (OutErrors)
	{
		*OutErrors = Errors;
	}

	FNDHouseGenerationManifest Fallback;
	FNDHouseLayout Layout;
	FString LayoutError;
	TArray<FString> FallbackErrors;
	if (!BuildFallback(Params, Fallback) || !FNDHouseLayout::FromManifest(Fallback, Layout, LayoutError)
		|| !FNDHouseValidator::Validate(Layout, Params, FallbackErrors))
	{
		UE_LOG(LogNDHouseGen, Error, TEXT("La casa de respaldo tampoco es valida (%s)"),
			FallbackErrors.Num() > 0 ? *FallbackErrors[0] : *LayoutError);
		if (OutErrors)
		{
			OutErrors->Append(FallbackErrors);
		}
		return false;
	}
	Fallback.Attempts = Params.MaxAttempts;
	OutManifest = MoveTemp(Fallback);
	return true;
}
