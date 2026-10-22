// No Despiertes Eso

#include "HouseGen/NDHouseGenerator.h"
#include "HouseGen/NDHouseLoot.h"
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
		FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Quota, Required, Candidates, PlanA);
		FNDHouseLootPlanner::Plan(Layout, Manifest.RunSeed, Quota, Required, Candidates, PlanB);

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

#endif
