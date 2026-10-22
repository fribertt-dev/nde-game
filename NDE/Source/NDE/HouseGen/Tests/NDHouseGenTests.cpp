// No Despiertes Eso

#include "HouseGen/NDHouseGenerator.h"
#include "HouseGen/NDRoomCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NDHouseGenTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** Reglas de H01 con la fachada girando según la semilla, para probar las cuatro orientaciones. */
	FNDHouseGenParams MakeParams(int64 Seed)
	{
		FNDHouseGenParams Params = FNDHouseGenParams::ForArchetype(TEXT("H01"));
		Params.Seed = Seed;
		Params.Lot.FrontDirection = static_cast<ENDCellDirection>(Seed % 4);
		return Params;
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
			const bool bInside = Outside.X >= 0 && Outside.Y >= 0 && Outside.X < Def.FootprintCells.X && Outside.Y < Def.FootprintCells.Y;
			TestFalse(FString::Printf(TEXT("%s.%s mira hacia fuera"), *Def.RoomModuleId.ToString(), *Socket.SocketId.ToString()), bInside);
			bool bDuplicate = false;
			Ids.Add(Socket.SocketId, &bDuplicate);
			TestFalse(FString::Printf(TEXT("%s.%s id unico"), *Def.RoomModuleId.ToString(), *Socket.SocketId.ToString()), bDuplicate);
		}
	}

	// La casa de respaldo también tiene que ser válida, en las cuatro orientaciones.
	for (int32 Front = 0; Front < 4; ++Front)
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Front);
		FNDHouseGenerationManifest Fallback;
		FNDHouseLayout Layout;
		FString Error;
		TArray<FString> Errors;
		if (!TestTrue(TEXT("Casa de respaldo se construye"), FNDHouseGenerator::BuildFallback(Params, Fallback) && FNDHouseLayout::FromManifest(Fallback, Layout, Error)))
		{
			continue;
		}
		const bool bValid = FNDHouseValidator::Validate(Layout, Params, Errors);
		TestTrue(FString::Printf(TEXT("Casa de respaldo valida (fachada %d): %s"), Front, *FString::Join(Errors, TEXT("; "))), bValid);
	}

	constexpr int32 SeedCount = 10000;
	int32 FallbackCount = 0;
	int32 FailureCount = 0;
	int32 LoopCount = 0;
	int32 SecondaryExitCount = 0;
	for (int64 Seed = 1; Seed <= SeedCount; ++Seed)
	{
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed);
		FNDHouseGenerationManifest Manifest;
		FNDHouseLayout Layout;
		if (!NDHouseGenTests::GenerateLayout(*this, Params, Manifest, Layout))
		{
			++FailureCount;
			continue;
		}
		// Conexo desde la entrada, dormitorio y objetivo a su profundidad, sin solapes, dentro del lote.
		TArray<FString> Errors;
		if (!FNDHouseValidator::Validate(Layout, Params, Errors))
		{
			AddError(FString::Printf(TEXT("Semilla %lld: %s"), Seed, *FString::Join(Errors, TEXT("; "))));
			++FailureCount;
		}
		FallbackCount += Manifest.bUsedFallback ? 1 : 0;
		LoopCount += Manifest.Connections.FilterByPredicate([](const FNDHouseManifestConnection& L) { return L.bIsLoop; }).Num();
		SecondaryExitCount += Manifest.SecondaryExitRoom != INDEX_NONE ? 1 : 0;
		if (FailureCount > 20)
		{
			AddError(TEXT("Demasiados fallos; se para la prueba"));
			break;
		}
	}
	AddInfo(FString::Printf(TEXT("%d semillas: %d casas de respaldo, %d bucles, %d accesos secundarios"), SeedCount, FallbackCount, LoopCount, SecondaryExitCount));
	TestTrue(TEXT("Casa de respaldo en menos del 1% de semillas"), FallbackCount * 100 < SeedCount);

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
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed);
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
		FNDHouseGenParams Params = NDHouseGenTests::MakeParams(FMath::Abs(Seed));
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
		const FNDHouseGenParams Params = NDHouseGenTests::MakeParams(Seed);
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

#endif
