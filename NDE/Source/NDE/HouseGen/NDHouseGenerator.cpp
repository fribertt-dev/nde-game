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

	struct FWorkRoom
	{
		const FNDRoomModuleDefinition* Def = nullptr;
		FIntPoint Grid = FIntPoint::ZeroValue;
		int32 YawSteps = 0;
		TArray<bool> SocketUsed;
	};

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
		TMap<FIntPoint, int32> Occupancy;
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

		bool IsAreaFree(const FIntPoint& Grid, const FIntPoint& Size) const
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				for (int32 Y = 0; Y < Size.Y; ++Y)
				{
					const FIntPoint Cell = Grid + FIntPoint(X, Y);
					if (!Params.Lot.IsBuildable(Cell) || Occupancy.Contains(Cell))
					{
						return false;
					}
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

		int32 AddRoom(const FNDRoomModuleDefinition& Def, const FIntPoint& Grid, int32 YawSteps)
		{
			const int32 Index = Rooms.Num();
			FWorkRoom& Room = Rooms.AddDefaulted_GetRef();
			Room.Def = &Def;
			Room.Grid = Grid;
			Room.YawSteps = YawSteps;
			Room.SocketUsed.Init(false, Def.DoorSockets.Num());
			const FIntPoint Size = NDHouseGen::RotateSize(Def.FootprintCells, YawSteps);
			for (int32 X = 0; X < Size.X; ++X)
			{
				for (int32 Y = 0; Y < Size.Y; ++Y)
				{
					Occupancy.Add(Grid + FIntPoint(X, Y), Index);
				}
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
				if (IsAreaFree(Grid, NDHouseGen::RotateSize(Def.FootprintCells, YawSteps)))
				{
					const int32 Index = AddRoom(Def, Grid, YawSteps);
					Rooms[Index].SocketUsed[ExitSocket] = true;
					return true;
				}
			}
			return false;
		}

		/** Pega un módulo nuevo a un hueco libre de Parent. INDEX_NONE si no cabe. */
		int32 Attach(int32 ParentIndex, const FNDRoomModuleDefinition& Def)
		{
			if (!Rooms.IsValidIndex(ParentIndex))
			{
				return INDEX_NONE;
			}
			const FWorkRoom& Parent = Rooms[ParentIndex];
			TArray<FIntPoint> Candidates; // X = hueco del padre, Y = hueco del módulo nuevo
			for (int32 ParentSocket = 0; ParentSocket < Parent.Def->DoorSockets.Num(); ++ParentSocket)
			{
				if (!IsSocketFree(ParentIndex, ParentSocket))
				{
					continue;
				}
				for (int32 NewSocket = 0; NewSocket < Def.DoorSockets.Num(); ++NewSocket)
				{
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
				if (!IsAreaFree(Grid, NDHouseGen::RotateSize(Def.FootprintCells, YawSteps)))
				{
					continue;
				}
				const int32 NewIndex = AddRoom(Def, Grid, YawSteps);
				Connect(ParentIndex, Candidate.X, NewIndex, Candidate.Y, false);
				return NewIndex;
			}
			return INDEX_NONE;
		}

		/** Pega el módulo a cualquier sala menos la entrada, que solo da al distribuidor. */
		int32 AttachAnywhere(const FNDRoomModuleDefinition& Def)
		{
			TArray<int32> Parents;
			for (int32 Index = 1; Index < Rooms.Num(); ++Index)
			{
				Parents.Add(Index);
			}
			Shuffle(Parents);
			for (const int32 Parent : Parents)
			{
				const int32 NewIndex = Attach(Parent, Def);
				if (NewIndex != INDEX_NONE)
				{
					return NewIndex;
				}
			}
			return INDEX_NONE;
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

		/** Bucles: abre huecos enfrentados entre salas vecinas aún no unidas (nunca la entrada). */
		void AddLoops(int32 Count)
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
					const int32* RoomB = Occupancy.Find(Outside);
					if (!RoomB || *RoomB <= RoomA)
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
		}

		/** Acceso secundario: un hueco libre que da a patio trasero o lateral dentro del lote. */
		void PickSecondaryExit()
		{
			TArray<FIntPoint> Candidates; // X = sala, Y = hueco
			for (int32 RoomIndex = 1; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				for (int32 SocketIndex = 0; SocketIndex < Rooms[RoomIndex].Def->DoorSockets.Num(); ++SocketIndex)
				{
					const ENDRoomSocketType Type = Rooms[RoomIndex].Def->DoorSockets[SocketIndex].Type;
					if (!IsSocketFree(RoomIndex, SocketIndex) || !NDHouseGen::IsConnectableSocket(Type) || Type == ENDRoomSocketType::Vent)
					{
						continue;
					}
					const FIntPoint Outside = GetSocketCell(RoomIndex, SocketIndex) + NDHouseGen::DirectionToOffset(GetSocketDirection(RoomIndex, SocketIndex));
					if (Params.Lot.IsBuildable(Outside) && !Occupancy.Contains(Outside))
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
				Room.Grid = FIntVector(Rooms[Index].Grid.X, Rooms[Index].Grid.Y, 0);
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

bool FNDHouseGenerator::TryGenerateAttempt(const FNDHouseGenParams& Params, int32 AttemptSeed, FNDHouseGenerationManifest& OutManifest)
{
	using namespace NDHouseGenerator;

	const FNDRoomModuleDefinition* Entry = FNDRoomCatalog::Find(EntryModuleId);
	const FNDRoomModuleDefinition* Hub = FNDRoomCatalog::Find(HubModuleId);
	const TArray<const FNDRoomModuleDefinition*> Bedrooms = MakePool(Params.District, { ENDRoomType::R06_MasterBedroom }, 1);
	const TArray<const FNDRoomModuleDefinition*> Objectives = MakePool(Params.District, { ENDRoomType::R09_Objective }, 1);
	// Salas de paso: necesitan otro hueco para seguir el camino.
	const TArray<const FNDRoomModuleDefinition*> Intermediates = MakePool(Params.District,
		{ ENDRoomType::R02_Corridor, ENDRoomType::R03_LivingRoom, ENDRoomType::R04_Kitchen, ENDRoomType::R05_DiningRoom, ENDRoomType::R08_Storage }, 2);
	const TArray<const FNDRoomModuleDefinition*> Fillers = MakePool(Params.District,
		{ ENDRoomType::R02_Corridor, ENDRoomType::R03_LivingRoom, ENDRoomType::R04_Kitchen, ENDRoomType::R05_DiningRoom, ENDRoomType::R07_Bathroom, ENDRoomType::R08_Storage }, 1);
	if (!Entry || !Hub || Bedrooms.IsEmpty() || Objectives.IsEmpty() || Intermediates.IsEmpty() || Fillers.IsEmpty())
	{
		UE_LOG(LogNDHouseGen, Error, TEXT("Faltan modulos en el catalogo para el distrito %s"), *Params.District.ToString());
		return false;
	}

	FRandomStream Stream(AttemptSeed);
	FBuilder Builder(Params, &Stream);

	const int32 RoomCount = Params.RoomCount > 0 ? FMath::Clamp(Params.RoomCount, Params.MinRooms, Params.MaxRooms) : Builder.Rand(Params.MinRooms, Params.MaxRooms);
	// Entrada + camino de BedroomDepth salas hasta el dormitorio + al menos una sala más (objetivo).
	const int32 MaxBedroomDepth = FMath::Min(Params.MaxBedroomDepth, RoomCount - 2);
	if (MaxBedroomDepth < Params.MinBedroomDepth)
	{
		return false;
	}
	const int32 BedroomDepth = Builder.Rand(Params.MinBedroomDepth, MaxBedroomDepth);
	// El objetivo cuelga de una sala del camino (distribuidor incluido, dormitorio no).
	const int32 BranchDepth = Builder.Rand(1, BedroomDepth - 1);
	const int32 MinObjectiveDepth = FMath::Max(Params.MinObjectiveDepth, BranchDepth + 1);
	const int32 MaxObjectiveDepth = FMath::Min(Params.MaxObjectiveDepth, BranchDepth + RoomCount - BedroomDepth - 1);
	if (MaxObjectiveDepth < MinObjectiveDepth)
	{
		return false;
	}
	const int32 ObjectiveDepth = Builder.Rand(MinObjectiveDepth, MaxObjectiveDepth);

	if (!Builder.PlaceEntry(*Entry))
	{
		return false;
	}
	TArray<int32> Spine = { Builder.Attach(0, *Hub) };
	for (int32 Depth = 2; Depth < BedroomDepth && Spine.Last() != INDEX_NONE; ++Depth)
	{
		Spine.Add(Builder.Attach(Spine.Last(), *Builder.Pick(Intermediates)));
	}
	Spine.Add(Builder.Attach(Spine.Last(), *Builder.Pick(Bedrooms)));
	if (Spine.Contains(INDEX_NONE))
	{
		return false;
	}

	int32 Previous = Spine[BranchDepth - 1];
	for (int32 Depth = BranchDepth + 1; Depth < ObjectiveDepth && Previous != INDEX_NONE; ++Depth)
	{
		Previous = Builder.Attach(Previous, *Builder.Pick(Intermediates));
	}
	if (Previous == INDEX_NONE || Builder.Attach(Previous, *Builder.Pick(Objectives)) == INDEX_NONE)
	{
		return false;
	}

	while (Builder.Rooms.Num() < RoomCount)
	{
		if (Builder.AttachAnywhere(*Builder.Pick(Fillers)) == INDEX_NONE)
		{
			return false;
		}
	}

	Builder.AddLoops(Builder.Rand(0, Params.MaxLoops));
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
	Builder.WriteManifest(OutManifest);
	OutManifest.bUsedFallback = true;
	return true;
}

bool FNDHouseGenerator::Generate(const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest, TArray<FString>* OutErrors)
{
	TArray<FString> Errors;
	for (int32 Attempt = 0; Attempt < Params.MaxAttempts; ++Attempt)
	{
		// Cada intento es "otra semilla" derivada de la de la partida, así que el resultado sigue siendo determinista.
		const int32 AttemptSeed = static_cast<int32>(HashCombineFast(GetTypeHash(Params.Seed), GetTypeHash(Attempt)));
		FNDHouseGenerationManifest Candidate;
		Errors.Reset();
		if (!TryGenerateAttempt(Params, AttemptSeed, Candidate))
		{
			Errors.Add(TEXT("No cupieron las salas en el lote"));
			continue;
		}

		FNDHouseLayout Layout;
		FString LayoutError;
		if (!FNDHouseLayout::FromManifest(Candidate, Layout, LayoutError))
		{
			Errors.Add(LayoutError);
			continue;
		}
		if (FNDHouseValidator::Validate(Layout, Params, Errors))
		{
			Candidate.Attempts = Attempt + 1;
			OutManifest = MoveTemp(Candidate);
			return true;
		}
	}

	UE_LOG(LogNDHouseGen, Warning, TEXT("Semilla %lld: %d intentos sin casa valida (%s); se usa la casa de respaldo"),
		Params.Seed, Params.MaxAttempts, Errors.Num() > 0 ? *Errors[0] : TEXT("?"));
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
