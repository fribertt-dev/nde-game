// No Despiertes Eso

#include "HouseGen/NDHouseFurniture.h"
#include "HouseGen/NDHouseGenerator.h"
#include "HouseGen/NDHouseLoot.h"
#include "Contracts/NDContractDefinition.h"
#include "Loot/NDLootDefinition.h"
#include "HouseGen/NDRoomCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NDHouseGenTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const ENDHouseSize AllSizes[] = { ENDHouseSize::Small, ENDHouseSize::Medium, ENDHouseSize::Large };

	FString SizeName(ENDHouseSize Size)
	{
		return StaticEnum<ENDHouseSize>()->GetNameStringByValue(static_cast<int64>(Size));
	}

	/** Reglas de H01 con la fachada girando según la semilla, para probar las cuatro orientaciones. */
	FNDHouseGenParams MakeParams(int64 Seed, ENDHouseSize Size = ENDHouseSize::Small)
	{
		FNDHouseGenParams Params = FNDHouseGenParams::ForArchetype(TEXT("H01"), Size);
		Params.Seed = Seed;
		Params.Lot.FrontDirection = static_cast<ENDCellDirection>(Seed % 4);
		return Params;
	}

	/** Igual, pero el tamaño también va rotando con la semilla. */
	FNDHouseGenParams MakeMixedParams(int64 Seed)
	{
		return MakeParams(Seed, AllSizes[static_cast<int32>(FMath::Abs(Seed) % 3)]);
	}

	/** true si A y B se unen sin salir del piso Floor. */
	bool AreConnectedOnFloor(const FNDHouseLayout& Layout, int32 A, int32 B, int32 Floor)
	{
		TArray<int32> Queue = { A };
		TSet<int32> Seen = { A };
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			for (const FNDHouseLayoutConnection& Link : Layout.Connections)
			{
				const int32 Other = Link.RoomA == Queue[Head] ? Link.RoomB : (Link.RoomB == Queue[Head] ? Link.RoomA : INDEX_NONE);
				if (Other != INDEX_NONE && Layout.Rooms[Other].Floor == Floor && !Seen.Contains(Other))
				{
					Seen.Add(Other);
					Queue.Add(Other);
				}
			}
		}
		return Seen.Contains(B);
	}

	bool GenerateLayout(FAutomationTestBase& Test, const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest, FNDHouseLayout& OutLayout)
	{
		TArray<FString> Errors;
		if (!FNDHouseGenerator::Generate(Params, OutManifest, &Errors))
		{
			Test.AddError(FString::Printf(TEXT("Semilla %lld: no genera (%s)"), Params.Seed, Errors.Num() > 0 ? *Errors[0] : TEXT("?")));
			return false;
		}
		FString Error;
		if (!FNDHouseLayout::FromManifest(OutManifest, OutLayout, Error))
		{
			Test.AddError(FString::Printf(TEXT("Semilla %lld: manifiesto no valido (%s)"), Params.Seed, *Error));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenConnectivityTest, "NDE.HouseGen.Connectivity", NDHouseGenTests::Flags)

bool FNDHouseGenConnectivityTest::RunTest(const FString& Parameters)
{
	// Catálogo: cada hueco está en el borde y mira hacia fuera; ids únicos.
	for (const FNDRoomModuleDefinition& Def : FNDRoomCatalog::GetModules())
	{
		TSet<FName> Ids;
		for (const FNDRoomDoorSocket& Socket : Def.DoorSockets)
		{
			const FIntPoint Outside = Socket.Cell + NDHouseGen::DirectionToOffset(Socket.Direction);
			TestTrue(FString::Printf(TEXT("%s.%s esta en una celda de la sala"), *Def.RoomModuleId.ToString(), *Socket.SocketId.ToString()), Def.ContainsCell(Socket.Cell));
			TestFalse(FString::Printf(TEXT("%s.%s mira hacia fuera"), *Def.RoomModuleId.ToString(), *Socket.SocketId.ToString()), Def.ContainsCell(Outside));
			bool bDuplicate = false;
			Ids.Add(Socket.SocketId, &bDuplicate);
			TestFalse(FString::Printf(TEXT("%s.%s id unico"), *Def.RoomModuleId.ToString(), *Socket.SocketId.ToString()), bDuplicate);
		}
	}

	// La casa de respaldo también tiene que ser válida, en cada tamaño y en las cuatro orientaciones.
	for (const ENDHouseSize Size : NDHouseGenTests::AllSizes)
	{
		for (int32 Front = 0; Front < 4; ++Front)
		{
			const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Front, Size);
			FNDHouseGenerationManifest Fallback;
			FNDHouseLayout Layout;
			FString Error;
			TArray<FString> Errors;
			if (!TestTrue(FString::Printf(TEXT("Casa de respaldo %s se construye"), *NDHouseGenTests::SizeName(Size)),
				FNDHouseGenerator::BuildFallback(Params, Fallback) && FNDHouseLayout::FromManifest(Fallback, Layout, Error)))
			{
				continue;
			}
			const bool bValid = FNDHouseValidator::Validate(Layout, Params, Errors);
			TestTrue(FString::Printf(TEXT("Casa de respaldo %s valida (fachada %d): %s"), *NDHouseGenTests::SizeName(Size), Front, *FString::Join(Errors, TEXT("; "))), bValid);
		}
	}

	// 10.000 semillas por tamaño: conexa desde la entrada, dormitorio y objetivo a su profundidad,
	// sin solapes y dentro del lote (lo comprueba el validador).
	constexpr int32 SeedCount = 10000;
	for (const ENDHouseSize Size : NDHouseGenTests::AllSizes)
	{
		int32 FallbackCount = 0;
		int32 FailureCount = 0;
		int32 LoopCount = 0;
		int32 SecondaryExitCount = 0;
		int32 MinRoomsSeen = MAX_int32;
		int32 MaxRoomsSeen = 0;
		for (int64 Seed = 1; Seed <= SeedCount; ++Seed)
		{
			const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed, Size);
			FNDHouseGenerationManifest Manifest;
			FNDHouseLayout Layout;
			if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
			{
				++FailureCount;
				continue;
			}
			TArray<FString> Errors;
			if (!FNDHouseValidator::Validate(Layout, Params, Errors))
			{
				AddError(FString::Printf(TEXT("%s semilla %lld: %s"), *NDHouseGenTests::SizeName(Size), Seed, *FString::Join(Errors, TEXT("; "))));
				++FailureCount;
			}
			FallbackCount += Manifest.bUsedFallback ? 1 : 0;
			LoopCount += Manifest.Connections.FilterByPredicate([](const FNDHouseManifestConnection& L) { return L.bIsLoop; }).Num();
			SecondaryExitCount += Manifest.SecondaryExitRoom != INDEX_NONE ? 1 : 0;
			MinRoomsSeen = FMath::Min(MinRoomsSeen, Manifest.Rooms.Num());
			MaxRoomsSeen = FMath::Max(MaxRoomsSeen, Manifest.Rooms.Num());
			if (FailureCount > 20)
			{
				AddError(TEXT("Demasiados fallos; se para la prueba"));
				break;
			}
		}
		AddInfo(FString::Printf(TEXT("%s, %d semillas: %d-%d salas, %d casas de respaldo (%.2f %%), %d bucles, %d accesos secundarios"),
			*NDHouseGenTests::SizeName(Size), SeedCount, MinRoomsSeen, MaxRoomsSeen, FallbackCount, FallbackCount * 100.f / SeedCount,
			LoopCount, SecondaryExitCount));
		TestTrue(FString::Printf(TEXT("%s: casa de respaldo en menos del 1%% de semillas"), *NDHouseGenTests::SizeName(Size)), FallbackCount * 100 < SeedCount);
	}

	// Un grafo roto se detecta: quitar una puerta necesaria deja salas sin alcanzar.
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(7);
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			const int32 TreeLink = Manifest.Connections.IndexOfByPredicate([](const FNDHouseManifestConnection& L) { return !L.bIsLoop && L.RoomA == 0; });
			if (TreeLink == INDEX_NONE)
			{
				AddError(TEXT("La entrada no esta unida al distribuidor"));
				return false;
			}
			Manifest.Connections.RemoveAt(TreeLink);
			FString Error;
			TArray<FString> Errors;
			TestTrue(TEXT("Manifiesto roto se resuelve"), FNDHouseLayout::FromManifest(Manifest, Layout, Error));
			TestFalse(TEXT("Casa sin la puerta entrada-distribuidor no es valida"), FNDHouseValidator::Validate(Layout, Params, Errors));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenDeterminismTest, "NDE.HouseGen.Determinism", NDHouseGenTests::Flags)

