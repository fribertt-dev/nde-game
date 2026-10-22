// No Despiertes Eso

#include "HouseGen/NDRoomCatalog.h"

namespace NDRoomCatalog
{
	using EDir = ENDCellDirection;
	using ESock = ENDRoomSocketType;
	using EMark = ENDRoomMarkerType;

	FNDRoomModuleDefinition MakeModule(const TCHAR* Id, ENDRoomType Type, int32 SizeX, int32 SizeY, float Difficulty = 1.f)
	{
		FNDRoomModuleDefinition Def;
		Def.RoomModuleId = FName(Id);
		Def.RoomType = Type;
		Def.FootprintCells = FIntPoint(SizeX, SizeY);
		Def.AllowedDistricts = { FName(TEXT("D01")) };
		Def.DifficultyWeight = Difficulty;
		return Def;
	}

	void AddSocket(FNDRoomModuleDefinition& Def, const TCHAR* Id, int32 X, int32 Y, EDir Dir, ESock Type = ESock::Door_M)
	{
		FNDRoomDoorSocket& Socket = Def.DoorSockets.AddDefaulted_GetRef();
		Socket.SocketId = FName(Id);
		Socket.Cell = FIntPoint(X, Y);
		Socket.Direction = Dir;
		Socket.Type = Type;
		Socket.Width = NDHouseGen::GetDefaultSocketWidth(Type);
	}

	/** Marca en la celda X,Y del módulo, desplazada Offset cm desde el centro de esa celda. */
	void AddMarker(FNDRoomModuleDefinition& Def, EMark Type, int32 X, int32 Y, float OffsetX = 0.f, float OffsetY = 0.f, float Z = 0.f)
	{
		const float Cell = NDHouseGen::CellSize;
		FNDRoomMarker& Marker = Def.Markers.AddDefaulted_GetRef();
		Marker.Type = Type;
		Marker.LocalPosition = FVector(
			(X + 0.5f) * Cell - Def.FootprintCells.X * Cell * 0.5f + OffsetX,
			(Y + 0.5f) * Cell - Def.FootprintCells.Y * Cell * 0.5f + OffsetY,
			Z);
	}

	TArray<FNDRoomModuleDefinition> BuildD01()
	{
		TArray<FNDRoomModuleDefinition> Modules;
		constexpr float SpawnZ = 60.f;

		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Entry_A"), ENDRoomType::R01_Entry, 1, 1);
			AddSocket(Def, TEXT("Exit"), 0, 0, EDir::South, ESock::Outdoor);
			AddSocket(Def, TEXT("N"), 0, 0, EDir::North);
			AddSocket(Def, TEXT("E"), 0, 0, EDir::East);
			AddSocket(Def, TEXT("W"), 0, 0, EDir::West);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, -70.f, -70.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, -70.f, 70.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, 70.f, -70.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, 70.f, 70.f, SpawnZ);
			AddMarker(Def, EMark::HideSpot, 0, 0, 130.f, 130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Corridor_Straight"), ENDRoomType::R02_Corridor, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddSocket(Def, TEXT("W0"), 0, 0, EDir::West);
			AddSocket(Def, TEXT("E1"), 1, 0, EDir::East);
			AddSocket(Def, TEXT("W1"), 1, 0, EDir::West);
			AddMarker(Def, EMark::LootSocket, 1, 0, 100.f, -140.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -100.f, 140.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Corridor_Corner"), ENDRoomType::R02_Corridor, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("E"), 0, 0, EDir::East);
			AddMarker(Def, EMark::HideSpot, 0, 0, 120.f, -120.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_LivingRoom_A"), ENDRoomType::R03_LivingRoom, 2, 2);
			AddSocket(Def, TEXT("S00"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("S01"), 0, 1, EDir::South);
			AddSocket(Def, TEXT("N10"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("N11"), 1, 1, EDir::North);
			AddSocket(Def, TEXT("W00"), 0, 0, EDir::West);
			AddSocket(Def, TEXT("W10"), 1, 0, EDir::West);
			AddSocket(Def, TEXT("E01"), 0, 1, EDir::East);
			AddSocket(Def, TEXT("E11"), 1, 1, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0);
			AddMarker(Def, EMark::LootSocket, 1, 1, 100.f, 100.f);
			AddMarker(Def, EMark::HideSpot, 0, 1, -120.f, 120.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 120.f, -120.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Kitchen_A"), ENDRoomType::R04_Kitchen, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddSocket(Def, TEXT("W1"), 1, 0, EDir::West);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, -140.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 0.f, 140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 140.f, -140.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_DiningRoom_A"), ENDRoomType::R05_DiningRoom, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E1"), 1, 0, EDir::East);
			AddSocket(Def, TEXT("W0"), 0, 0, EDir::West);
			AddMarker(Def, EMark::LootSocket, 0, 0);
			AddMarker(Def, EMark::LootSocket, 1, 0);
			AddMarker(Def, EMark::HideSpot, 1, 0, 140.f, 140.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_MasterBedroom_A"), ENDRoomType::R06_MasterBedroom, 2, 2, 2.f);
			AddSocket(Def, TEXT("S00"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("W10"), 1, 0, EDir::West);
			AddMarker(Def, EMark::MonsterAnchor, 1, 1);
			AddMarker(Def, EMark::LootSocket, 0, 1, 100.f, 100.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 140.f, -100.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -140.f, -140.f);
			AddMarker(Def, EMark::HideSpot, 1, 1, 140.f, 140.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Bathroom_A"), ENDRoomType::R07_Bathroom, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South, ESock::Door_S);
			AddMarker(Def, EMark::LootSocket, 0, 0, 100.f, 100.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, 120.f, -120.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Storage_A"), ENDRoomType::R08_Storage, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N"), 0, 0, EDir::North);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, 120.f);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, -120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, 100.f, 100.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Objective_A"), ENDRoomType::R09_Objective, 1, 1, 1.5f);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("E"), 0, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 100.f, 0.f);
			AddMarker(Def, EMark::LootSocket, 0, 0, -50.f, -120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -120.f, 120.f);
			Modules.Add(Def);
		}
		return Modules;
	}
}

const TArray<FNDRoomModuleDefinition>& FNDRoomCatalog::GetModules()
{
	static const TArray<FNDRoomModuleDefinition> Modules = NDRoomCatalog::BuildD01();
	return Modules;
}

const FNDRoomModuleDefinition* FNDRoomCatalog::Find(FName RoomModuleId)
{
	return GetModules().FindByPredicate([RoomModuleId](const FNDRoomModuleDefinition& Def) { return Def.RoomModuleId == RoomModuleId; });
}

void FNDRoomCatalog::GetModulesOfType(ENDRoomType Type, FName District, TArray<const FNDRoomModuleDefinition*>& OutModules)
{
	for (const FNDRoomModuleDefinition& Def : GetModules())
	{
		if (Def.RoomType == Type && Def.IsAllowedIn(District))
		{
			OutModules.Add(&Def);
		}
	}
}
