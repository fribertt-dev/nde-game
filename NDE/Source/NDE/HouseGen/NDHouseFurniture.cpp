// No Despiertes Eso

#include "HouseGen/NDHouseFurniture.h"

#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"

namespace NDHouseFurniture
{
	using EKind = ENDDecorationKind;
	using EPlace = ENDFurniturePlacement;

	FNDFurnitureArt MakeArt(const TCHAR* Folder, const TCHAR* Name, const FVector& Size, EKind Kind, TArray<FNDFurnitureCompanion> Companions = {})
	{
		FNDFurnitureArt Art;
		Art.Id = FName(Name);
		Art.MeshPath = FString::Printf(TEXT("/Game/_NDE_/Props/%s/SM_%s/SM_%s.SM_%s"), Folder, Name, Name, Name);
		Art.Size = Size;
		Art.Kind = Kind;
		Art.Companions = MoveTemp(Companions);
		return Art;
	}

	/** Malla escalada (alfombras: más pequeñas y casi planas para no parecer un escalón). */
	FNDFurnitureArt MakeScaled(FNDFurnitureArt Art, const FVector& Scale)
	{
		Art.MeshScale = Scale;
		Art.Size *= Scale;
		return Art;
	}

	/** Malla con el frente en otro eje (MeshYaw); Size va ya en el marco del mueble. */
	FNDFurnitureArt MakeTurned(FNDFurnitureArt Art, float MeshYaw, FName Id = NAME_None)
	{
		Art.Id = Id.IsNone() ? Art.Id : Id;
		Art.MeshYaw = MeshYaw;
		return Art;
	}