bool FNDHouseGenDeterminismTest::RunTest(const FString& Parameters)
{
	TSet<FString> Distinct;
	for (int64 Seed = 1; Seed <= 1000; ++Seed)
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(Seed);
		FNDHouseGenerationManifest First;
		FNDHouseGenerationManifest Second;
		FNDHouseGenerator::Generate(Params, First);
		FNDHouseGenerator::Generate(Params, Second);
		const FString FirstText = First.ToDebugString();
		if (FirstText != Second.ToDebugString())
		{
			AddError(FString::Printf(TEXT("Semilla %lld da dos casas distintas"), Seed));
		}
		if (Seed <= 200)
		{
			Distinct.Add(FirstText);
		}
	}
	// Semillas negativas y grandes también valen.
	for (const int64 Seed : { -1LL, -987654321LL, 9007199254740993LL })
	{
		FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(FMath::Abs(Seed));
		Params.Seed = Seed;
		FNDHouseGenerationManifest First;
		FNDHouseGenerationManifest Second;
		TestTrue(FString::Printf(TEXT("Semilla %lld genera"), Seed), FNDHouseGenerator::Generate(Params, First) && FNDHouseGenerator::Generate(Params, Second));
		TestEqual(FString::Printf(TEXT("Semilla %lld determinista"), Seed), First.ToDebugString(), Second.ToDebugString());
	}
	// Semillas distintas tienen que dar casas variadas.
	TestTrue(FString::Printf(TEXT("Variedad: %d casas distintas de 200 semillas"), Distinct.Num()), Distinct.Num() >= 150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenCarryEnvelopeTest, "NDE.HouseGen.CarryEnvelope", NDHouseGenTests::Flags)

