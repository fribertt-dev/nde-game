#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "UI/Data/NDUIMockDataProvider.h"
#include "UI/NDUIFunctionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDUIMockSpeciesUniqueTest, "NDE.UI.Mock.SpeciesUniquePerMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNDUIMockSpeciesUniqueTest::RunTest(const FString& Parameters)
{
	UNDUIMockDataProvider* Provider = NewObject<UNDUIMockDataProvider>();

	// En el mock, otro jugador ya es Raccoon.
	Provider->RequestSelectSpecies(ENDDesveladoSpecies::Raccoon);
	TestTrue(TEXT("No se puede elegir una especie tomada"), Provider->GetLocalPlayer().Species == ENDDesveladoSpecies::None);

	Provider->RequestSelectSpecies(ENDDesveladoSpecies::Fox);
	TestTrue(TEXT("Se puede elegir una especie libre"), Provider->GetLocalPlayer().Species == ENDDesveladoSpecies::Fox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDUIMockReadyNeedsSpeciesTest, "NDE.UI.Mock.ReadyNeedsSpecies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNDUIMockReadyNeedsSpeciesTest::RunTest(const FString& Parameters)
{
	UNDUIMockDataProvider* Provider = NewObject<UNDUIMockDataProvider>();

	Provider->RequestSetReady(true);
	TestFalse(TEXT("Sin especie no se puede marcar listo"), Provider->GetLocalPlayer().bIsReady);

	Provider->RequestSelectSpecies(ENDDesveladoSpecies::Lemur);
	Provider->RequestSetReady(true);
	TestTrue(TEXT("Con especie sí"), Provider->GetLocalPlayer().bIsReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDUIMockPurchaseTest, "NDE.UI.Mock.Purchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNDUIMockPurchaseTest::RunTest(const FString& Parameters)
{
	UNDUIMockDataProvider* Provider = NewObject<UNDUIMockDataProvider>();
	const int32 StartMoney = Provider->GetLocalMoney();

	// T04 cuesta 220 y el mock empieza con 400.
	Provider->RequestPurchase(TEXT("T04"));
	TestEqual(TEXT("La compra descuenta el precio"), Provider->GetLocalMoney(), StartMoney - 220);

	// T07 cuesta 350: ya no alcanza.
	Provider->RequestPurchase(TEXT("T07"));
	TestEqual(TEXT("Sin fondos no se compra"), Provider->GetLocalMoney(), StartMoney - 220);

	Provider->RequestPurchase(TEXT("T04"));
	TestEqual(TEXT("No se compra dos veces"), Provider->GetLocalMoney(), StartMoney - 220);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDUIClockFormatTest, "NDE.UI.ClockFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNDUIClockFormatTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("10 minutos"), UNDUIFunctionLibrary::FormatClock(600.f).ToString(), FString(TEXT("10:00")));
	TestEqual(TEXT("Redondea hacia arriba"), UNDUIFunctionLibrary::FormatClock(0.2f).ToString(), FString(TEXT("0:01")));
	TestEqual(TEXT("Nunca negativo"), UNDUIFunctionLibrary::FormatClock(-5.f).ToString(), FString(TEXT("0:00")));

	FNDNightClockVM Clock;
	Clock.Phase = ENDNightPhase::DawnEscapeGrace;
	TestEqual(TEXT("Tras el amanecer no queda noche"), UNDUIFunctionLibrary::GetNightSecondsRemaining(nullptr, Clock), 0.f);
	return true;
}

#endif
