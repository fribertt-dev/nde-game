// No Despiertes Eso

#include "Monsters/NDMonsterBrain.h"

#include "Monsters/NDMonsterDefinition.h"

float FNDSnoreCycle::GetLoudness(double ServerTime) const
{
	if (!IsActive())
	{
		return 0.f;
	}
	const float T = GetElapsed(ServerTime);
	if (T < 0.f)
	{
		return 0.f;
	}
	if (T < InhaleSeconds)
	{
		// Inspiración: sube suave (el aviso).
		return Amplitude * 0.25f * (T / InhaleSeconds);
	}
	const float SnoreT = T - InhaleSeconds;
	if (SnoreT < SnoreSeconds)
	{
		// Ronquido: entra rápido y se apaga al final.
		const float Attack = FMath::Clamp(SnoreT / 0.25f, 0.f, 1.f);
		const float Release = FMath::Clamp((SnoreSeconds - SnoreT) / 0.4f, 0.f, 1.f);
		return Amplitude * FMath::Min(Attack, Release);
	}
	// Pausa: casi silencio (la espiración).
	const float PauseT = SnoreT - SnoreSeconds;
	return Amplitude * 0.1f * FMath::Clamp(1.f - PauseT, 0.f, 1.f);
}

float FNDSnoreCycle::GetSecondsToNextWindow(double ServerTime) const
{
	if (!IsActive())
	{
		return -1.f;
	}
	const float T = GetElapsed(ServerTime);
	if (T < InhaleSeconds)
	{
		return InhaleSeconds - T;
	}
	if (T < InhaleSeconds + SnoreSeconds)
	{
		return 0.f;
	}
	// El próximo ciclo (aún sin generar) empieza al acabar este, con una inspiración parecida.
	return FMath::Max(0.f, PeriodSeconds - T) + InhaleSeconds;
}

namespace NDMonsterBrain
{
	bool IsInBed(ENDMonsterState State)
	{
		return State == ENDMonsterState::DeepSleep || State == ENDMonsterState::LightSleep || State == ENDMonsterState::Stirring || State == ENDMonsterState::HalfAwake;
	}

	bool IsSnoringState(ENDMonsterState State)
	{
		return State == ENDMonsterState::DeepSleep || State == ENDMonsterState::LightSleep;
	}

	float GetDistanceAttenuation(const UNDMonsterDefinition& Def, float DistanceCm)
	{
		if (DistanceCm >= Def.HearingMaxDistance)
		{
			return 0.f;
		}
		// Curva suave (no lineal, GDD 9.2): 1 / (1 + (d / d50)^2), y cae a 0 en el último 20 % del alcance.
		const float Ratio = DistanceCm / FMath::Max(1.f, Def.HearingHalfDistance);
		const float Curve = 1.f / (1.f + Ratio * Ratio);
		const float FadeStart = Def.HearingMaxDistance * 0.8f;
		const float Fade = DistanceCm > FadeStart ? 1.f - (DistanceCm - FadeStart) / (Def.HearingMaxDistance - FadeStart) : 1.f;
		return Curve * FMath::Clamp(Fade, 0.f, 1.f);
	}

	float GetHearingSensitivity(const UNDMonsterDefinition& Def, ENDMonsterState State)
	{
		switch (State)
		{
		case ENDMonsterState::DeepSleep: return Def.HearingDeepSleep;
		case ENDMonsterState::LightSleep: return Def.HearingLightSleep;
		case ENDMonsterState::Stirring: return Def.HearingStirring;
		case ENDMonsterState::NightWalk: return Def.HearingLightSleep;
		default: return Def.HearingAwake;
		}
	}

	float GetSnoreMask(const UNDMonsterDefinition& Def, ENDMonsterState State, const FNDSnoreCycle& Cycle, double ServerTime, float BaseLoudness)
	{
		if (!IsSnoringState(State) || !Cycle.IsInMaskWindow(ServerTime))
		{
			return 1.f;
		}
		const float Mask = State == ENDMonsterState::DeepSleep ? Def.SnoreMaskDeepSleep : Def.SnoreMaskLightSleep;
		// Nunca hay silencio total: lo muy fuerte casi no se tapa (GDD 9.4).
		return BaseLoudness >= Def.SnoreLoudNoise ? FMath::Max(Mask, 0.8f) : Mask;
	}

	float GetPerceivedLoudness(const UNDMonsterDefinition& Def, float BaseLoudness, float DistanceCm, float Transmission, float SnoreMask)
	{
		return FMath::Max(0.f, BaseLoudness * GetDistanceAttenuation(Def, DistanceCm) * FMath::Clamp(Transmission, 0.f, 1.f) * SnoreMask);
	}

	float GetSuspicionGain(const UNDMonsterDefinition& Def, ENDMonsterState State, float Perceived)
	{
		if (Perceived <= 0.f)
		{
			return 0.f;
		}
		return FMath::Pow(Perceived / 10.f, Def.HearingExponent) * Def.HearingGain * GetHearingSensitivity(Def, State);
	}