bool FNDHouseGenCarryEnvelopeTest::RunTest(const FString& Parameters)
{
	// La cápsula del Desvelado mide 22 cm de radio: el bulto tiene que ser más ancho.
	constexpr float CapsuleDiameter = 44.f;
	TestTrue(TEXT("Envolvente del bulto mayor que la capsula"), FNDHouseGenParams().CarryEnvelopeWidth > CapsuleDiameter);

	for (int64 Seed = 1; Seed <= 2000; ++Seed)
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(Seed);
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			continue;
		}
		TArray<int32> Route;
		if (!Layout.FindRoute(Layout.EntryRoom, Layout.ObjectiveRoom, Params.CarryEnvelopeWidth, &Route))
		{
			AddError(FString::Printf(TEXT("Semilla %lld: sin ruta de bulto al objetivo"), Seed));
			continue;
		}
		// Cada tramo de la ruta cruza una puerta al menos tan ancha como el bulto.
		for (int32 Step = 1; Step < Route.Num(); ++Step)
		{
			const bool bWide = Layout.Connections.ContainsByPredicate([&](const FNDHouseLayoutConnection& L)
			{
				const bool bJoins = (L.RoomA == Route[Step - 1] && L.RoomB == Route[Step]) || (L.RoomB == Route[Step - 1] && L.RoomA == Route[Step]);
				return bJoins && L.OpeningWidth >= Params.CarryEnvelopeWidth;
			});
			if (!bWide)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: la ruta cruza una puerta estrecha"), Seed));
			}
		}

		// Un bulto más ancho que una Door_M (160 cm) no cabe: la validación lo tiene que rechazar.
		if (Seed <= 100)
		{
			FNDHouseGenParams WideParams = Params;
			WideParams.CarryEnvelopeWidth = 200.f;
			TArray<FString> Errors;
			TestFalse(FString::Printf(TEXT("Semilla %lld: bulto de 200 cm rechazado"), Seed), FNDHouseValidator::Validate(Layout, WideParams, Errors));
		}
	}

	// Un baño (Door_S, 100 cm) nunca está en la ruta del bulto.
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(1);
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			TestFalse(TEXT("Door_S no deja pasar el bulto"), NDHouseGen::GetDefaultSocketWidth(ENDRoomSocketType::Door_S) >= Params.CarryEnvelopeWidth);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenFloorsTest, "NDE.HouseGen.Floors", NDHouseGenTests::Flags)

