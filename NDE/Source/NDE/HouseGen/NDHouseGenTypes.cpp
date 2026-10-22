// No Despiertes Eso

#include "HouseGen/NDHouseGenTypes.h"

FString FNDHouseGenerationManifest::ToDebugString() const
{
	FString Out = FString::Printf(TEXT("schema=%d gen=%s seed=%lld contract=%s house=%s district=%s attempts=%d fallback=%d\n"),
		SchemaVersion, *GenerationVersion, RunSeed, *ContractId.ToString(), *HouseArchetype.ToString(), *District.ToString(),
		Attempts, bUsedFallback ? 1 : 0);
	Out += FString::Printf(TEXT("  lot %s size=(%d,%d) front=%d yard=%d exit2=%d.%s\n"),
		*Lot.LotId.ToString(), Lot.SizeCells.X, Lot.SizeCells.Y, static_cast<int32>(Lot.FrontDirection), Lot.FrontYardCells,
		SecondaryExitRoom, *SecondaryExitSocket.ToString());
	for (const FNDHouseManifestRoom& Room : Rooms)
	{
		Out += FString::Printf(TEXT("  room %d %s grid=(%d,%d,%d) yaw=%d depth=%d\n"),
			Room.InstanceId, *Room.ModuleId.ToString(), Room.Grid.X, Room.Grid.Y, Room.Grid.Z, Room.Yaw, Room.Depth);
	}
	for (const FNDHouseManifestConnection& Link : Connections)
	{
		Out += FString::Printf(TEXT("  link %d.%s <-> %d.%s%s\n"),
			Link.RoomA, *Link.SocketA.ToString(), Link.RoomB, *Link.SocketB.ToString(), Link.bIsLoop ? TEXT(" loop") : TEXT(""));
	}
	return Out;
}

namespace NDHouseGen
{
	FIntPoint DirectionToOffset(ENDCellDirection Direction)
	{
		switch (Direction)
		{
		case ENDCellDirection::North: return FIntPoint(1, 0);
		case ENDCellDirection::East: return FIntPoint(0, 1);
		case ENDCellDirection::South: return FIntPoint(-1, 0);
		default: return FIntPoint(0, -1);
		}
	}

	ENDCellDirection RotateDirection(ENDCellDirection Direction, int32 YawSteps)
	{
		return static_cast<ENDCellDirection>((static_cast<int32>(Direction) + (YawSteps & 3)) & 3);
	}

	ENDCellDirection OppositeDirection(ENDCellDirection Direction)
	{
		return RotateDirection(Direction, 2);
	}

	FIntPoint RotateCell(const FIntPoint& Cell, const FIntPoint& Size, int32 YawSteps)
	{
		// Girar +90° lleva +X hacia +Y, igual que el yaw de Unreal.
		switch (YawSteps & 3)
		{
		case 1: return FIntPoint(Size.Y - 1 - Cell.Y, Cell.X);
		case 2: return FIntPoint(Size.X - 1 - Cell.X, Size.Y - 1 - Cell.Y);
		case 3: return FIntPoint(Cell.Y, Size.X - 1 - Cell.X);
		default: return Cell;
		}
	}

	FIntPoint RotateSize(const FIntPoint& Size, int32 YawSteps)
	{
		return (YawSteps & 1) ? FIntPoint(Size.Y, Size.X) : Size;
	}

	int32 YawToSteps(int32 YawDegrees)
	{
		return ((YawDegrees / 90) % 4 + 4) % 4;
	}

	float GetDefaultSocketWidth(ENDRoomSocketType Type)
	{
		switch (Type)
		{
		case ENDRoomSocketType::Door_S: return 100.f;
		case ENDRoomSocketType::Door_M: return 160.f;
		case ENDRoomSocketType::Door_L: return 220.f;
		case ENDRoomSocketType::Arch_L: return 260.f;
		case ENDRoomSocketType::Vent: return 40.f;
		case ENDRoomSocketType::Secret: return 100.f;
		default: return 160.f;
		}
	}

	bool IsConnectableSocket(ENDRoomSocketType Type)
	{
		// Escaleras: solo hay planta principal. Exterior: es la salida, no une salas.
		return Type != ENDRoomSocketType::Stairs && Type != ENDRoomSocketType::Outdoor;
	}

	bool AreSocketsCompatible(ENDRoomSocketType A, ENDRoomSocketType B)
	{
		if (!IsConnectableSocket(A) || !IsConnectableSocket(B))
		{
			return false;
		}
		// Una rejilla solo encaja con otra rejilla; el resto de huecos encajan entre sí.
		return (A == ENDRoomSocketType::Vent) == (B == ENDRoomSocketType::Vent);
	}

	FColor GetRoomTypeColor(ENDRoomType Type)
	{
		switch (Type)
		{
		case ENDRoomType::R01_Entry: return FColor(60, 200, 90);
		case ENDRoomType::R02_Corridor: return FColor(170, 170, 170);
		case ENDRoomType::R03_LivingRoom: return FColor(230, 170, 60);
		case ENDRoomType::R04_Kitchen: return FColor(80, 180, 230);
		case ENDRoomType::R05_DiningRoom: return FColor(200, 120, 60);
		case ENDRoomType::R06_MasterBedroom: return FColor(220, 50, 50);
		case ENDRoomType::R07_Bathroom: return FColor(90, 220, 220);
		case ENDRoomType::R08_Storage: return FColor(140, 110, 80);
		case ENDRoomType::R09_Objective: return FColor(250, 220, 40);
		default: return FColor(180, 100, 220);
		}
	}

	FString GetRoomTypeName(ENDRoomType Type)
	{
		return StaticEnum<ENDRoomType>()->GetNameStringByValue(static_cast<int64>(Type));
	}
}
