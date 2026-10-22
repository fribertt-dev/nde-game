// Pruebas automáticas de las reglas puras del bucle (Session Frontend > Automation > NDE).

#include "Misc/AutomationTest.h"
#include "Contracts/NDSettlementLibrary.h"
#include "Expedition/NDNightClockComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NDExpeditionTests
{
	FNDDepositRecord MakeRecord(int32 BaseValue, ENDLootCondition Condition, float Factor)
	{
		FNDDepositRecord Record;
		Record.LootGuid = FGuid::NewGuid();
		Record.BaseValue = BaseValue;
		Record.Condition = Condition;
		Record.ConditionFactor = Factor;
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDNightClockPhaseTest, "NDE.Expedition.NightClockPhases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDNightClockPhaseTest::RunTest(const FString& Parameters)
{
	auto Phase = [](double Elapsed, bool bDawn = true)
	{
		return UNDNightClockComponent::ComputePhase(Elapsed, 600.f, 45.f, 0.5f, 0.8f, bDawn);
	};

	TestTrue(TEXT("Inicio: noche profunda"), Phase(0.0) == ENDNightPhase::DeepNight);
	TestTrue(TEXT("49%: noche profunda"), Phase(299.0) == ENDNightPhase::DeepNight);
	TestTrue(TEXT("50%: madrugada"), Phase(300.0) == ENDNightPhase::Madrugada);
	TestTrue(TEXT("80%: pre-amanecer"), Phase(480.0) == ENDNightPhase::PreDawn);
	TestTrue(TEXT("\u00DAltimo segundo de noche"), Phase(599.9) == ENDNightPhase::PreDawn);
	TestTrue(TEXT("100%: amanecer"), Phase(600.0) == ENDNightPhase::DawnGrace);
	TestTrue(TEXT("\u00DAltimo segundo de la ventana"), Phase(644.9) == ENDNightPhase::DawnGrace);
	TestTrue(TEXT("+45 s: fin del d\u00EDa"), Phase(645.0) == ENDNightPhase::DayEnded);
	TestTrue(TEXT("Sin amanecer se queda en pre-amanecer"), Phase(10000.0, false) == ENDNightPhase::PreDawn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDLootValueTest, "NDE.Expedition.LootValue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDLootValueTest::RunTest(const FString& Parameters)
{
	using namespace NDExpeditionTests;
	const TArray<FNDDepositRecord> Deposits = {
		MakeRecord(100, ENDLootCondition::Intact, 1.f),
		MakeRecord(200, ENDLootCondition::Damaged, 0.65f),
		MakeRecord(190, ENDLootCondition::Broken, 0.2f),
	};

	TestEqual(TEXT("Puntos v\u00E1lidos"), UNDSettlementLibrary::ComputeLootPoints(Deposits), 100 + 130 + 38);
	TestEqual(TEXT("Valor vendido x1"), UNDSettlementLibrary::ComputeLootSoldValue(Deposits, 1.f), 268);
	TestEqual(TEXT("Valor vendido x1.5"), UNDSettlementLibrary::ComputeLootSoldValue(Deposits, 1.5f), 402);

	FNDEconomyTuning Tuning;
	TestEqual(TEXT("Bono extra: 300 sobre cuota = 3 pasos"), UNDSettlementLibrary::ComputeExtraLootBonus(1000, 700, 150, Tuning), 30);
	TestEqual(TEXT("Bono extra con tope del 35%"), UNDSettlementLibrary::ComputeExtraLootBonus(3000, 700, 150, Tuning), 52);
	TestEqual(TEXT("Sin excedente no hay bono"), UNDSettlementLibrary::ComputeExtraLootBonus(500, 700, 150, Tuning), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDOutcomeTest, "NDE.Expedition.Outcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDOutcomeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Todos fuera + secundario = perfecto"), UNDSettlementLibrary::DetermineOutcome(true, true, 4, 4) == ENDExpeditionOutcome::Perfect);
	TestTrue(TEXT("Sin secundario = \u00E9xito"), UNDSettlementLibrary::DetermineOutcome(true, false, 4, 4) == ENDExpeditionOutcome::Success);
	TestTrue(TEXT("Uno se qued\u00F3 atr\u00E1s = \u00E9xito"), UNDSettlementLibrary::DetermineOutcome(true, true, 3, 4) == ENDExpeditionOutcome::Success);
	TestTrue(TEXT("Sin objetivo pero con extra\u00EDdos = parcial"), UNDSettlementLibrary::DetermineOutcome(false, true, 2, 4) == ENDExpeditionOutcome::PartialExtraction);
	TestTrue(TEXT("Bot\u00EDn en camioneta sin extra\u00EDdos = fracaso"), UNDSettlementLibrary::DetermineOutcome(true, true, 0, 4) == ENDExpeditionOutcome::Failure);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDPlayerRewardTest, "NDE.Expedition.PlayerReward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDPlayerRewardTest::RunTest(const FString& Parameters)
{
	const FNDEconomyTuning Tuning;
	auto Reward = [&Tuning](ENDExpeditionOutcome Outcome, ENDParticipantStatus Status, bool bSecondary, bool bRescue)
	{
		return UNDSettlementLibrary::ComputePlayerReward(Outcome, Status, bSecondary, bRescue, 1000, 700, 150, 45, Tuning);
	};

	const FNDPlayerReward Extracted = Reward(ENDExpeditionOutcome::Perfect, ENDParticipantStatus::Extracted, true, false);
	TestEqual(TEXT("Extra\u00EDdo: base + extra + secundario"), Extracted.Coins, 150 + 30 + 23);
	TestEqual(TEXT("Extra\u00EDdo: reputaci\u00F3n + secundario"), Extracted.Reputation, 45 + 9);

	const FNDPlayerReward Rescued = Reward(ENDExpeditionOutcome::Success, ENDParticipantStatus::Extracted, false, true);
	TestEqual(TEXT("Bono de rescate de equipo"), Rescued.Coins, 150 + 30 + 11);

	const FNDPlayerReward LeftBehind = Reward(ENDExpeditionOutcome::Success, ENDParticipantStatus::LeftBehind, true, true);
	TestEqual(TEXT("Atr\u00E1s: pierde bonos personales"), LeftBehind.Coins, 150 + 30);
	TestEqual(TEXT("Atr\u00E1s: reputaci\u00F3n base"), LeftBehind.Reputation, 45);

	const FNDPlayerReward Disconnected = Reward(ENDExpeditionOutcome::Success, ENDParticipantStatus::Disconnected, true, false);
	TestEqual(TEXT("Abandono: sin reputaci\u00F3n"), Disconnected.Reputation, 0);

	const FNDPlayerReward Partial = Reward(ENDExpeditionOutcome::PartialExtraction, ENDParticipantStatus::Extracted, true, false);
	TestEqual(TEXT("Parcial: 20% del valor vendido"), Partial.Coins, 200);
	TestEqual(TEXT("Parcial: sin reputaci\u00F3n"), Partial.Reputation, 0);

	const FNDPlayerReward Failure = Reward(ENDExpeditionOutcome::Failure, ENDParticipantStatus::LeftBehind, false, false);
	TestEqual(TEXT("Fracaso: 5% del bot\u00EDn depositado"), Failure.Coins, 50);
	return true;
}

#endif
