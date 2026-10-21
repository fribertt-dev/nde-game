// No Despiertes Eso

#include "Networking/NDExpeditionGameState.h"

#include "Net/UnrealNetwork.h"

void ANDExpeditionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDExpeditionGameState, SessionPhase);
	DOREPLIFETIME(ANDExpeditionGameState, PhaseSequence);
	DOREPLIFETIME(ANDExpeditionGameState, PhaseStartServerTime);
	DOREPLIFETIME(ANDExpeditionGameState, RunGuid);
}

void ANDExpeditionGameState::SetSessionPhase(ENDExpeditionSessionPhase NewPhase)
{
	if (!HasAuthority() || SessionPhase == NewPhase)
	{
		return;
	}
	SessionPhase = NewPhase;
	PhaseStartServerTime = GetServerWorldTimeSeconds();
	++PhaseSequence;
	OnRep_PhaseSequence();
}

void ANDExpeditionGameState::SetRunGuid(const FGuid& InRunGuid)
{
	if (HasAuthority())
	{
		RunGuid = InRunGuid;
	}
}

void ANDExpeditionGameState::OnRep_PhaseSequence()
{
	// Las propiedades de un mismo actor llegan en el mismo paquete, así que SessionPhase ya está al día.
	if (PhaseSequence == LastNotifiedSequence)
	{
		return;
	}
	LastNotifiedSequence = PhaseSequence;
	OnSessionPhaseChanged.Broadcast(SessionPhase);
}