	float GetDecayPerSecond(const UNDMonsterDefinition& Def, ENDMonsterState State)
	{
		switch (State)
		{
		case ENDMonsterState::DeepSleep: return Def.DecayDeepSleep;
		case ENDMonsterState::LightSleep: return Def.DecayLightSleep;
		case ENDMonsterState::Stirring:
		case ENDMonsterState::HalfAwake: return Def.DecayStirring;
		default: return Def.DecayAwake;
		}
	}

	ENDMonsterState GetBedState(const UNDMonsterDefinition& Def, ENDMonsterState Current, float Suspicion)
	{
		if (Suspicion >= Def.WakeThreshold)
		{
			return ENDMonsterState::WakingUp;
		}
		// Subir: el umbral exacto. Bajar: el umbral menos la histéresis.
		auto TierOf = [](ENDMonsterState S)
		{
			switch (S)
			{
			case ENDMonsterState::LightSleep: return 1;
			case ENDMonsterState::Stirring: return 2;
			case ENDMonsterState::HalfAwake: return 3;
			default: return 0;
			}
		};
		const float Thresholds[] = { 0.f, Def.LightSleepThreshold, Def.StirringThreshold, Def.HalfAwakeThreshold };
		int32 Tier = TierOf(Current);
		while (Tier < 3 && Suspicion >= Thresholds[Tier + 1])
		{
			++Tier;
		}
		while (Tier > 0 && Suspicion < Thresholds[Tier] - Def.Hysteresis)
		{
			--Tier;
		}
		static const ENDMonsterState States[] = { ENDMonsterState::DeepSleep, ENDMonsterState::LightSleep, ENDMonsterState::Stirring, ENDMonsterState::HalfAwake };
		return States[Tier];
	}

	FNDSnoreCycle MakeSnoreCycle(const UNDMonsterDefinition& Def, ENDMonsterState State, double StartServerTime, int32 CycleIndex, FRandomStream& Rng)
	{
		FNDSnoreCycle Cycle;
		Cycle.StartServerTime = StartServerTime;
		Cycle.CycleIndex = CycleIndex;
		Cycle.InhaleSeconds = Def.SnoreInhaleSeconds;
		Cycle.PeriodSeconds = Rng.FRandRange(Def.SnorePeriodMin, FMath::Max(Def.SnorePeriodMin, Def.SnorePeriodMax));
		if (State == ENDMonsterState::DeepSleep)
		{
			Cycle.Amplitude = 1.f;
			Cycle.SnoreSeconds = Rng.FRandRange(Def.SnoreWindowMin, FMath::Max(Def.SnoreWindowMin, Def.SnoreWindowMax));
		}
		else if (State == ENDMonsterState::LightSleep)
		{
			// Ronquido corto y flojo (GDD 9.3).
			Cycle.Amplitude = 0.5f;
			Cycle.SnoreSeconds = Def.SnoreWindowMin * 0.6f;
		}
		else
		{
			// Despierto o revolviéndose: respira sin roncar; el ciclo solo marca el tiempo.
			Cycle.Amplitude = 0.f;
			Cycle.SnoreSeconds = 0.f;
			Cycle.PeriodSeconds = 2.f;
		}
		// La ventana tiene que caber en el ciclo, con al menos medio segundo de pausa.
		Cycle.PeriodSeconds = FMath::Max(Cycle.PeriodSeconds, Cycle.InhaleSeconds + Cycle.SnoreSeconds + 0.5f);
		return Cycle;
	}

	float GetDetectionRate(const UNDMonsterDefinition& Def, float Visibility, float DistanceCm, float Range)
	{
		if (Visibility <= 0.f || Range <= 0.f || DistanceCm > Range)
		{
			return 0.f;
		}
		// De cerca (3 m o menos) a tope; en el borde del alcance, al 30 %.
		const float Near = 300.f;
		const float DistanceFactor = DistanceCm <= Near ? 1.f : FMath::Lerp(1.f, 0.3f, (DistanceCm - Near) / FMath::Max(1.f, Range - Near));
		return Def.DetectionRate * FMath::Clamp(Visibility, 0.f, 1.5f) * DistanceFactor;
	}

	FString GetStateLabel(ENDMonsterState State)
	{
		switch (State)
		{
		case ENDMonsterState::DeepSleep: return TEXT("Sueño profundo");
		case ENDMonsterState::LightSleep: return TEXT("Sueño ligero");
		case ENDMonsterState::Stirring: return TEXT("Se revuelve");
		case ENDMonsterState::HalfAwake: return TEXT("Medio despierto");
		case ENDMonsterState::WakingUp: return TEXT("Se levanta");
		case ENDMonsterState::Investigate: return TEXT("Investiga");
		case ENDMonsterState::Search: return TEXT("Busca");
		case ENDMonsterState::Chase: return TEXT("Persigue");
		case ENDMonsterState::ReturnToSleep: return TEXT("Vuelve a la cama");
		case ENDMonsterState::NightWalk: return TEXT("Ronda nocturna");
		case ENDMonsterState::DawnHunt: return TEXT("Amanecer: despierto");
		default: return TEXT("?");
		}
	}
}