bool FNDHouseGenFloorsTest::RunTest(const FString& Parameters)
{
	// 10.000 semillas por tamaño pidiendo siempre piso superior y sótano: el validador comprueba
	// conectividad entre los tres niveles, pisos apoyados, escaleras completas y ruta del bulto.
	constexpr int32 SeedCount = 10000;
	for (const ENDHouseSize Size : NDHouseGenTests::AllSizes)
	{
		int32 FallbackCount = 0;
		int32 FailureCount = 0;
		int32 WithUpper = 0;
		int32 WithBasement = 0;
		int32 BedroomUpstairs = 0;
		int32 TwoStairs = 0;
		int32 TwoStairsConnected = 0;
		int32 BasementForCreature = 0;
		for (int64 Seed = 1; Seed <= SeedCount; ++Seed)
		{
			FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed, Size);
			Params.ApplyFloorOptions(ENDFloorOption::Always, ENDFloorOption::Always);
			FNDHouseGenerationManifest Manifest;
			FNDHouseLayout Layout;
			if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
			{
				++FailureCount;
				continue;
			}
			TArray<FString> Errors;
			if (!FNDHouseValidator::Validate(Layout, Params, Errors))
			{
				AddError(FString::Printf(TEXT("%s semilla %lld: %s"), *NDHouseGenTests::SizeName(Size), Seed, *FString::Join(Errors, TEXT("; "))));
				++FailureCount;
			}
			int32 Lowest = 0;
			int32 Highest = 0;
			Layout.GetFloorRange(Lowest, Highest);
			FallbackCount += Manifest.bUsedFallback ? 1 : 0;
			WithUpper += Highest > 0 ? 1 : 0;
			WithBasement += Lowest < 0 ? 1 : 0;
			BedroomUpstairs += Layout.Rooms.IsValidIndex(Layout.BedroomRoom) && Layout.Rooms[Layout.BedroomRoom].Floor > 0 ? 1 : 0;
			TArray<int32> UpperTops;
			for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
			{
				if (Layout.Rooms[RoomIndex].Def->StairPart == ENDStairPart::Top && Layout.Rooms[RoomIndex].Floor == NDHouseGen::UpperFloor)
				{
					UpperTops.Add(RoomIndex);
				}
			}
			TwoStairs += UpperTops.Num() >= 2 ? 1 : 0;
			TwoStairsConnected += UpperTops.Num() >= 2 && NDHouseGenTests::AreConnectedOnFloor(Layout, UpperTops[0], UpperTops[1], NDHouseGen::UpperFloor) ? 1 : 0;
			// Sótano alcanzable por la criatura: todas sus salas sin pasos bajos.
			if (Lowest < 0)
			{
				bool bAllReachable = true;
				for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
				{
					bAllReachable &= Layout.Rooms[RoomIndex].Floor >= 0 || Layout.FindRoute(Layout.EntryRoom, RoomIndex, 0.f, nullptr, true);
				}
				BasementForCreature += bAllReachable ? 1 : 0;
			}
			if (FailureCount > 20)
			{
				AddError(TEXT("Demasiados fallos; se para la prueba"));
				break;
			}
		}
		auto Percent = [](int32 Part, int32 Whole) { return Whole > 0 ? Part * 100.f / Whole : 0.f; };
		AddInfo(FString::Printf(TEXT("%s con pisos, %d semillas: %d con piso superior, %d con sotano | dormitorio arriba %.1f %% | dos escaleras %d, unidas arriba %.1f %% de las casas con piso superior | sotano alcanzable por la criatura %.1f %% | casas de respaldo %.2f %%"),
			*NDHouseGenTests::SizeName(Size), SeedCount, WithUpper, WithBasement, Percent(BedroomUpstairs, WithUpper), TwoStairs,
			Percent(TwoStairsConnected, WithUpper), Percent(BasementForCreature, WithBasement), Percent(FallbackCount, SeedCount)));
		TestTrue(FString::Printf(TEXT("%s con pisos: casa de respaldo en menos del 1%% de semillas"), *NDHouseGenTests::SizeName(Size)), FallbackCount * 100 < SeedCount);
	}

	// Determinismo con pisos: misma semilla, misma casa.
	for (int64 Seed = 1; Seed <= 300; ++Seed)
	{
		FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(Seed);
		Params.ApplyFloorOptions(ENDFloorOption::Always, ENDFloorOption::Always);
		FNDHouseGenerationManifest First;
		FNDHouseGenerationManifest Second;
		FNDHouseGenerator::Generate(Params, First);
		FNDHouseGenerator::Generate(Params, Second);
		if (First.ToDebugString() != Second.ToDebugString())
		{
			AddError(FString::Printf(TEXT("Semilla %lld con pisos da dos casas distintas"), Seed));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenLootPlanTest, "NDE.HouseGen.LootPlan", NDHouseGenTests::Flags)

bool FNDHouseGenLootPlanTest::RunTest(const FString& Parameters)
{
	// Botín del corte gris: id, categoría y valor (los Data Assets de Content/_NDE_/Data/Loot).
	const TArray<FNDHouseLootCandidate> Candidates = {
		{ TEXT("L01"), ENDLootCategory::Pocket, 85 }, { TEXT("L02"), ENDLootCategory::Pocket, 70 },
		{ TEXT("L03"), ENDLootCategory::Pocket, 120 }, { TEXT("L07"), ENDLootCategory::Light, 120 },
		{ TEXT("L08"), ENDLootCategory::Light, 180 }, { TEXT("L11"), ENDLootCategory::Light, 190 },
		{ TEXT("L12"), ENDLootCategory::Medium, 250 }, { TEXT("L16"), ENDLootCategory::Heavy, 520 },
		{ TEXT("L19"), ENDLootCategory::Heavy, 670 }, { TEXT("L32"), ENDLootCategory::Medium, 390 } };
	const TArray<FName> Required = { TEXT("L16") };
	constexpr int32 Quota = 700;

	int32 ShortHouses = 0;
	for (int64 Seed = 1; Seed <= 300; ++Seed)
	{
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (!NDHouseGenTests::GenerateLayout(*this, NDHouseGenTests::MakeMixedParams(Seed), Manifest, Layout))
		{
			return false;
		}

		TArray<FNDHouseLootPlacement> PlanA;
		TArray<FNDHouseLootPlacement> PlanB;
		FNDHouseLootContractSpec Spec;
		Spec.Quota = Quota;
		Spec.RequiredLootIds = Required;
		FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Spec, Candidates, PlanA);
		FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Spec, Candidates, PlanB);

		bool bSame = PlanA.Num() == PlanB.Num();
		for (int32 Index = 0; bSame && Index < PlanA.Num(); ++Index)
		{
			bSame = PlanA[Index].MarkerIndex == PlanB[Index].MarkerIndex && PlanA[Index].Candidate == PlanB[Index].Candidate
				&& PlanA[Index].LootGuid == PlanB[Index].LootGuid;
		}
		if (!bSame)
		{
			AddError(FString::Printf(TEXT("Semilla %lld: el reparto del botin no es determinista"), Seed));
			return false;
		}

		TSet<FGuid> Guids;
		int32 Value = 0;
		int32 FreeSockets = 0;
		for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
		{
			FreeSockets += Marker.Type == ENDRoomMarkerType::LootSocket ? 1 : 0;
		}
		for (const FNDHouseLootPlacement& Placement : PlanA)
		{
			const FNDHouseLootCandidate& Candidate = Candidates[Placement.Candidate];
			Value += Candidate.Value;
			FreeSockets -= Placement.MarkerIndex != INDEX_NONE ? 1 : 0;
			if (Guids.Contains(Placement.LootGuid))
			{
				AddError(FString::Printf(TEXT("Semilla %lld: LootGuid repetido"), Seed));
			}
			Guids.Add(Placement.LootGuid);
			if (Placement.bRequired && Placement.Room != Layout.ObjectiveRoom)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: el objeto del contrato no esta en la sala objetivo"), Seed));
			}
			if (!Placement.bRequired && Candidate.Category == ENDLootCategory::Heavy && Layout.Rooms[Placement.Room].Floor != 0)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: un pesado extra fuera de la planta baja"), Seed));
			}
		}
		if (!PlanA.ContainsByPredicate([](const FNDHouseLootPlacement& P) { return P.bRequired; }))
		{
			AddError(FString::Printf(TEXT("Semilla %lld: falta el objeto del contrato"), Seed));
		}
		// Valor de sobra para la cuota, salvo que no queden huecos.
		if (Value < Quota)
		{
			AddError(FString::Printf(TEXT("Semilla %lld: solo %d puntos de botin para una cuota de %d"), Seed, Value, Quota));
		}
		ShortHouses += Value < FNDHouseLootPlanner::GetTargetValue(Quota) && FreeSockets > 0 ? 1 : 0;
	}
	TestEqual(TEXT("Casas sin el margen de cuota pese a tener huecos libres"), ShortHouses, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenGameplayPointsTest, "NDE.HouseGen.GameplayPoints", NDHouseGenTests::Flags)

