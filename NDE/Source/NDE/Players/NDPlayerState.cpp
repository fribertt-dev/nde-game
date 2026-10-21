// No Despiertes Eso

#include "Players/NDPlayerState.h"

#include "Net/UnrealNetwork.h"

void ANDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDPlayerState, bIsReady);
}

void ANDPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	// "Listo" no sobrevive al viaje: en cada mapa hay que volver a confirmarlo.
	if (ANDPlayerState* NewState = Cast<ANDPlayerState>(PlayerState))
	{
		NewState->bIsReady = false;
	}
}

void ANDPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority() || bIsReady == bNewReady)
	{
		return;
	}
	bIsReady = bNewReady;
	OnRep_IsReady();
}

void ANDPlayerState::OnRep_IsReady()
{
	OnReadyChanged.Broadcast(this, bIsReady);
}
