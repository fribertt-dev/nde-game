// No Despiertes Eso
// Suite de calidad de las casas H01: muchas semillas por tamaño y contrato, con métricas y layouts fallidos.

#include "Contracts/NDContractDefinition.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HouseGen/NDHouseGenerator.h"
#include "HouseGen/NDHouseLoot.h"
#include "Loot/NDLootDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NDHouseGenQuality
{
	/** Semillas por tamaño y contrato (nde.Quality.Seeds para probar con menos). */
	TAutoConsoleVariable<int32> CVarSeeds(TEXT("nde.Quality.Seeds"), 10000,
		TEXT("Semillas por tamano y contrato de la prueba NDE.Quality.HouseGen (por defecto 10000)."));
	/** Layouts fallidos que se dibujan como SVG (los demás solo se apuntan). */
	constexpr int32 MaxDrawnFailures = 30;

	const TCHAR* ContractIds[] = { TEXT("C01"), TEXT("C02"), TEXT("C03"), TEXT("C04"), TEXT("C05"), TEXT("C06"), TEXT("C07") };
	const TCHAR* LootIds[] = { TEXT("L01"), TEXT("L02"), TEXT("L03"), TEXT("L04"), TEXT("L05"), TEXT("L07"), TEXT("L08"), TEXT("L09"), TEXT("L10"),
		TEXT("L11"), TEXT("L12"), TEXT("L16"), TEXT("L19"), TEXT("L20"), TEXT("L32"), TEXT("L33"), TEXT("L34"), TEXT("L41"), TEXT("L50") };
	const ENDHouseSize Sizes[] = { ENDHouseSize::Small, ENDHouseSize::Medium, ENDHouseSize::Large };

	struct FStats
	{
		int32 Houses = 0;
		int32 Fallbacks = 0;
		int32 ObjectiveUnreachable = 0;
		int32 QuotaShort = 0;
		int32 CarryRouteMissing = 0;
		int32 HidingInvalid = 0;
		int32 PatrolInvalid = 0;
		int32 WithUpper = 0;
		int32 WithBasement = 0;
		double Seconds = 0.0;

		int32 Failures() const { return ObjectiveUnreachable + QuotaShort + CarryRouteMissing + HidingInvalid + PatrolInvalid; }

		void Add(const FStats& Other)
		{
			Houses += Other.Houses; Fallbacks += Other.Fallbacks; ObjectiveUnreachable += Other.ObjectiveUnreachable;
			QuotaShort += Other.QuotaShort; CarryRouteMissing += Other.CarryRouteMissing; HidingInvalid += Other.HidingInvalid;
			PatrolInvalid += Other.PatrolInvalid; WithUpper += Other.WithUpper; WithBasement += Other.WithBasement; Seconds += Other.Seconds;
		}
	};

	FString SizeName(ENDHouseSize Size)
	{
		return StaticEnum<ENDHouseSize>()->GetNameStringByValue(static_cast<int64>(Size));
	}

	/** Layout en SVG: una columna por piso, salas coloreadas, puertas, objetivo y dormitorio marcados. */
	FString DrawLayoutSvg(const FNDHouseLayout& Layout, const FString& Title, const TArray<FString>& Problems)
	{
		constexpr float Scale = 24.f;
		const float LotW = Layout.Lot.SizeCells.Y * Scale;
		const float LotH = Layout.Lot.SizeCells.X * Scale;
		const float Margin = 20.f;
		FString Svg = FString::Printf(TEXT("<svg xmlns='http://www.w3.org/2000/svg' width='%.0f' height='%.0f' font-family='sans-serif' font-size='10'>\n"),
			3.f * (LotW + Margin) + Margin, LotH + 90.f + 14.f * Problems.Num());
		Svg += FString::Printf(TEXT("<text x='%.0f' y='16' font-size='13'>%s</text>\n"), Margin, *Title);
		for (int32 Index = 0; Index < Problems.Num(); ++Index)
		{
			Svg += FString::Printf(TEXT("<text x='%.0f' y='%.0f' fill='#b00'>%s</text>\n"), Margin, 32.f + Index * 14.f, *Problems[Index].Replace(TEXT("<"), TEXT("(")));
		}
		const float Top = 44.f + 14.f * Problems.Num();
		for (int32 Floor = -1; Floor <= 1; ++Floor)
		{
			const float Left = Margin + (Floor + 1) * (LotW + Margin);
			Svg += FString::Printf(TEXT("<rect x='%.0f' y='%.0f' width='%.0f' height='%.0f' fill='#f4f4f0' stroke='#999'/>\n"), Left, Top, LotW, LotH);
			Svg += FString::Printf(TEXT("<text x='%.0f' y='%.0f'>Piso %d</text>\n"), Left, Top + LotH + 14.f, Floor);
			for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
			{
				const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
				if (Room.Floor != Floor)
				{
					continue;
				}
				const TCHAR* Fill = RoomIndex == Layout.ObjectiveRoom ? TEXT("#f2c94c") : (RoomIndex == Layout.BedroomRoom ? TEXT("#eb8a8a")
					: (Room.Def->StairPart != ENDStairPart::None ? TEXT("#9ab") : (RoomIndex == Layout.EntryRoom ? TEXT("#8c8") : TEXT("#cfd8e3"))));
				TArray<FIntPoint> Cells;
				Layout.GetRoomCells(RoomIndex, Cells);
				for (const FIntPoint& Cell : Cells)
				{
					Svg += FString::Printf(TEXT("<rect x='%.0f' y='%.0f' width='%.0f' height='%.0f' fill='%s' stroke='#555' stroke-width='0.5'/>\n"),
						Left + Cell.Y * Scale, Top + (Layout.Lot.SizeCells.X - 1 - Cell.X) * Scale, Scale, Scale, Fill);
				}
				const FIntPoint First = Cells.Num() > 0 ? Cells[0] : FIntPoint::ZeroValue;
				Svg += FString::Printf(TEXT("<text x='%.0f' y='%.0f' font-size='8'>%d</text>\n"),
					Left + First.Y * Scale + 2.f, Top + (Layout.Lot.SizeCells.X - 1 - First.X) * Scale + 9.f, RoomIndex);
				for (const FNDHouseLayoutSocket& Socket : Room.Sockets)
				{
					if (!Socket.IsOpen() || Socket.Type == ENDRoomSocketType::Stairs)
					{
						continue;
					}
					const FIntPoint Out = NDHouseGen::DirectionToOffset(Socket.LotDirection);
					const float X = Left + (Socket.LotCell.Y + 0.5f + Out.Y * 0.5f) * Scale;
					const float Y = Top + (Layout.Lot.SizeCells.X - 1 - Socket.LotCell.X + 0.5f - Out.X * 0.5f) * Scale;
					Svg += FString::Printf(TEXT("<circle cx='%.1f' cy='%.1f' r='2.5' fill='%s'/>\n"), X, Y, Socket.bExit ? TEXT("#0a0") : TEXT("#333"));
				}
			}
		}
		Svg += TEXT("</svg>\n");
		return Svg;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenQualityTest, "NDE.Quality.HouseGen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNDHouseGenQualityTest::RunTest(const FString& Parameters)
{
	using namespace NDHouseGenQuality;
	const int32 Seeds = FMath::Max(1, CVarSeeds.GetValueOnGameThread());

	TArray<FNDHouseLootCandidate> Candidates;
	for (const TCHAR* Id : LootIds)
	{
		const UNDLootDefinition* Loot = LoadObject<UNDLootDefinition>(nullptr, *FString::Printf(TEXT("/Game/_NDE_/Data/Loot/DA_Loot_%s.DA_Loot_%s"), Id, Id));
		if (!Loot)
		{
			AddError(FString::Printf(TEXT("Falta DA_Loot_%s"), Id));
			return false;
		}
		Candidates.Add({ Loot->LootId, Loot->Category, Loot->BaseValue });
	}

	const FString OutDir = FPaths::ProjectSavedDir() / TEXT("HouseGenQuality");
	IFileManager::Get().MakeDirectory(*OutDir, true);
	TArray<FString> FailedLines;
	int32 Drawn = 0;
	TArray<FString> Report;
	Report.Add(FString::Printf(TEXT("# Calidad de casas H01 (%s)"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"))));
	Report.Add(FString::Printf(TEXT("%d semillas por tamaño y contrato; un tercio con piso superior y sótano forzados, el resto en Auto. Generación %s."),
		Seeds, NDHouseGen::GenerationVersion));
	Report.Add(TEXT(""));
	Report.Add(TEXT("| Contrato | Tamaño | Casas | Respaldo | Objetivo inaccesible | Cuota corta | Sin ruta de bulto | Escondites mal | Patrulla mal | Con piso sup. | Con sótano | ms por casa |"));
	Report.Add(TEXT("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"));

	FNDHouseGenQualityTest& Test = *this;
	FStats Total;
	for (const TCHAR* ContractId : ContractIds)
	{
		const UNDContractDefinition* Contract = LoadObject<UNDContractDefinition>(nullptr,
			*FString::Printf(TEXT("/Game/_NDE_/Data/Contracts/DA_Contract_%s.DA_Contract_%s"), ContractId, ContractId));
		if (!Contract)
		{
			AddError(FString::Printf(TEXT("Falta DA_Contract_%s"), ContractId));
			return false;
		}
		const FNDHouseLootContractSpec Spec = FNDHouseLootContractSpec::FromContract(Contract);
		for (const ENDHouseSize Size : Sizes)
		{
			FStats Stats;
			for (int32 Seed = 1; Seed <= Seeds; ++Seed)
			{
				FNDHouseGenParams Params = FNDHouseGenParams::ForArchetype(TEXT("H01"), Size);
				Params.Seed = Seed;
				Params.Lot.FrontDirection = static_cast<ENDCellDirection>(Seed % 4);
				Params.ContractId = Contract->ContractId;
				Params.ContractRequirements = Contract->HouseRequirements;
				if (Seed % 3 == 0)
				{
					Params.ApplyFloorOptions(ENDFloorOption::Always, ENDFloorOption::Always);
				}

				const double Start = FPlatformTime::Seconds();
				FNDHouseGenerationManifest Manifest;
				TArray<FString> Errors;
				const bool bGenerated = FNDHouseGenerator::Generate(Params, Manifest, &Errors);
				FNDHouseLayout Layout;
				FString LayoutError;
				const bool bLayout = bGenerated && FNDHouseLayout::FromManifest(Manifest, Layout, LayoutError);
				Stats.Seconds += FPlatformTime::Seconds() - Start;
				++Stats.Houses;
				if (!bLayout)
				{
					++Stats.ObjectiveUnreachable;
					FailedLines.Add(FString::Printf(TEXT("%s %s %d: no genera (%s)"), ContractId, *SizeName(Size), Seed, Errors.Num() > 0 ? *Errors[0] : *LayoutError));
					continue;
				}
				Stats.Fallbacks += Manifest.bUsedFallback ? 1 : 0;
				int32 LowestFloor = 0;
				int32 HighestFloor = 0;
				Layout.GetFloorRange(LowestFloor, HighestFloor);
				Stats.WithUpper += HighestFloor > 0 ? 1 : 0;
				Stats.WithBasement += LowestFloor < 0 ? 1 : 0;

				TArray<FString> Problems;
				TArray<int32> Depths;
				Layout.ComputeDepths(Depths);
				const int32 Anchor = Layout.GetAnchorRoom(Spec.House);
				TArray<FNDHouseLootPlacement> Placements;
				FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Spec, Candidates, Placements);

				// Objetivo del contrato: todos los requeridos colocados en su sala, a la que se llega.
				int32 RequiredPlaced = 0;
				bool bObjectiveOk = Layout.Rooms.IsValidIndex(Anchor) && Depths[Anchor] != INDEX_NONE;
				for (const FNDHouseLootPlacement& Placement : Placements)
				{
					if (Placement.bRequired)
					{
						++RequiredPlaced;
						bObjectiveOk &= Placement.Room == Anchor;
					}
				}
				bObjectiveOk &= RequiredPlaced == Spec.RequiredLootIds.Num();
				if (!bObjectiveOk)
				{
					++Stats.ObjectiveUnreachable;
					Problems.Add(TEXT("objetivo inaccesible o sin colocar"));
				}

				// Cuota con margen (sin señuelos).
				int32 Value = 0;
				for (const FNDHouseLootPlacement& Placement : Placements)
				{
					Value += Placement.bDecoy ? 0 : Candidates[Placement.Candidate].Value;
				}
				if (Spec.Quota > 0 && Value < Spec.Quota * Spec.House.QuotaCoverage)
				{
					++Stats.QuotaShort;
					Problems.Add(FString::Printf(TEXT("cuota corta: %d de %d"), Value, FMath::CeilToInt(Spec.Quota * Spec.House.QuotaCoverage)));
				}

				// Ruta del bulto (al objetivo y, si lo hay, al objetivo de bulto pesado).
				const bool bHeavy = Spec.RequiredLootIds.ContainsByPredicate([&Candidates](const FName& Id)
				{
					return Candidates.ContainsByPredicate([&Id](const FNDHouseLootCandidate& C) { return C.LootId == Id && C.Category == ENDLootCategory::Heavy; });
				});
				const float Width = FMath::Max3(Spec.House.MinRouteWidth, bHeavy ? Params.CarryEnvelopeWidth : 0.f, 0.f);
				if (!Layout.FindRoute(Layout.EntryRoom, Anchor, Width) || !Layout.FindRoute(Layout.EntryRoom, Layout.ObjectiveRoom, Params.CarryEnvelopeWidth))
				{
					++Stats.CarryRouteMissing;
					Problems.Add(TEXT("sin ruta de bulto"));
				}

				// Escondites: al menos dos, con la entrada en su sala.
				const bool bHidingOk = Layout.HidingSpots.Num() >= 2 && !Layout.HidingSpots.ContainsByPredicate([&Layout](const FNDHouseHidingSpot& Spot)
				{
					return Layout.FindRoomAt(Spot.EntryPoint) != Spot.Room;
				});
				if (!bHidingOk)
				{
					++Stats.HidingInvalid;
					Problems.Add(TEXT("escondites invalidos"));
				}

				// Patrulla: empieza y acaba en el dormitorio; fuera solo salas de puerta pequeña.
				TSet<int32> Visited(Layout.PatrolRoute);
				bool bPatrolOk = Layout.PatrolRoute.Num() > 0 && Layout.PatrolRoute[0] == Layout.BedroomRoom && Layout.PatrolRoute.Last() == Layout.BedroomRoom;
				for (const FNDHousePatrolNode& Node : Layout.PatrolNodes)
				{
					if (!Visited.Contains(Node.StableId))
					{
						bPatrolOk &= !Layout.Connections.ContainsByPredicate([&Layout, &Node](const FNDHouseLayoutConnection& Link)
						{
							return (Link.RoomA == Node.Room || Link.RoomB == Node.Room) && !Layout.IsLowClearance(Link) && Link.OpeningWidth >= 140.f;
						});
					}
				}
				if (!bPatrolOk)
				{
					++Stats.PatrolInvalid;
					Problems.Add(TEXT("patrulla invalida"));
				}

				if (Problems.Num() > 0 || Manifest.bUsedFallback)
				{
					if (Manifest.bUsedFallback)
					{
						Problems.Add(TEXT("casa de respaldo"));
					}
					const FString Name = FString::Printf(TEXT("%s_%s_%d"), ContractId, *SizeName(Size), Seed);
					FailedLines.Add(FString::Printf(TEXT("%s: %s"), *Name, *FString::Join(Problems, TEXT(", "))));
					if (Drawn < MaxDrawnFailures)
					{
						++Drawn;
						FFileHelper::SaveStringToFile(DrawLayoutSvg(Layout, Name, Problems), *(OutDir / FString::Printf(TEXT("layout_%s.svg"), *Name)));
					}
				}
			}
			Report.Add(FString::Printf(TEXT("| %s | %s | %d | %d (%.2f %%) | %d | %d | %d | %d | %d | %d | %d | %.2f |"), ContractId, *SizeName(Size), Stats.Houses,
				Stats.Fallbacks, 100.f * Stats.Fallbacks / FMath::Max(1, Stats.Houses), Stats.ObjectiveUnreachable, Stats.QuotaShort, Stats.CarryRouteMissing,
				Stats.HidingInvalid, Stats.PatrolInvalid, Stats.WithUpper, Stats.WithBasement, 1000.0 * Stats.Seconds / FMath::Max(1, Stats.Houses)));
			Test.AddInfo(Report.Last());
			Total.Add(Stats);
		}
	}

	Report.Add(TEXT(""));
	Report.Add(FString::Printf(TEXT("**Total:** %d casas, %d de respaldo (%.2f %%), %d objetivos inaccesibles, %d cuotas cortas, %d sin ruta de bulto, %d escondites y %d patrullas inválidos, %.2f ms por casa."),
		Total.Houses, Total.Fallbacks, 100.f * Total.Fallbacks / FMath::Max(1, Total.Houses), Total.ObjectiveUnreachable, Total.QuotaShort,
		Total.CarryRouteMissing, Total.HidingInvalid, Total.PatrolInvalid, 1000.0 * Total.Seconds / FMath::Max(1, Total.Houses)));
	FFileHelper::SaveStringArrayToFile(Report, *(OutDir / TEXT("report.md")));
	FFileHelper::SaveStringArrayToFile(FailedLines, *(OutDir / TEXT("failed_seeds.txt")));
	AddInfo(Report.Last());

	TestTrue(TEXT("Casas de respaldo por debajo del 1 %"), Total.Fallbacks < Total.Houses / 100);
	TestEqual(TEXT("Objetivos inaccesibles"), Total.ObjectiveUnreachable, 0);
	TestEqual(TEXT("Cuotas cortas"), Total.QuotaShort, 0);
	TestEqual(TEXT("Casas sin ruta de bulto"), Total.CarryRouteMissing, 0);
	TestEqual(TEXT("Escondites inválidos"), Total.HidingInvalid, 0);
	TestEqual(TEXT("Patrullas inválidas"), Total.PatrolInvalid, 0);
	return true;
}

#endif