	TArray<FNDFurnitureArt> BuildArt()
	{
		// Tamaños medidos de las mallas importadas (bounds), a escala de la casa del gigante.
		const FNDFurnitureCompanion LampOnTop{ TEXT("TableLamp"), FVector(0.f, 0.f, 140.f) };
		return {
			// Sala de estar
			MakeArt(TEXT("Living"), TEXT("Sofa_A"), FVector(500.f, 195.f, 146.f), EKind::Armchair),
			MakeArt(TEXT("Living"), TEXT("Armchair_A"), FVector(225.f, 204.f, 167.f), EKind::Armchair),
			MakeArt(TEXT("Living"), TEXT("Bookshelf_Tall"), FVector(231.f, 133.f, 450.f), EKind::Shelf),
			MakeArt(TEXT("Living"), TEXT("Fireplace"), FVector(375.f, 211.f, 335.f), EKind::Appliance),
			MakeArt(TEXT("Living"), TEXT("CoffeeTable"), FVector(250.f, 158.f, 70.f), EKind::Table),
			MakeArt(TEXT("Living"), TEXT("SideTable"), FVector(110.f, 126.f, 140.f), EKind::Appliance, { LampOnTop }),
			MakeArt(TEXT("Living"), TEXT("TV_Old"), FVector(131.f, 87.f, 175.f), EKind::Rug),
			MakeArt(TEXT("Living"), TEXT("TableLamp"), FVector(66.f, 66.f, 115.f), EKind::Rug),
			MakeArt(TEXT("Living"), TEXT("KitchenTable"), FVector(300.f, 180.f, 128.f), EKind::Table,
				{ { TEXT("Stool"), FVector(-70.f, 141.f, 0.f), 0.f, false }, { TEXT("Stool"), FVector(70.f, -141.f, 0.f), 180.f, false } }),
			MakeArt(TEXT("Living"), TEXT("Stool"), FVector(82.f, 71.f, 83.f), EKind::Crate),
			MakeArt(TEXT("Dining"), TEXT("TVStand"), FVector(375.f, 123.f, 109.f), EKind::Appliance, { { TEXT("TV_Old"), FVector(0.f, 18.f, 109.f) } }),
			MakeScaled(MakeArt(TEXT("Entry_Common"), TEXT("Rug_A"), FVector(400.f, 318.f, 21.f), EKind::Rug), FVector(0.75f, 0.75f, 0.2f)),
			MakeScaled(MakeArt(TEXT("Entry_Common"), TEXT("Rug_B"), FVector(400.f, 322.f, 22.f), EKind::Rug), FVector(0.75f, 0.75f, 0.2f)),
			// Cocina
			MakeArt(TEXT("Kitchen"), TEXT("Fridge"), FVector(169.f, 195.f, 450.f), EKind::Appliance),
			MakeArt(TEXT("Kitchen"), TEXT("Stove_Oven"), FVector(182.f, 160.f, 225.f), EKind::Appliance),
			MakeArt(TEXT("Kitchen"), TEXT("Counter_Sink"), FVector(206.f, 163.f, 225.f), EKind::Appliance),
			MakeArt(TEXT("Kitchen"), TEXT("Counter_Base"), FVector(202.f, 105.f, 225.f), EKind::Appliance, { { TEXT("Kettle"), FVector(-40.f, 10.f, 225.f), 30.f } }),
			MakeArt(TEXT("Kitchen"), TEXT("Counter_Straight"), FVector(150.f, 67.f, 185.f), EKind::Appliance,
				{ { TEXT("Pot"), FVector(-35.f, 0.f, 185.f) }, { TEXT("Pan"), FVector(35.f, 0.f, 185.f), 20.f } }),
			MakeArt(TEXT("Kitchen"), TEXT("Kettle"), FVector(54.f, 38.f, 29.f), EKind::Rug),
			MakeArt(TEXT("Kitchen"), TEXT("Pot"), FVector(37.f, 32.f, 38.f), EKind::Rug),
			MakeArt(TEXT("Kitchen"), TEXT("Pan"), FVector(65.f, 38.f, 11.f), EKind::Rug),
			MakeArt(TEXT("Kitchen"), TEXT("Pantry_Shelf"), FVector(272.f, 128.f, 450.f), EKind::Shelf),
			MakeArt(TEXT("Kitchen"), TEXT("TrashBin_Kitchen"), FVector(87.f, 94.f, 142.f), EKind::Crate),
			MakeArt(TEXT("Kitchen"), TEXT("PetBowl"), FVector(71.f, 73.f, 23.f), EKind::Rug),
			// Comedor
			MakeArt(TEXT("Dining"), TEXT("DiningTable"), FVector(400.f, 266.f, 91.f), EKind::Table,
				{ { TEXT("DiningChair"), FVector(-100.f, 212.f, 0.f), 0.f, false }, { TEXT("DiningChair"), FVector(100.f, 212.f, 0.f), 0.f, false },
				  { TEXT("DiningChair"), FVector(-100.f, -212.f, 0.f), 180.f, false }, { TEXT("DiningChair"), FVector(100.f, -212.f, 0.f), 180.f, false },
				  { TEXT("DiningChair"), FVector(279.f, 0.f, 0.f), -90.f, false }, { TEXT("DiningChair"), FVector(-279.f, 0.f, 0.f), 90.f, false },
				  { TEXT("Tableware_Plate"), FVector(-100.f, 70.f, 91.f) }, { TEXT("Tableware_Plate"), FVector(100.f, -70.f, 91.f) },
				  { TEXT("Tableware_Fork"), FVector(-145.f, 70.f, 91.f) }, { TEXT("Tableware_Knife"), FVector(-55.f, 70.f, 91.f) },
				  { TEXT("Tableware_Spoon"), FVector(145.f, -70.f, 91.f) }, { TEXT("Tableware_Cup"), FVector(40.f, -40.f, 91.f) } }),
			MakeArt(TEXT("Dining"), TEXT("DiningTable_Cloth"), FVector(400.f, 237.f, 150.f), EKind::Table,
				{ { TEXT("DiningChair"), FVector(-100.f, 198.f, 0.f), 0.f, false }, { TEXT("DiningChair"), FVector(100.f, -198.f, 0.f), 180.f, false },
				  { TEXT("Tableware_Plate"), FVector(0.f, 60.f, 91.f) }, { TEXT("Tableware_Cup"), FVector(60.f, -40.f, 91.f) } }),
			MakeArt(TEXT("Dining"), TEXT("Tableware_Fork"), FVector(24.f, 26.f, 22.f), EKind::Rug),
			MakeArt(TEXT("Dining"), TEXT("Tableware_Knife"), FVector(26.f, 28.f, 21.f), EKind::Rug),
			MakeArt(TEXT("Dining"), TEXT("Tableware_Spoon"), FVector(21.f, 22.f, 22.f), EKind::Rug),
			MakeArt(TEXT("Dining"), TEXT("Tableware_Cup"), FVector(25.f, 30.f, 21.f), EKind::Rug),
			// La silla tiene el respaldo en +X de su malla.
			MakeTurned(MakeArt(TEXT("Dining"), TEXT("DiningChair"), FVector(127.f, 138.f, 225.f), EKind::Crate), 90.f),
			MakeArt(TEXT("Dining"), TEXT("Tableware_Plate"), FVector(60.f, 60.f, 6.f), EKind::Rug),
			MakeArt(TEXT("Dining"), TEXT("Sideboard"), FVector(400.f, 116.f, 176.f), EKind::Shelf),
			MakeArt(TEXT("Dining"), TEXT("DisplayCabinet"), FVector(261.f, 125.f, 450.f), EKind::Shelf),
			// Dormitorios
			MakeArt(TEXT("Bedroom"), TEXT("Bed_Double"), FVector(475.f, 472.f, 266.f), EKind::Bed),
			MakeArt(TEXT("Bedroom"), TEXT("Bed_Single"), FVector(283.f, 475.f, 219.f), EKind::Bed),
			MakeTurned(MakeArt(TEXT("Bedroom"), TEXT("Bed_Single"), FVector(475.f, 283.f, 219.f), EKind::Bed), 90.f, TEXT("Bed_Single_Side")),
			MakeArt(TEXT("Bedroom"), TEXT("Wardrobe"), FVector(298.f, 179.f, 500.f), EKind::Wardrobe),
			MakeArt(TEXT("Bedroom"), TEXT("Dresser"), FVector(250.f, 115.f, 164.f), EKind::Appliance, { { TEXT("Radio_Old"), FVector(60.f, 20.f, 164.f) } }),
			MakeArt(TEXT("Living"), TEXT("Radio_Old"), FVector(75.f, 41.f, 46.f), EKind::Rug),
			MakeArt(TEXT("Bedroom"), TEXT("Bed_Blanket"), FVector(200.f, 198.f, 101.f), EKind::Rug),
			MakeArt(TEXT("Bedroom"), TEXT("Bed_Pillow"), FVector(175.f, 97.f, 61.f), EKind::Rug),
			MakeArt(TEXT("Bedroom"), TEXT("Nightstand"), FVector(137.f, 111.f, 140.f), EKind::Crate, { LampOnTop }),
			MakeArt(TEXT("Bedroom"), TEXT("LaundryHamper"), FVector(142.f, 142.f, 160.f), EKind::Crate),
			MakeArt(TEXT("Bedroom"), TEXT("Slipper_Giant"), FVector(46.f, 75.f, 28.f), EKind::Rug),
			// Baño
			MakeArt(TEXT("Bathroom"), TEXT("Toilet"), FVector(131.f, 169.f, 190.f), EKind::Appliance, { { TEXT("ToiletPaper"), FVector(100.f, 50.f, 0.f) } }),
			MakeArt(TEXT("Bathroom"), TEXT("ToiletPaper"), FVector(26.f, 30.f, 18.f), EKind::Rug),
			MakeArt(TEXT("Bathroom"), TEXT("SoapBar"), FVector(24.f, 25.f, 15.f), EKind::Rug),
			MakeArt(TEXT("Bathroom"), TEXT("Toothbrush_Cup"), FVector(20.f, 19.f, 40.f), EKind::Rug),
			MakeArt(TEXT("Bathroom"), TEXT("Towel_Hanging"), FVector(250.f, 124.f, 238.f), EKind::Appliance),
			MakeArt(TEXT("Bathroom"), TEXT("Sink_Bathroom"), FVector(179.f, 118.f, 210.f), EKind::Appliance,
				{ { TEXT("BathMirror_Cabinet"), FVector(0.f, 16.f, 300.f) }, { TEXT("SoapBar"), FVector(-65.f, 20.f, 210.f) }, { TEXT("Toothbrush_Cup"), FVector(65.f, 20.f, 210.f) } }),
			MakeArt(TEXT("Bathroom"), TEXT("BathMirror_Cabinet"), FVector(175.f, 86.f, 157.f), EKind::Rug),
			MakeArt(TEXT("Bathroom"), TEXT("Bathtub"), FVector(425.f, 239.f, 230.f), EKind::Appliance),
			MakeScaled(MakeArt(TEXT("Bathroom"), TEXT("BathMat"), FVector(200.f, 156.f, 28.f), EKind::Rug), FVector(0.8f, 0.8f, 0.2f)),
			// Entrada, trastero y otras
			MakeArt(TEXT("Entry_Common"), TEXT("CoatRack"), FVector(186.f, 183.f, 450.f), EKind::Appliance),
			MakeArt(TEXT("Entry_Common"), TEXT("ShoeRack"), FVector(250.f, 135.f, 223.f), EKind::Shelf),
			MakeScaled(MakeArt(TEXT("Entry_Common"), TEXT("Doormat"), FVector(150.f, 97.f, 13.f), EKind::Rug), FVector(1.f, 1.f, 0.4f)),
			MakeArt(TEXT("Entry_Common"), TEXT("CardboardBox"), FVector(146.f, 150.f, 113.f), EKind::Crate),
			MakeArt(TEXT("Entry_Common"), TEXT("BookStack"), FVector(90.f, 74.f, 74.f), EKind::Rug),
			MakeArt(TEXT("Entry_Common"), TEXT("Boot_Giant"), FVector(45.f, 75.f, 69.f), EKind::Rug),
		};
	}

