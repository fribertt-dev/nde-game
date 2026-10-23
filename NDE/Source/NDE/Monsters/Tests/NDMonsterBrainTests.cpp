// Pruebas automáticas de las cuentas de la criatura (Session Frontend > Automation > NDE.Monster).

#include "Misc/AutomationTest.h"
#include "Monsters/NDMonsterBrain.h"
#include "Monsters/NDMonsterDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NDMonsterTests
{
	/**
	 * Simula a un Desvelado que da pasos de Loudness cada StepSeconds a DistanceCm de la cama durante
	 * Seconds, con o sin ronquido. Devuelve los segundos hasta que se levanta (o -1 si no se levanta).
	 */
	float SecondsToWake(const UNDMonsterDefinition& Def, float Loudness, float StepSeconds, float DistanceCm, float Seconds, bool bSnore)
	{
		ENDMonsterState State = ENDMonsterState::DeepSleep;
		float Suspicion = 0.f;
		FRandomStream Rng(7);
		FNDSnoreCycle Cycle = NDMonsterBrain::MakeSnoreCycle(Def, State, 0.0, 0, Rng);
		const float Tick = 0.1f;
		float NextStep = 0.f;
		for (float Time = 0.f; Time < Seconds; Time += Tick)
		{
			if (Time >= Cycle.StartServerTime + Cycle.PeriodSeconds)
			{
				Cycle = NDMonsterBrain::MakeSnoreCycle(Def, State, Time, Cycle.CycleIndex + 1, Rng);
			}
			float Gain = 0.f;
			if (Time >= NextStep)
			{
				NextStep += StepSeconds;
				const float Mask = bSnore ? NDMonsterBrain::GetSnoreMask(Def, State, Cycle, Time, Loudness) : 1.f;
				const float Perceived = NDMonsterBrain::GetPerceivedLoudness(Def, Loudness, DistanceCm, 1.f, Mask);
				Gain = NDMonsterBrain::GetSuspicionGain(Def, State, Perceived);
			}
			Suspicion += Gain;
			if (Gain <= 0.f)
			{
				Suspicion -= NDMonsterBrain::GetDecayPerSecond(Def, State) * Tick;
			}
			Suspicion = FMath::Clamp(Suspicion, 0.f, 100.f);
			const ENDMonsterState Next = NDMonsterBrain::GetBedState(Def, State, Suspicion);
			if (Next == ENDMonsterState::WakingUp)
			{
				return Time;
			}
			State = Next;
		}
		return -1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDMonsterBedStatesTest, "NDE.Monster.BedStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDMonsterBedStatesTest::RunTest(const FString& Parameters)
{
	const UNDMonsterDefinition& Def = *GetDefault<UNDMonsterDefinition>();
	using namespace NDMonsterBrain;
	TestTrue(TEXT("0: sueño profundo"), GetBedState(Def, ENDMonsterState::DeepSleep, 0.f) == ENDMonsterState::DeepSleep);
	TestTrue(TEXT("25: sueño ligero"), GetBedState(Def, ENDMonsterState::DeepSleep, 25.f) == ENDMonsterState::LightSleep);
	TestTrue(TEXT("45 desde profundo: se revuelve"), GetBedState(Def, ENDMonsterState::DeepSleep, 45.f) == ENDMonsterState::Stirring);
	TestTrue(TEXT("65: medio despierto"), GetBedState(Def, ENDMonsterState::LightSleep, 65.f) == ENDMonsterState::HalfAwake);
	TestTrue(TEXT("80: se levanta"), GetBedState(Def, ENDMonsterState::HalfAwake, 80.f) == ENDMonsterState::WakingUp);
	// Histéresis: con 55 sigue medio despierto (umbral 60 - 10); con 49 baja.
	TestTrue(TEXT("Histéresis: 55 sigue medio despierto"), GetBedState(Def, ENDMonsterState::HalfAwake, 55.f) == ENDMonsterState::HalfAwake);
	TestTrue(TEXT("Histéresis: 49 vuelve a revolverse"), GetBedState(Def, ENDMonsterState::HalfAwake, 49.f) == ENDMonsterState::Stirring);
	TestTrue(TEXT("Histéresis: 20 sigue en sueño ligero"), GetBedState(Def, ENDMonsterState::LightSleep, 20.f) == ENDMonsterState::LightSleep);
	TestTrue(TEXT("Histéresis: 5 baja hasta profundo"), GetBedState(Def, ENDMonsterState::HalfAwake, 5.f) == ENDMonsterState::DeepSleep);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDMonsterSnoreTest, "NDE.Monster.Snore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDMonsterSnoreTest::RunTest(const FString& Parameters)
{
	const UNDMonsterDefinition& Def = *GetDefault<UNDMonsterDefinition>();
	using namespace NDMonsterBrain;
	FRandomStream Rng(3);
	for (int32 Index = 0; Index < 50; ++Index)
	{
		const FNDSnoreCycle Cycle = MakeSnoreCycle(Def, ENDMonsterState::DeepSleep, 100.0, Index, Rng);
		TestTrue(TEXT("Ronca en sueño profundo"), Cycle.IsActive());
		TestTrue(TEXT("Ciclo de 5-9 s"), Cycle.PeriodSeconds >= Def.SnorePeriodMin && Cycle.PeriodSeconds <= Def.SnorePeriodMax + 0.01f);
		TestTrue(TEXT("La ventana cabe en el ciclo"), Cycle.InhaleSeconds + Cycle.SnoreSeconds < Cycle.PeriodSeconds);
		TestFalse(TEXT("Inspirando aún no tapa"), Cycle.IsInMaskWindow(100.0 + Cycle.InhaleSeconds * 0.5));
		TestTrue(TEXT("Roncando tapa"), Cycle.IsInMaskWindow(100.0 + Cycle.InhaleSeconds + 0.3));
		TestTrue(TEXT("Roncando suena más que inspirando"), Cycle.GetLoudness(100.0 + Cycle.InhaleSeconds + 0.5) > Cycle.GetLoudness(100.0 + 0.2));
		TestEqual(TEXT("Dentro de la ventana: 0 s al siguiente"), Cycle.GetSecondsToNextWindow(100.0 + Cycle.InhaleSeconds + 0.3), 0.f);
	}
	const FNDSnoreCycle Awake = MakeSnoreCycle(Def, ENDMonsterState::HalfAwake, 0.0, 0, Rng);
	TestFalse(TEXT("Medio despierto no ronca"), Awake.IsActive());

	// El ronquido tapa pasos, pero no un golpe fuerte (GDD 9.4: nunca silencio total).
	const FNDSnoreCycle Cycle = MakeSnoreCycle(Def, ENDMonsterState::DeepSleep, 0.0, 0, Rng);
	const double InWindow = Cycle.InhaleSeconds + 0.3;
	TestTrue(TEXT("Paso tapado"), GetSnoreMask(Def, ENDMonsterState::DeepSleep, Cycle, InWindow, 10.f) < 0.5f);
	TestTrue(TEXT("Vidrio roto casi sin tapar"), GetSnoreMask(Def, ENDMonsterState::DeepSleep, Cycle, InWindow, 58.f) >= 0.8f);
	TestEqual(TEXT("Fuera de la ventana no tapa"), GetSnoreMask(Def, ENDMonsterState::DeepSleep, Cycle, 0.1, 10.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDMonsterHearingTest, "NDE.Monster.Hearing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDMonsterHearingTest::RunTest(const FString& Parameters)
{
	const UNDMonsterDefinition& Def = *GetDefault<UNDMonsterDefinition>();
	using namespace NDMonsterBrain;
	TestEqual(TEXT("Pegado: todo"), GetDistanceAttenuation(Def, 0.f), 1.f);
	TestTrue(TEXT("A la distancia media: la mitad"), FMath::IsNearlyEqual(GetDistanceAttenuation(Def, Def.HearingHalfDistance), 0.5f, 0.01f));
	TestEqual(TEXT("Fuera de alcance: nada"), GetDistanceAttenuation(Def, Def.HearingMaxDistance + 1.f), 0.f);
	TestTrue(TEXT("Más lejos, menos"), GetDistanceAttenuation(Def, 1000.f) < GetDistanceAttenuation(Def, 500.f));
	TestTrue(TEXT("Una puerta frena"), GetPerceivedLoudness(Def, 20.f, 500.f, 0.6f, 1.f) < GetPerceivedLoudness(Def, 20.f, 500.f, 1.f, 1.f));
	TestTrue(TEXT("Medio despierto oye más que dormido"), GetSuspicionGain(Def, ENDMonsterState::HalfAwake, 10.f) > GetSuspicionGain(Def, ENDMonsterState::DeepSleep, 10.f));

	// Equilibrio con los valores de M01 (GDD 9.2: agachado 4, andar 10, correr 22) a 2,5 m de la cama.
	const float Crouch = NDMonsterTests::SecondsToWake(Def, 4.f, 0.5f, 250.f, 120.f, true);
	const float Walk = NDMonsterTests::SecondsToWake(Def, 10.f, 0.4f, 250.f, 120.f, true);
	const float WalkNoSnore = NDMonsterTests::SecondsToWake(Def, 10.f, 0.4f, 250.f, 120.f, false);
	const float Run = NDMonsterTests::SecondsToWake(Def, 22.f, 0.33f, 250.f, 120.f, true);
	AddInfo(FString::Printf(TEXT("Segundos hasta despertar a 2,5 m: agachado %.1f, andando %.1f (sin ronquido %.1f), corriendo %.1f"), Crouch, Walk, WalkNoSnore, Run));
	TestEqual(TEXT("Agachado cerca de la cama no le despierta en 2 minutos"), Crouch, -1.f);
	TestTrue(TEXT("Corriendo al lado le despierta en menos de 10 s"), Run >= 0.f && Run < 10.f);
	TestTrue(TEXT("Andando tarda más que corriendo"), Walk < 0.f || Walk > Run);
	TestTrue(TEXT("El ronquido ayuda (o no le despierta)"), Walk < 0.f || WalkNoSnore < 0.f || Walk >= WalkNoSnore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNDMonsterSightTest, "NDE.Monster.Sight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNDMonsterSightTest::RunTest(const FString& Parameters)
{
	const UNDMonsterDefinition& Def = *GetDefault<UNDMonsterDefinition>();
	using namespace NDMonsterBrain;
	TestEqual(TEXT("Escondido: no se llena"), GetDetectionRate(Def, 0.f, 200.f, Def.SightRange), 0.f);
	TestEqual(TEXT("Fuera de alcance: no se llena"), GetDetectionRate(Def, 1.f, Def.SightRange + 10.f, Def.SightRange), 0.f);
	TestTrue(TEXT("Agachado se llena más despacio"), GetDetectionRate(Def, Def.CrouchedVisibility, 500.f, Def.SightRange) < GetDetectionRate(Def, 1.f, 500.f, Def.SightRange));
	TestTrue(TEXT("Lejos se llena más despacio"), GetDetectionRate(Def, 1.f, 1400.f, Def.SightRange) < GetDetectionRate(Def, 1.f, 300.f, Def.SightRange));
	// De pie y de cerca le ve en menos de un segundo; es el aviso justo antes de perseguir.
	const float Seconds = 1.f / GetDetectionRate(Def, 1.f, 300.f, Def.SightRange);
	TestTrue(TEXT("De cerca te ve en menos de 1 s"), Seconds < 1.f && Seconds > 0.3f);
	return true;
}

#endif
