#include "Expedition/NDNightClockComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UNDNightClockComponent::UNDNightClockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UNDNightClockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNDNightClockComponent, NightStartServerTime);
	DOREPLIFETIME(UNDNightClockComponent, StoppedElapsedSeconds);
	DOREPLIFETIME(UNDNightClockComponent, NightDurationSeconds);
	DOREPLIFETIME(UNDNightClockComponent, DawnGraceSeconds);
	DOREPLIFETIME(UNDNightClockComponent, bDawnEnabled);
	DOREPLIFETIME(UNDNightClockComponent, bRunning);
	DOREPLIFETIME(UNDNightClockComponent, Phase);
}

void UNDNightClockComponent::StartNight(float InNightDurationSeconds, float InDawnGraceSeconds, bool bInDawnEnabled)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	NightDurationSeconds = FMath::Max(1.f, InNightDurationSeconds);
	DawnGraceSeconds = FMath::Max(0.f, InDawnGraceSeconds);
	bDawnEnabled = bInDawnEnabled;
	NightStartServerTime = GetServerNow();
	StoppedElapsedSeconds = -1.0;
	bRunning = true;
	FiredWarnings.Reset();
	bWarningsPrimed = false;
	SetPhase(ENDNightPhase::DeepNight);
}

void UNDNightClockComponent::StopClock()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bRunning)
	{
		return;
	}
	StoppedElapsedSeconds = GetElapsedSeconds();
	bRunning = false;
}

void UNDNightClockComponent::DebugSkipSeconds(float Seconds)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bRunning && Seconds > 0.f)
	{
		NightStartServerTime -= Seconds;
	}
}

double UNDNightClockComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

double UNDNightClockComponent::GetElapsedSeconds() const
{
	if (StoppedElapsedSeconds >= 0.0)
	{
		return StoppedElapsedSeconds;
	}
	if (Phase == ENDNightPhase::NotStarted)
	{
		return 0.0;
	}
	return FMath::Max(0.0, GetServerNow() - NightStartServerTime);
}

float UNDNightClockComponent::GetNightAlpha() const
{
	return static_cast<float>(FMath::Clamp(GetElapsedSeconds() / FMath::Max(1.f, NightDurationSeconds), 0.0, 1.0));
}

float UNDNightClockComponent::GetSecondsUntilDawn() const
{
	if (Phase == ENDNightPhase::NotStarted)
	{
		return NightDurationSeconds;
	}
	return static_cast<float>(FMath::Max(0.0, NightDurationSeconds - GetElapsedSeconds()));
}

float UNDNightClockComponent::GetGraceSecondsRemaining() const
{
	if (!IsDawnTriggered())
	{
		return DawnGraceSeconds;
	}
	const double IntoGrace = GetElapsedSeconds() - NightDurationSeconds;
	return static_cast<float>(FMath::Clamp(DawnGraceSeconds - IntoGrace, 0.0, static_cast<double>(DawnGraceSeconds)));
}

ENDNightPhase UNDNightClockComponent::ComputePhase(double ElapsedSeconds, float NightDuration, float Grace, float MadrugadaStartAlpha, float PreDawnStartAlpha, bool bDawnEnabled)
{
	const double Duration = FMath::Max(1.0, static_cast<double>(NightDuration));
	if (ElapsedSeconds >= Duration)
	{
		if (!bDawnEnabled)
		{
			return ENDNightPhase::PreDawn;
		}
		return ElapsedSeconds >= Duration + Grace ? ENDNightPhase::DayEnded : ENDNightPhase::DawnGrace;
	}

	// Tolerancia: los umbrales son float y 0.8 en double queda por debajo de 0.8f.
	const double Alpha = ElapsedSeconds / Duration + UE_KINDA_SMALL_NUMBER;
	if (Alpha >= PreDawnStartAlpha)
	{
		return ENDNightPhase::PreDawn;
	}
	if (Alpha >= MadrugadaStartAlpha)
	{
		return ENDNightPhase::Madrugada;
	}
	return ENDNightPhase::DeepNight;
}

void UNDNightClockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Phase == ENDNightPhase::NotStarted)
	{
		return;
	}

	// Solo el servidor decide la fase; los clientes la reciben por replicación.
	if (bRunning && GetOwner()->HasAuthority())
	{
		const ENDNightPhase NewPhase = ComputePhase(GetElapsedSeconds(), NightDurationSeconds, DawnGraceSeconds, MadrugadaStartAlpha, PreDawnStartAlpha, bDawnEnabled);
		if (NewPhase != Phase)
		{
			SetPhase(NewPhase);
		}
	}

	UpdateWarnings();
}

void UNDNightClockComponent::SetPhase(ENDNightPhase NewPhase)
{
	const ENDNightPhase OldPhase = Phase;
	Phase = NewPhase;
	OnNightPhaseChanged.Broadcast(Phase, OldPhase);
}

void UNDNightClockComponent::OnRep_Phase(ENDNightPhase OldPhase)
{
	OnNightPhaseChanged.Broadcast(Phase, OldPhase);
}

void UNDNightClockComponent::UpdateWarnings()
{
	if (!bRunning || !bDawnEnabled || IsDawnTriggered())
	{
		return;
	}

	const float Remaining = GetSecondsUntilDawn();

	// Quien entra tarde no recibe avisos viejos de golpe.
	if (!bWarningsPrimed)
	{
		bWarningsPrimed = true;
		for (const int32 Threshold : PreDawnWarningSeconds)
		{
			if (Remaining < Threshold - 1.f)
			{
				FiredWarnings.Add(Threshold);
			}
		}
	}

	for (const int32 Threshold : PreDawnWarningSeconds)
	{
		if (Remaining <= Threshold && !FiredWarnings.Contains(Threshold))
		{
			FiredWarnings.Add(Threshold);
			OnPreDawnWarning.Broadcast(Threshold);
		}
	}
}