bool FNDHouseGenGameplayPointsTest::RunTest(const FString& Parameters)
{
	int32 Houses = 0;
	int32 TotalHiding = 0;
	int32 TotalHazards = 0;
	for (int64 Seed = 1; Seed <= 600; ++Seed)
	{
		FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(Seed);
		// Un tercio con piso superior y sótano, para probar escaleras en la patrulla y el sonido.
		if (Seed % 3 == 0)
		{
			Params.ApplyFloorOptions(ENDFloorOption::Always, ENDFloorOption::Always);
		}
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			return false;
		}
		++Houses;
		TotalHiding += Layout.HidingSpots.Num();
		TotalHazards += Layout.Hazards.Num();

		// Mismo manifiesto, mismos puntos (lo que construye un cliente).
		FNDHouseLayout Again;
		FString Error;
		FNDHouseLayout::FromManifest(Manifest, Again, Error);
		const bool bSame = Again.HidingSpots.Num() == Layout.HidingSpots.Num() && Again.Hazards.Num() == Layout.Hazards.Num()
			&& Again.PatrolRoute == Layout.PatrolRoute
			&& (Layout.Hazards.IsEmpty() || Again.Hazards.Last().LotTransform.Equals(Layout.Hazards.Last().LotTransform));
		if (!bSame)
		{
			AddError(FString::Printf(TEXT("Semilla %lld: los puntos de juego no salen iguales del mismo manifiesto"), Seed));
			return false;
		}

		// Escondites con entrada dentro de su sala.
		if (Layout.HidingSpots.Num() < 2)
		{
			AddError(FString::Printf(TEXT("Semilla %lld: %d escondites"), Seed, Layout.HidingSpots.Num()));
		}
		for (const FNDHouseHidingSpot& Spot : Layout.HidingSpots)
		{
			if (Layout.FindRoomAt(Spot.EntryPoint) != Spot.Room || Spot.Capacity < 1 || Spot.Capacity > 2)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: escondite %d mal colocado"), Seed, Spot.StableId));
			}
		}

		// Patrulla: todas las salas salvo las de puerta pequeña (baños), empieza y acaba en el dormitorio.
		TSet<int32> Visited(Layout.PatrolRoute);
		const int32 SmallDoorRooms = Layout.Rooms.FilterByPredicate([&Layout](const FNDHouseLayoutRoom& Room)
		{
			return !Layout.Connections.ContainsByPredicate([&Layout, &Room](const FNDHouseLayoutConnection& Link)
			{
				return (Link.RoomA == Room.InstanceId || Link.RoomB == Room.InstanceId) && !Layout.IsLowClearance(Link) && Link.OpeningWidth >= 140.f;
			});
		}).Num();
		if (Visited.Num() + SmallDoorRooms < Layout.Rooms.Num() || Layout.PatrolRoute.IsEmpty()
			|| Layout.PatrolRoute[0] != Layout.BedroomRoom || Layout.PatrolRoute.Last() != Layout.BedroomRoom)
		{
			AddError(FString::Printf(TEXT("Semilla %lld: la patrulla recorre %d de %d salas"), Seed, Visited.Num(), Layout.Rooms.Num()));
		}

		// Peligros fuera de la ruta del bulto.
		TSet<int32> RouteRooms;
		Layout.GetCarryRouteRooms(Params.CarryEnvelopeWidth, RouteRooms);
		for (const FNDHouseHazard& Hazard : Layout.Hazards)
		{
			if (RouteRooms.Contains(Hazard.Room))
			{
				AddError(FString::Printf(TEXT("Semilla %lld: peligro en la ruta del bulto"), Seed));
			}
		}

		// Sonido: 1 en la misma sala, simétrico, y entre 0 y 1 hacia el dormitorio.
		const FVector Entry = Layout.PatrolNodes[Layout.EntryRoom].Location + FVector(0.f, 0.f, 50.f);
		const FVector Bedroom = Layout.PatrolNodes[Layout.BedroomRoom].Location + FVector(0.f, 0.f, 50.f);
		const float Same = Layout.GetSoundTransmission(Entry, Entry);
		const float There = Layout.GetSoundTransmission(Entry, Bedroom);
		const float Back = Layout.GetSoundTransmission(Bedroom, Entry);
		if (!FMath::IsNearlyEqual(Same, 1.f, 1e-4f) || There <= 0.f || There >= 1.f || !FMath::IsNearlyEqual(There, Back, 1e-4f))
		{
			AddError(FString::Printf(TEXT("Semilla %lld: sonido entrada-dormitorio %.3f / %.3f (misma sala %.3f)"), Seed, There, Back, Same));
		}
	}
	AddInfo(FString::Printf(TEXT("%d casas: %.1f escondites y %.1f peligros de media"), Houses,
		TotalHiding / static_cast<float>(Houses), TotalHazards / static_cast<float>(Houses)));
	return true;
}

