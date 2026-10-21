// No Despiertes Eso

#include "Networking/NDLobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "Players/NDPlayerState.h"

void ANDLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDLobbyGameState, Phase);
	DOREPLIFETIME(ANDLobbyGameState, PhaseSequence);
	DOREPLIFETIME(ANDLobbyGameState, TravelServerTime);
}

int32 ANDLobbyGameState::GetNumReadyPlayers() const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const ANDPlayerState* NDPS = Cast<ANDPlayerState>(PS);
		Count += (NDPS && NDPS->IsReady()) ? 1 : 0;
	}
	return Count;
}

void ANDLobbyGameState::SetPhase(ENDLobbyPhase NewPhase, double InTravelServerTime)
{
	if (!HasAuthority() || (Phase == NewPhase && TravelServerTime == InTravelServerTime))
	{
		return;
	}
	Phase = NewPhase;
	TravelServerTime = InTravelServerTime;
	++PhaseSequence;
	OnRep_Phase();
}

void ANDLobbyGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}
