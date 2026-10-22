// No Despiertes Eso

#include "HouseGen/NDHouseGenDebug.h"

#include "DrawDebugHelpers.h"
#include "HouseGen/NDHouseLayout.h"

namespace NDHouseGenDebug
{
	constexpr float TextDuration = 3600.f;

	FColor GetMarkerColor(ENDRoomMarkerType Type)
	{
		switch (Type)
		{
		case ENDRoomMarkerType::LootSocket: return FColor::Yellow;
		case ENDRoomMarkerType::HideSpot: return FColor::Blue;
		case ENDRoomMarkerType::MonsterAnchor: return FColor::Red;
		case ENDRoomMarkerType::Extraction: return FColor::Green;
		default: return FColor::White;
		}
	}

	FString GetRoomLabel(const FNDHouseLayout& Layout, int32 RoomIndex)
	{
		const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
		return FString::Printf(TEXT("P%d %s d%d%s"), Room.Floor, *NDHouseGen::GetRoomTypeName(Room.Def->RoomType), Room.Depth,
			RoomIndex == Layout.ObjectiveRoom ? TEXT(" OBJ") : TEXT(""));
	}

	void DrawLayout(const UWorld* World, const FTransform& LotToWorld, const FNDHouseLayout& Layout, float CarryEnvelopeWidth, bool bDrawText)
	{
		if (!World)
		{
			return;
		}
		using namespace NDHouseGen;
		const FQuat Rotation = LotToWorld.GetRotation();
		auto ToWorld = [&LotToWorld](const FVector& LotLocation) { return LotToWorld.TransformPosition(LotLocation); };

		// Lote y flecha hacia la calle.
		const FVector LotExtent(Layout.Lot.SizeCells.X * CellSize * 0.5f, Layout.Lot.SizeCells.Y * CellSize * 0.5f, 5.f);
		DrawDebugBox(World, ToWorld(FVector::ZeroVector), LotExtent, Rotation, FColor::White, true, -1.f, 0, 4.f);
		const FIntPoint Front = DirectionToOffset(Layout.Lot.FrontDirection);
		const FVector FrontDir(Front.X, Front.Y, 0.f);
		const float FrontDistance = (Front.X != 0 ? LotExtent.X : LotExtent.Y);
		DrawDebugDirectionalArrow(World, ToWorld(FrontDir * (FrontDistance - 300.f) + FVector(0, 0, 20)), ToWorld(FrontDir * (FrontDistance + 300.f) + FVector(0, 0, 20)),
			200.f, FColor::White, true, -1.f, 0, 8.f);

		for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
		{
			const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
			const FVector Center = Layout.GetRoomCenter(RoomIndex);
			const FVector Extent(Room.Size.X * CellSize * 0.5f - 20.f, Room.Size.Y * CellSize * 0.5f - 20.f, 75.f);
			DrawDebugBox(World, ToWorld(Center + FVector(0, 0, 75.f)), Extent, Rotation, GetRoomTypeColor(Room.Def->RoomType), true, -1.f, 0, 6.f);
			if (bDrawText)
			{
				DrawDebugString(World, ToWorld(Center + FVector(0, 0, 180.f)), GetRoomLabel(Layout, RoomIndex), nullptr, GetRoomTypeColor(Room.Def->RoomType), TextDuration, true);
			}
			for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
			{
				if (Room.Sockets[SocketIndex].bExit)
				{
					DrawDebugSphere(World, ToWorld(Layout.GetSocketLocation(RoomIndex, SocketIndex) + FVector(0, 0, 110.f)), 40.f, 8, FColor::Green, true, -1.f, 0, 3.f);
				}
			}
		}

		const FVector LineHeight(0, 0, 150.f);
		for (const FNDHouseLayoutConnection& Link : Layout.Connections)
		{
			const FColor Color = Link.bIsLoop ? FColor::Orange : FColor::White;
			const FVector Door = Layout.GetSocketLocation(Link.RoomA, Link.SocketA) + LineHeight;
			DrawDebugLine(World, ToWorld(Layout.GetRoomCenter(Link.RoomA) + LineHeight), ToWorld(Door), Color, true, -1.f, 0, 5.f);
			DrawDebugLine(World, ToWorld(Door), ToWorld(Layout.GetRoomCenter(Link.RoomB) + LineHeight), Color, true, -1.f, 0, 5.f);
		}

		TArray<int32> Route;
		if (Layout.FindRoute(Layout.EntryRoom, Layout.ObjectiveRoom, CarryEnvelopeWidth, &Route))
		{
			const FVector RouteHeight(0, 0, 165.f);
			for (int32 Step = 1; Step < Route.Num(); ++Step)
			{
				DrawDebugLine(World, ToWorld(Layout.GetRoomCenter(Route[Step - 1]) + RouteHeight), ToWorld(Layout.GetRoomCenter(Route[Step]) + RouteHeight), FColor::Green, true, -1.f, 0, 12.f);
			}
		}

		for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
		{
			const float Size = Marker.Type == ENDRoomMarkerType::Extraction || Marker.Type == ENDRoomMarkerType::MonsterAnchor ? 40.f : 20.f;
			DrawDebugPoint(World, ToWorld(Marker.LotTransform.GetLocation() + FVector(0, 0, 30.f)), Size, GetMarkerColor(Marker.Type), true, -1.f);
		}
	}
}
