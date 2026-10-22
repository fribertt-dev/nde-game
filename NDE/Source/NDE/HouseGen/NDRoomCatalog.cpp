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

		// Piezas de la segunda tanda (GDD 14.4).
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Entry_B"), ENDRoomType::R01_Entry, 2, 1);
			AddSocket(Def, TEXT("Exit"), 0, 0, EDir::South, ESock::Outdoor);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddSocket(Def, TEXT("W0"), 0, 0, EDir::West);
			AddSocket(Def, TEXT("E1"), 1, 0, EDir::East);
			AddSocket(Def, TEXT("W1"), 1, 0, EDir::West);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, -60.f, -80.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, -60.f, 80.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, 80.f, -80.f, SpawnZ);
			AddMarker(Def, EMark::PlayerSpawn, 0, 0, 80.f, 80.f, SpawnZ);
			AddMarker(Def, EMark::LootSocket, 1, 0, 120.f, 140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 130.f, -130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Corridor_T"), ENDRoomType::R02_Corridor, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("E"), 0, 0, EDir::East);
			AddSocket(Def, TEXT("W"), 0, 0, EDir::West);
			AddMarker(Def, EMark::HideSpot, 0, 0, 130.f, 130.f);
			Modules.Add(Def);
		}
		{
			// Sala grande en L: rectángulo 3x2 sin la celda (2,1).
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_LivingRoom_L"), ENDRoomType::R03_LivingRoom, 3, 2, 1.5f);
			Def.BlockedCells = { FIntPoint(2, 1) };
			AddSocket(Def, TEXT("S00"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("S01"), 0, 1, EDir::South);
			AddSocket(Def, TEXT("W00"), 0, 0, EDir::West);
			AddSocket(Def, TEXT("E01"), 0, 1, EDir::East);
			AddSocket(Def, TEXT("E11"), 1, 1, EDir::East);
			AddSocket(Def, TEXT("N11"), 1, 1, EDir::North);
			AddSocket(Def, TEXT("N20"), 2, 0, EDir::North);
			AddSocket(Def, TEXT("W20"), 2, 0, EDir::West);
			AddSocket(Def, TEXT("E20"), 2, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, 0.f);
			AddMarker(Def, EMark::LootSocket, 1, 1, 100.f, 100.f);
			AddMarker(Def, EMark::LootSocket, 2, 0, 100.f, -100.f);
			AddMarker(Def, EMark::HideSpot, 0, 1, -130.f, 130.f);
			AddMarker(Def, EMark::HideSpot, 2, 0, 130.f, 130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Study_A"), ENDRoomType::R11_Study, 1, 1, 1.2f);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("E"), 0, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 120.f, 0.f);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, -130.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -120.f, 120.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Laundry_A"), ENDRoomType::R13_Laundry, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("W"), 0, 0, EDir::West);
			AddMarker(Def, EMark::LootSocket, 0, 0, 120.f, 120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, 120.f, -120.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Library_A"), ENDRoomType::R14_Library, 2, 1, 1.2f);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, -140.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 0.f, -140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 130.f, 130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Playroom_A"), ENDRoomType::R15_Playroom, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("W1"), 1, 0, EDir::West);
			AddSocket(Def, TEXT("E1"), 1, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 100.f, 100.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 100.f, -100.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -120.f, -120.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 120.f, 120.f);
			Modules.Add(Def);
		}
		{
			// Despensa: variante pequeña del almacén, con puerta estrecha.
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Pantry_A"), ENDRoomType::R12_Pantry, 1, 1);
			AddSocket(Def, TEXT("S"), 0, 0, EDir::South, ESock::Door_S);
			AddMarker(Def, EMark::LootSocket, 0, 0, 120.f, -120.f);
			AddMarker(Def, EMark::LootSocket, 0, 0, 120.f, 120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, 0.f, 130.f);
			Modules.Add(Def);
		}

		// Pisos: cada escalera son dos salas iguales, una encima de otra (arranque abajo, llegada arriba).
		// Se entra abajo por S0 (pie de la rampa); la llegada tiene una sola puerta. Los huecos Stairs solo
		// marcan la unión vertical. Sube 400 cm (un piso) con tramos de 26,6° como mucho.
		auto MakeStairs = [](const TCHAR* BottomId, const TCHAR* TopId, int32 SizeX, int32 SizeY)
		{
			FNDRoomModuleDefinition Bottom = MakeModule(BottomId, ENDRoomType::R17_Stairwell, SizeX, SizeY);
			Bottom.StairPart = ENDStairPart::Bottom;
			Bottom.StairPairModuleId = FName(TopId);
			FNDRoomModuleDefinition Top = MakeModule(TopId, ENDRoomType::R17_Stairwell, SizeX, SizeY);
			Top.StairPart = ENDStairPart::Top;
			Top.StairPairModuleId = FName(BottomId);
			return TPair<FNDRoomModuleDefinition, FNDRoomModuleDefinition>(Bottom, Top);
		};
		auto AddSegment = [](FNDRoomModuleDefinition& Def, const FVector& Start, const FVector& End, float Width, float Height = 0.f)
		{
			FNDStairSegment& Segment = Def.StairSegments.AddDefaulted_GetRef();
			Segment.Start = Start;
			Segment.End = End;
			Segment.Width = Width;
			Segment.Height = Height;
		};
		{
			// Recta 2x1: una rampa de punta a punta; se sale arriba por el otro extremo (N1). Abajo, N1 es un paso
			// bajo el final de la rampa: ahí va la sala que sostiene la llegada si hace falta (no pasa la criatura).
			TPair<FNDRoomModuleDefinition, FNDRoomModuleDefinition> Stairs = MakeStairs(TEXT("D01_Stairs_Bottom"), TEXT("D01_Stairs_Top"), 2, 1);
			FNDRoomModuleDefinition& Bottom = Stairs.Key;
			AddSocket(Bottom, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Bottom, TEXT("N1"), 1, 0, EDir::North);
			Bottom.DoorSockets.Last().bLowClearance = true;
			AddSocket(Bottom, TEXT("Up"), 1, 0, EDir::West, ESock::Stairs);
			AddSegment(Bottom, FVector(-400.f, 0.f, 0.f), FVector(400.f, 0.f, 400.f), 360.f);
			AddSocket(Stairs.Value, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Stairs.Value, TEXT("Down"), 0, 0, EDir::South, ESock::Stairs);
			Modules.Add(Bottom);
			Modules.Add(Stairs.Value);
		}
		{
			// En U 2x1: dos carriles de 170 cm separados por un murete. Se sube por el carril -Y hasta el rellano
			// del fondo y se vuelve por el carril +Y: la llegada queda sobre la entrada, encima de la sala de la que sale.
			TPair<FNDRoomModuleDefinition, FNDRoomModuleDefinition> Stairs = MakeStairs(TEXT("D01_StairsU_Bottom"), TEXT("D01_StairsU_Top"), 2, 1);
			FNDRoomModuleDefinition& Bottom = Stairs.Key;
			AddSocket(Bottom, TEXT("S0"), 0, 0, EDir::South);
			Bottom.DoorSockets.Last().Offset = 95.f;
			AddSocket(Bottom, TEXT("Up"), 1, 0, EDir::North, ESock::Stairs);
			AddSegment(Bottom, FVector(-400.f, -95.f, 0.f), FVector(200.f, -95.f, 200.f), 170.f);
			AddSegment(Bottom, FVector(200.f, 0.f, 200.f), FVector(400.f, 0.f, 200.f), 360.f);
			AddSegment(Bottom, FVector(200.f, 95.f, 200.f), FVector(-400.f, 95.f, 400.f), 170.f);
			AddSegment(Bottom, FVector(-400.f, 0.f, 0.f), FVector(200.f, 0.f, 0.f), 20.f, 500.f);
			AddSocket(Stairs.Value, TEXT("S0"), 0, 0, EDir::South);
			Stairs.Value.DoorSockets.Last().Offset = -95.f;
			AddSocket(Stairs.Value, TEXT("Down"), 1, 0, EDir::North, ESock::Stairs);
			Modules.Add(Bottom);
			Modules.Add(Stairs.Value);
		}
		{
			// En L (2x2 sin la celda 0,1): tramo por X, rellano y tramo por Y; se sale arriba por el lado de la celda 1,1.
			TPair<FNDRoomModuleDefinition, FNDRoomModuleDefinition> Stairs = MakeStairs(TEXT("D01_StairsL_Bottom"), TEXT("D01_StairsL_Top"), 2, 2);
			FNDRoomModuleDefinition& Bottom = Stairs.Key;
			Bottom.BlockedCells = { FIntPoint(0, 1) };
			Stairs.Value.BlockedCells = { FIntPoint(0, 1) };
			AddSocket(Bottom, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Bottom, TEXT("Up"), 1, 0, EDir::North, ESock::Stairs);
			AddSegment(Bottom, FVector(-400.f, -200.f, 0.f), FVector(0.f, -200.f, 200.f), 360.f);
			AddSegment(Bottom, FVector(0.f, -200.f, 200.f), FVector(400.f, -200.f, 200.f), 360.f);
			AddSegment(Bottom, FVector(200.f, 0.f, 200.f), FVector(200.f, 400.f, 400.f), 360.f);
			AddSocket(Stairs.Value, TEXT("E11"), 1, 1, EDir::East);
			AddSocket(Stairs.Value, TEXT("Down"), 0, 0, EDir::South, ESock::Stairs);
			Modules.Add(Bottom);
			Modules.Add(Stairs.Value);
		}
		{
			// Dormitorio principal alargado: encaja mejor en el piso de arriba.
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_MasterBedroom_B"), ENDRoomType::R06_MasterBedroom, 2, 1, 2.f);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("W1"), 1, 0, EDir::West);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddMarker(Def, EMark::MonsterAnchor, 1, 0);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, 140.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 140.f, -140.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -140.f, -140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 140.f, 140.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_KidsBedroom_A"), ENDRoomType::R10_KidsBedroom, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("E1"), 1, 0, EDir::East);
			AddSocket(Def, TEXT("W0"), 0, 0, EDir::West);
			AddMarker(Def, EMark::LootSocket, 1, 0, 120.f, -120.f);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, 140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 120.f, 120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -130.f, -130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Basement_A"), ENDRoomType::R16_Basement, 2, 2, 1.3f);
			AddSocket(Def, TEXT("S00"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N11"), 1, 1, EDir::North);
			AddSocket(Def, TEXT("W10"), 1, 0, EDir::West);
			AddSocket(Def, TEXT("E01"), 0, 1, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 1, 100.f, 100.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 120.f, -120.f);
			AddMarker(Def, EMark::HideSpot, 0, 0, -130.f, -130.f);
			AddMarker(Def, EMark::HideSpot, 1, 1, 130.f, 130.f);
			Modules.Add(Def);
		}
		{
			FNDRoomModuleDefinition Def = MakeModule(TEXT("D01_Workshop_A"), ENDRoomType::R16_Basement, 2, 1);
			AddSocket(Def, TEXT("S0"), 0, 0, EDir::South);
			AddSocket(Def, TEXT("N1"), 1, 0, EDir::North);
			AddSocket(Def, TEXT("E0"), 0, 0, EDir::East);
			AddMarker(Def, EMark::LootSocket, 0, 0, 0.f, -140.f);
			AddMarker(Def, EMark::LootSocket, 1, 0, 0.f, 140.f);
			AddMarker(Def, EMark::HideSpot, 1, 0, 130.f, -130.f);
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