namespace NDHouseGenContractTests
{
	const TCHAR* ContractIds[] = { TEXT("C01"), TEXT("C02"), TEXT("C03"), TEXT("C04"), TEXT("C05"), TEXT("C06"), TEXT("C07") };
	const TCHAR* LootIds[] = { TEXT("L01"), TEXT("L02"), TEXT("L03"), TEXT("L04"), TEXT("L05"), TEXT("L07"), TEXT("L08"), TEXT("L09"), TEXT("L10"),
		TEXT("L11"), TEXT("L12"), TEXT("L16"), TEXT("L19"), TEXT("L20"), TEXT("L32"), TEXT("L33"), TEXT("L34"), TEXT("L41"), TEXT("L50") };

	template <typename T>
	T* LoadData(const TCHAR* Folder, const TCHAR* Prefix, const TCHAR* Id)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/_NDE_/Data/%s/%s%s.%s%s"), Folder, Prefix, Id, Prefix, Id));
	}

	/** Botín de los Data Assets reales. */
	bool LoadCandidates(FAutomationTestBase& Test, TArray<FNDHouseLootCandidate>& OutCandidates)
	{
		for (const TCHAR* Id : LootIds)
		{
			const UNDLootDefinition* Loot = LoadData<UNDLootDefinition>(TEXT("Loot"), TEXT("DA_Loot_"), Id);
			if (!Loot)
			{
				Test.AddError(FString::Printf(TEXT("Falta DA_Loot_%s (ejecuta Scripts/nde_setup_contratos_d01.py)"), Id));
				return false;
			}
			OutCandidates.Add({ Loot->LootId, Loot->Category, Loot->BaseValue });
		}
		return true;
	}

	/** Recuento por contrato de lo que vigilan las pruebas. */
	struct FStats
	{
		int32 Houses = 0;
		int32 Fallbacks = 0;
		int32 ObjectiveUnreachable = 0;
		int32 QuotaShort = 0;
		int32 CarryRouteMissing = 0;
		int32 RescueInvalid = 0;
	};

	/** Genera casas de todos los tamaños con cada contrato y reparte su botín. */
	bool Run(FAutomationTestBase& Test, int32 SeedsPerSize, TMap<FString, FStats>& OutStats)
	{
		TArray<FNDHouseLootCandidate> Candidates;
		if (!LoadCandidates(Test, Candidates))
		{
			return false;
		}
		for (const TCHAR* ContractId : ContractIds)
		{
			const UNDContractDefinition* Contract = LoadData<UNDContractDefinition>(TEXT("Contracts"), TEXT("DA_Contract_"), ContractId);
			if (!Contract)
			{
				Test.AddError(FString::Printf(TEXT("Falta DA_Contract_%s"), ContractId));
				return false;
			}
			const FNDHouseLootContractSpec Spec = FNDHouseLootContractSpec::FromContract(Contract);
			FStats& Stats = OutStats.Add(ContractId);
			for (const ENDHouseSize Size : NDHouseGenTests::AllSizes)
			{
				for (int64 Seed = 1; Seed <= SeedsPerSize; ++Seed)
				{
					FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed, Size);
					Params.ContractId = Contract->ContractId;
					Params.ContractRequirements = Contract->HouseRequirements;
					FNDHouseGenerationManifest Manifest;
					FNDHouseLayout Layout;
					if (!NDHouseGenTests::GenerateLayout(Test, Params, Manifest, Layout))
					{
						return false;
					}
					++Stats.Houses;
					Stats.Fallbacks += Manifest.bUsedFallback ? 1 : 0;

					TArray<FNDHouseLootPlacement> Placements;
					FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Spec, Candidates, Placements);

					// Objetivo: todos los requeridos en la sala del contrato, a la que se llega.
					const int32 Anchor = Layout.GetAnchorRoom(Spec.House);
					TArray<int32> Depths;
					Layout.ComputeDepths(Depths);
					int32 RequiredPlaced = 0;
					for (const FNDHouseLootPlacement& Placement : Placements)
					{
						if (Placement.bRequired)
						{
							++RequiredPlaced;
							if (Placement.Room != Anchor || !Depths.IsValidIndex(Anchor) || Depths[Anchor] == INDEX_NONE)
							{
								++Stats.ObjectiveUnreachable;
							}
						}
					}
					Stats.ObjectiveUnreachable += RequiredPlaced < Spec.RequiredLootIds.Num() ? 1 : 0;

					// Cuota cubierta con margen (sin contar señuelos).
					int32 Value = 0;
					for (const FNDHouseLootPlacement& Placement : Placements)
					{
						Value += Placement.bDecoy ? 0 : Candidates[Placement.Candidate].Value;
					}
					if (Spec.Quota > 0 && Value < Spec.Quota * Spec.House.QuotaCoverage)
					{
						++Stats.QuotaShort;
					}

					// Ruta del bulto hasta el objetivo con el ancho que pida el contrato (o el del bulto pesado).
					const bool bHeavy = Spec.RequiredLootIds.ContainsByPredicate([&Candidates](const FName& Id)
					{
						return Candidates.ContainsByPredicate([&Id](const FNDHouseLootCandidate& C) { return C.LootId == Id && C.Category == ENDLootCategory::Heavy; });
					});
					const float Width = FMath::Max(Spec.House.MinRouteWidth, bHeavy ? Params.CarryEnvelopeWidth : 0.f);
					if (!Layout.FindRoute(Layout.EntryRoom, Anchor, Width))
					{
						++Stats.CarryRouteMissing;
					}

					// Rescate: tarro con dos accesos, lejos de las puertas, y el objeto dentro.
					if (Spec.House.ObjectiveAnchor == ENDContractAnchor::RescueContainer)
					{
						const FNDHouseLootPlacement* Rescue = Placements.FindByPredicate([](const FNDHouseLootPlacement& P) { return P.bInRescueContainer; });
						bool bValid = Rescue && Layout.CountAccesses(Anchor) >= 2;
						if (Rescue)
						{
							const FNDHouseLayoutRoom& Room = Layout.Rooms[Rescue->Room];
							for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
							{
								bValid &= !Room.Sockets[SocketIndex].IsOpen()
									|| FVector::Dist2D(Layout.GetSocketLocation(Rescue->Room, SocketIndex), Rescue->LotTransform.GetLocation()) >= 100.f;
							}
						}
						Stats.RescueInvalid += bValid ? 0 : 1;
					}
				}
			}
			Test.AddInfo(FString::Printf(TEXT("%s: %d casas, %d de respaldo, objetivo inaccesible %d, cuota corta %d, sin ruta de bulto %d, rescate invalido %d"),
				ContractId, Stats.Houses, Stats.Fallbacks, Stats.ObjectiveUnreachable, Stats.QuotaShort, Stats.CarryRouteMissing, Stats.RescueInvalid));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenContractQuotaTest, "NDE.HouseGen.Contracts.TestQuotaCoverage", NDHouseGenTests::Flags)

