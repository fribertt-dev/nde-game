#include "Expedition/NDExpeditionPlayerComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Loot/NDExtractionVan.h"

UNDExpeditionPlayerComponent::UNDExpeditionPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

APlayerState* UNDExpeditionPlayerComponent::GetOwnerPlayerState() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetPlayerState() : nullptr;
}

bool UNDExpeditionPlayerComponent::IsInsideVan() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	for (TActorIterator<ANDExtractionVan> It(GetWorld()); It; ++It)
	{
		if (It->IsPawnInside(Pawn))
		{
			return true;
		}
	}
	return false;
}

void UNDExpeditionPlayerComponent::RequestVanInteraction()
{
	ServerVanInteraction();
}

void UNDExpeditionPlayerComponent::RequestEscapeVote(bool bYes)
{
	ServerEscapeVote(bYes);
}

void UNDExpeditionPlayerComponent::ServerVanInteraction_Implementation()
{
	if (UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this))
	{
		Expedition->HandleVanInteraction(GetOwnerPlayerState());
	}
}

void UNDExpeditionPlayerComponent::ServerEscapeVote_Implementation(bool bYes)
{
	if (UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this))
	{
		Expedition->CastEscapeVote(GetOwnerPlayerState(), bYes);
	}
}