	const TArray<FNDFurnitureArt>& GetArt()
	{
		static const TArray<FNDFurnitureArt> Art = BuildArt();
		return Art;
	}

	int32 FindArt(FName Id)
	{
		return GetArt().IndexOfByPredicate([Id](const FNDFurnitureArt& Art) { return Art.Id == Id; });
	}

	FName GetCreatureBedId()
	{
		return TEXT("Bed_Double");
	}

	TMap<ENDRoomType, TArray<FNDRoomFurniture>> BuildRoomFurniture()
	{
		using ERoom = ENDRoomType;
		const FNDRoomFurniture Rug{ TEXT("Rug_A"), EPlace::Flat, 1, 0.7f };
		const FNDRoomFurniture RugB{ TEXT("Rug_B"), EPlace::Flat, 1, 0.7f };
		const FNDRoomFurniture Boxes{ TEXT("CardboardBox"), EPlace::Wall, 3, 0.8f };
		// Piezas pequeñas de relleno: caben en las salas de 1 celda.
		const FNDRoomFurniture Bin{ TEXT("TrashBin_Kitchen"), EPlace::Wall, 1, 0.8f };
		const FNDRoomFurniture Stool{ TEXT("Stool"), EPlace::Wall, 1, 0.8f };
		TMap<ERoom, TArray<FNDRoomFurniture>> Rooms;
		Rooms.Add(ERoom::R01_Entry, { { TEXT("ShoeRack") }, { TEXT("CoatRack"), EPlace::Wall, 1, 0.8f }, { TEXT("Boot_Giant"), EPlace::Flat, 1, 0.6f }, Rug });
		Rooms.Add(ERoom::R03_LivingRoom, { { TEXT("Sofa_A") }, { TEXT("Fireplace"), EPlace::Wall, 1, 0.7f }, { TEXT("TVStand"), EPlace::Wall, 1, 0.8f },
			{ TEXT("Armchair_A"), EPlace::Wall, 2 }, { TEXT("Bookshelf_Tall"), EPlace::Wall, 1, 0.6f }, { TEXT("SideTable"), EPlace::Wall, 1, 0.7f },
			{ TEXT("CoffeeTable"), EPlace::Center }, Rug });
		Rooms.Add(ERoom::R04_Kitchen, { { TEXT("Fridge") }, { TEXT("Stove_Oven") }, { TEXT("Counter_Sink") }, { TEXT("Counter_Base"), EPlace::Wall, 2 },
			{ TEXT("Counter_Straight"), EPlace::Wall, 1, 0.6f }, { TEXT("TrashBin_Kitchen"), EPlace::Wall, 1, 0.8f }, { TEXT("KitchenTable"), EPlace::Center, 1, 0.8f },
			{ TEXT("PetBowl"), EPlace::Flat, 1, 0.5f } });
		Rooms.Add(ERoom::R05_DiningRoom, { { TEXT("DiningTable_Cloth"), EPlace::Center, 1, 0.5f }, { TEXT("DiningTable"), EPlace::Center }, { TEXT("Sideboard") }, { TEXT("DisplayCabinet"), EPlace::Wall, 1, 0.8f },
			{ TEXT("DiningChair"), EPlace::Wall, 2, 0.6f }, Stool });
		Rooms.Add(ERoom::R06_MasterBedroom, { { TEXT("Wardrobe") }, { TEXT("Nightstand"), EPlace::Wall, 2 }, { TEXT("Dresser") },
			{ TEXT("LaundryHamper"), EPlace::Wall, 1, 0.6f }, Stool, { TEXT("Slipper_Giant"), EPlace::Flat, 1, 0.7f } });
		Rooms.Add(ERoom::R07_Bathroom, { { TEXT("Toilet") }, { TEXT("Sink_Bathroom") }, { TEXT("Bathtub") }, { TEXT("LaundryHamper"), EPlace::Wall, 1, 0.6f }, { TEXT("Towel_Hanging"), EPlace::Wall, 1, 0.7f }, Bin,
			{ TEXT("BathMat"), EPlace::Flat } });
		Rooms.Add(ERoom::R08_Storage, { Boxes, { TEXT("Pantry_Shelf"), EPlace::Wall, 2 }, { TEXT("Bookshelf_Tall"), EPlace::Wall, 1, 0.5f }, Bin, Stool });
		Rooms.Add(ERoom::R09_Objective, { { TEXT("DisplayCabinet") }, { TEXT("SideTable") }, { TEXT("Bookshelf_Tall"), EPlace::Wall, 2 }, { TEXT("Armchair_A"), EPlace::Wall, 1, 0.6f }, Stool, RugB });
		Rooms.Add(ERoom::R10_KidsBedroom, { { TEXT("Bed_Single") }, { TEXT("Bed_Single_Side") }, { TEXT("Nightstand") }, { TEXT("Wardrobe") }, { TEXT("Bookshelf_Tall"), EPlace::Wall, 1, 0.5f }, RugB });
		Rooms.Add(ERoom::R11_Study, { { TEXT("KitchenTable"), EPlace::Wall }, { TEXT("Bookshelf_Tall"), EPlace::Wall, 2 }, { TEXT("Armchair_A") },
			{ TEXT("SideTable"), EPlace::Wall, 1, 0.7f }, Stool, { TEXT("BookStack"), EPlace::Flat, 1, 0.7f } });
		Rooms.Add(ERoom::R12_Pantry, { { TEXT("Pantry_Shelf"), EPlace::Wall, 2 }, Boxes, { TEXT("TrashBin_Kitchen"), EPlace::Wall, 1, 0.6f } });
		Rooms.Add(ERoom::R13_Laundry, { { TEXT("Counter_Sink") }, { TEXT("LaundryHamper"), EPlace::Wall, 2 }, Boxes, Bin, Stool, { TEXT("Bed_Blanket"), EPlace::Flat, 1, 0.7f } });
		Rooms.Add(ERoom::R14_Library, { { TEXT("Bookshelf_Tall"), EPlace::Wall, 5 }, { TEXT("Armchair_A") }, { TEXT("SideTable") }, Rug,
			{ TEXT("BookStack"), EPlace::Flat, 1, 0.7f } });
		Rooms.Add(ERoom::R15_Playroom, { Boxes, { TEXT("Bookshelf_Tall") }, { TEXT("LaundryHamper") }, Stool, RugB, { TEXT("BookStack"), EPlace::Flat }, { TEXT("Bed_Pillow"), EPlace::Flat, 2 } });
		Rooms.Add(ERoom::R16_Basement, { Boxes, { TEXT("Pantry_Shelf"), EPlace::Wall, 2 }, { TEXT("CardboardBox"), EPlace::Center, 1, 0.5f } });
		return Rooms;
	}

	const TArray<FNDRoomFurniture>& GetRoomFurniture(ENDRoomType RoomType)
	{
		static const TMap<ENDRoomType, TArray<FNDRoomFurniture>> Rooms = BuildRoomFurniture();
		static const TArray<FNDRoomFurniture> None;
		const TArray<FNDRoomFurniture>* Found = Rooms.Find(RoomType);
		return Found ? *Found : None;
	}
}

namespace NDHouseArt
{
	TAutoConsoleVariable<int32> CVarArt(
		TEXT("nde.HouseGen.Art"),
		1,
		TEXT("Mallas de arte en la casa: 1 = si (la colision greybox queda invisible), 0 = solo greybox. Se aplica al construir o regenerar."));

	bool IsEnabled()
	{
		return CVarArt.GetValueOnGameThread() != 0;
	}

	UStaticMesh* LoadMesh(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FString GetStructurePath(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/_NDE_/Props/Structure/SM_%s/SM_%s.SM_%s"), Name, Name, Name);
	}
}