bool FNDHouseGenContractQuotaTest::RunTest(const FString& Parameters)
{
	TMap<FString, NDHouseGenContractTests::FStats> Stats;
	if (!NDHouseGenContractTests::Run(*this, 60, Stats))
	{
		return false;
	}
	for (const TPair<FString, NDHouseGenContractTests::FStats>& Pair : Stats)
	{
		TestEqual(FString::Printf(TEXT("%s: casas con cuota corta"), *Pair.Key), Pair.Value.QuotaShort, 0);
		TestEqual(FString::Printf(TEXT("%s: objetivo inaccesible"), *Pair.Key), Pair.Value.ObjectiveUnreachable, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenContractRescueTest, "NDE.HouseGen.Contracts.TestRescueContainer", NDHouseGenTests::Flags)

bool FNDHouseGenContractRescueTest::RunTest(const FString& Parameters)
{
	TMap<FString, NDHouseGenContractTests::FStats> Stats;
	if (!NDHouseGenContractTests::Run(*this, 60, Stats))
	{
		return false;
	}
	TestEqual(TEXT("C06: tarros sin dos accesos, tapando una puerta o sin Trasto"), Stats.FindRef(TEXT("C06")).RescueInvalid, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenContractCarryTest, "NDE.HouseGen.Contracts.TestCarryEnvelope", NDHouseGenTests::Flags)

bool FNDHouseGenContractCarryTest::RunTest(const FString& Parameters)
{
	TMap<FString, NDHouseGenContractTests::FStats> Stats;
	if (!NDHouseGenContractTests::Run(*this, 60, Stats))
	{
		return false;
	}
	for (const TPair<FString, NDHouseGenContractTests::FStats>& Pair : Stats)
	{
		TestEqual(FString::Printf(TEXT("%s: sin ruta de bulto al objetivo"), *Pair.Key), Pair.Value.CarryRouteMissing, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDHouseGenVariantsTest, "NDE.HouseGen.VariantsAndDecorations", NDHouseGenTests::Flags)

bool FNDHouseGenVariantsTest::RunTest(const FString& Parameters)
{
	// Cada familia de salas de H01 (salvo escaleras, cuyas variantes son los tres tipos) tiene 2-5 variantes.
	TMap<ENDRoomType, int32> Families;
	for (const FNDRoomModuleDefinition& Def : FNDRoomCatalog::GetModules())
	{
		if (Def.StairPart == ENDStairPart::None)
		{
			++Families.FindOrAdd(Def.RoomType);
		}
	}
	for (const TPair<ENDRoomType, int32>& Family : Families)
	{
		TestTrue(FString::Printf(TEXT("%s tiene %d variantes (2-5)"), *NDHouseGen::GetRoomTypeName(Family.Key), Family.Value), Family.Value >= 2 && Family.Value <= 5);
	}

	TSet<FName> UsedModules;
	int32 Rooms = 0;
	int32 Decorated = 0;
	int32 DecorationCount = 0;
	TMap<FName, int32> EmptyByModule;
	for (int64 Seed = 1; Seed <= 400; ++Seed)
	{
		FNDHouseGenParams Params = NDHouseGenTests::MakeMixedParams(Seed);
		if (Seed % 2 == 0)
		{
			Params.ApplyFloorOptions(ENDFloorOption::Always, ENDFloorOption::Always);
		}
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			return false;
		}
		for (const FNDHouseLayoutRoom& Room : Layout.Rooms)
		{
			UsedModules.Add(Room.Def->RoomModuleId);
		}

		// Muebles: nunca junto a una puerta, en la ruta puerta-centro ni encima del botín.
		// Se mira la huella del mueble: ni la puerta, ni su ruta al centro, ni el centro, ni el botín caen dentro.
		for (const FNDHouseDecoration& Decoration : Layout.Decorations)
		{
			if (!Decoration.bCollision)
			{
				continue;
			}
			const FBox2D Footprint = Decoration.GetFootprint();
			const float Z = Decoration.LotTransform.GetLocation().Z;
			const FNDHouseLayoutRoom& Room = Layout.Rooms[Decoration.Room];
			const FVector2D Hub(Layout.PatrolNodes[Decoration.Room].Location);
			bool bBlocks = Footprint.IsInside(Hub);
			for (int32 SocketIndex = 0; SocketIndex < Room.Sockets.Num(); ++SocketIndex)
			{
				if (Room.Sockets[SocketIndex].IsOpen())
				{
					const FVector2D Door(Layout.GetSocketLocation(Decoration.Room, SocketIndex));
					for (int32 Step = 0; Step <= 20 && !bBlocks; ++Step)
					{
						bBlocks = Footprint.IsInside(FMath::Lerp(Door, Hub, Step / 20.f));
					}
				}
			}
			// El frente (-Y del mueble; MeshYaw ya gira la malla para eso) mira a la sala, nunca a una pared.
			const FVector Front = Decoration.LotTransform.GetRotation().RotateVector(FVector(0.f, -1.f, 0.f));
			const FVector FrontPoint = Decoration.LotTransform.GetLocation() + Front * (Decoration.HalfExtent.Y + 30.f);
			if (Layout.FindRoomAt(FrontPoint) != Decoration.Room)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: mueble %d (%s) con el frente contra una pared"), Seed, Decoration.StableId,
					NDHouseFurniture::GetArt().IsValidIndex(Decoration.Art) ? *NDHouseFurniture::GetArt()[Decoration.Art].Id.ToString() : TEXT("greybox")));
			}
			if (bBlocks)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: mueble %d tapa una puerta o la ruta de la sala %d"), Seed, Decoration.StableId, Decoration.Room));
			}
			for (const FNDHouseLayoutMarker& Marker : Layout.Markers)
			{
				if (Marker.Type == ENDRoomMarkerType::LootSocket && FMath::Abs(Marker.LotTransform.GetLocation().Z - Z) < 200.f
					&& Footprint.IsInside(FVector2D(Marker.LotTransform.GetLocation())))
				{
					AddError(FString::Printf(TEXT("Semilla %lld: mueble %d encima de un hueco de botin"), Seed, Decoration.StableId));
				}
			}
		}
		for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
		{
			const FNDHouseLayoutRoom& Room = Layout.Rooms[RoomIndex];
			if (RoomIndex == Layout.EntryRoom || Room.Def->StairPart != ENDStairPart::None || Room.Def->RoomType == ENDRoomType::R02_Corridor)
			{
				continue;
			}
			++Rooms;
			const int32 InRoom = Layout.Decorations.FilterByPredicate([RoomIndex](const FNDHouseDecoration& D) { return D.Room == RoomIndex && D.bCollision; }).Num();
			Decorated += InRoom > 0 ? 1 : 0;
			if (InRoom == 0)
			{
				++EmptyByModule.FindOrAdd(Room.Def->RoomModuleId);
			}
			DecorationCount += InRoom;
			// Hasta 8 muebles grandes (más sus sillas) en las salas de 4 celdas.
			if (InRoom > 14)
			{
				AddError(FString::Printf(TEXT("Semilla %lld: %d muebles en la sala %d"), Seed, InRoom, RoomIndex));
			}
		}
	}
	const int32 CatalogModules = FNDRoomCatalog::GetModules().Num();
	AddInfo(FString::Printf(TEXT("Modulos usados: %d de %d. Salas con muebles: %d de %d (%.1f muebles de media)"),
		UsedModules.Num(), CatalogModules, Decorated, Rooms, DecorationCount / static_cast<float>(FMath::Max(1, Rooms))));
	AddInfo(FString::Printf(TEXT("Salas sin muebles: %s"), *FString::JoinBy(EmptyByModule, TEXT(", "), [](const TPair<FName, int32>& Pair) { return FString::Printf(TEXT("%s %d"), *Pair.Key.ToString(), Pair.Value); })));
	TestTrue(TEXT("La mayoría de salas tiene muebles"), Decorated >= Rooms * 0.8f);
	return true;
}

#endif
