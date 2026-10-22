// No Despiertes Eso

#include "HouseGen/NDRoomModuleData.h"

FPrimaryAssetId UNDRoomModuleData::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType RoomModuleType(TEXT("NDRoomModule"));
	return FPrimaryAssetId(RoomModuleType, Definition.RoomModuleId.IsNone() ? GetFName() : Definition.RoomModuleId);
}
