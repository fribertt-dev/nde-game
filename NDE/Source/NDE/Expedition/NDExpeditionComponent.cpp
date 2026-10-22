#include "Expedition/NDExpeditionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Contracts/NDContractDefinition.h"
#include "Contracts/NDSettlementLibrary.h"
#include "Expedition/NDNightClockComponent.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDExtractionVan.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"

UNDExpeditionComponent::UNDExpeditionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UNDExpeditionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNDExpeditionComponent, Contract);
	DOREPLIFETIME(UNDExpeditionComponent, Phase);
	DOREPLIFETIME(UNDExpeditionComponent, bStarted);
	DOREPLIFETIME(UNDExpeditionComponent, RunGuid);
	DOREPLIFETIME(UNDExpeditionComponent, Participants);
	DOREPLIFETIME(UNDExpeditionComponent, Deposits);
	DOREPLIFETIME(UNDExpeditionComponent, bObjectiveMet);
	DOREPLIFETIME(UNDExpeditionComponent, InsertionEndServerTime);
	DOREPLIFETIME(UNDExpeditionComponent, bVoteActive);
	DOREPLIFETIME(UNDExpeditionComponent, VoteEndServerTime);
	DOREPLIFETIME(UNDExpeditionComponent, VotersYes);
	DOREPLIFETIME(UNDExpeditionComponent, VotersNo);
	DOREPLIFETIME(UNDExpeditionComponent, bEvacuationActive);
	DOREPLIFETIME(UNDExpeditionComponent, EvacuationEndServerTime);
	DOREPLIFETIME(UNDExpeditionComponent, RunSummary);
}

void UNDExpeditionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UNDNightClockComponent* Clock = GetNightClock())
	{
		Clock->OnNightPhaseChanged.AddDynamic(this, &UNDExpeditionComponent::HandleNightPhaseChanged);
	}

	if (GetOwner()->HasAuthority() && bAutoStart)
	{
		GetWorld()->GetTimerManager().SetTimer(AutoStartTimer, this, &UNDExpeditionComponent::StartExpedition, FMath::Max(0.01f, AutoStartDelaySeconds), false);
	}
}

// ---------------------------------------------------------------------------
// Utilidades

UNDNightClockComponent* UNDExpeditionComponent::GetNightClock() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UNDNightClockComponent>() : nullptr;
}

ANDExtractionVan* UNDExpeditionComponent::GetVan() const
{
	if (Van)
	{
		return Van;
	}
	TActorIterator<ANDExtractionVan> It(GetWorld());
	return It ? *It : nullptr;
}

void UNDExpeditionComponent::RegisterVan(ANDExtractionVan* InVan)
{
	Van = InVan;
}

double UNDExpeditionComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World ? World->GetTimeSeconds() : 0.0;
}

FNDParticipant* UNDExpeditionComponent::FindParticipant(const APlayerState* PlayerState)
{
	return PlayerState ? Participants.FindByPredicate([PlayerState](const FNDParticipant& P) { return P.PlayerState == PlayerState; }) : nullptr;
}

ENDParticipantStatus UNDExpeditionComponent::GetParticipantStatus(const APlayerState* PlayerState) const
{
	const FNDParticipant* Found = PlayerState ? Participants.FindByPredicate([PlayerState](const FNDParticipant& P) { return P.PlayerState == PlayerState; }) : nullptr;
	return Found ? Found->Status : ENDParticipantStatus::Disconnected;
}

bool UNDExpeditionComponent::IsActiveParticipant(const APlayerState* PlayerState) const
{
	return GetParticipantStatus(PlayerState) == ENDParticipantStatus::Active;
}

bool UNDExpeditionComponent::IsInVan(const APlayerState* PlayerState) const
{
	const ANDExtractionVan* TheVan = GetVan();
	return TheVan && PlayerState && TheVan->IsPawnInside(PlayerState->GetPawn());
}

int32 UNDExpeditionComponent::CountParticipantsWithStatus(ENDParticipantStatus Status) const
{
	int32 Count = 0;
	for (const FNDParticipant& Participant : Participants)
	{
		Count += Participant.Status == Status ? 1 : 0;
	}
	return Count;
}

bool UNDExpeditionComponent::CanPlayerAct(const APawn* Pawn) const
{
	if (Phase == ENDExpeditionPhase::Settlement)
	{
		return false;
	}
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	const FNDParticipant* Found = PlayerState ? Participants.FindByPredicate([PlayerState](const FNDParticipant& P) { return P.PlayerState == PlayerState; }) : nullptr;
	return !Found || Found->Status == ENDParticipantStatus::Active;
}

bool UNDExpeditionComponent::CanAcceptDeposits() const
{
	return bStarted && Phase != ENDExpeditionPhase::Settlement;
}

int32 UNDExpeditionComponent::GetLootPoints() const
{
	return UNDSettlementLibrary::ComputeLootPoints(Deposits);
}

int32 UNDExpeditionComponent::GetQuota() const
{
	return Contract ? Contract->RequiredQuotaValue : 0;
}

float UNDExpeditionComponent::GetInsertionSecondsRemaining() const
{
	return (bStarted && Phase == ENDExpeditionPhase::Insertion) ? static_cast<float>(FMath::Max(0.0, InsertionEndServerTime - GetServerNow())) : 0.f;
}

float UNDExpeditionComponent::GetEscapeVoteSecondsRemaining() const
{
	return bVoteActive ? static_cast<float>(FMath::Max(0.0, VoteEndServerTime - GetServerNow())) : 0.f;
}

float UNDExpeditionComponent::GetEvacuationSecondsRemaining() const
{
	return bEvacuationActive ? static_cast<float>(FMath::Max(0.0, EvacuationEndServerTime - GetServerNow())) : 0.f;
}

// ---------------------------------------------------------------------------
// Ciclo de la expedición

void UNDExpeditionComponent::SetContract(UNDContractDefinition* InContract)
{
	if (GetOwner()->HasAuthority() && !bStarted)
	{
		Contract = InContract;
	}
}

void UNDExpeditionComponent::SetRunGuid(const FGuid& InRunGuid)
{
	if (GetOwner()->HasAuthority() && !bSettled)
	{
		RunGuid = InRunGuid;
	}
}

void UNDExpeditionComponent::StartExpedition()
{
	if (!GetOwner()->HasAuthority() || bStarted)
	{
		return;
	}
	if (!Contract)
	{
		UE_LOG(LogTemp, Warning, TEXT("NDExpedition: no hay contrato asignado; la expedici\u00F3n no arranca."));
		return;
	}

	if (!RunGuid.IsValid())
	{
		RunGuid = FGuid::NewGuid();
	}
	bStarted = true;
	InsertionEndServerTime = GetServerNow() + InsertionSeconds;
	RefreshParticipants();
	SetPhase(ENDExpeditionPhase::Insertion);
	BroadcastStateChanged();
}

void UNDExpeditionComponent::NotifyPlayerEnteredHouse()
{
	if (GetOwner()->HasAuthority() && bStarted && Phase == ENDExpeditionPhase::Insertion)
	{
		BeginInfiltration();
	}
}

void UNDExpeditionComponent::BeginInfiltration()
{
	InfiltrationStartServerTime = GetServerNow();
	SetPhase(ENDExpeditionPhase::Infiltration);

	if (UNDNightClockComponent* Clock = GetNightClock())
	{
		Clock->StartNight(Contract->NightDurationSeconds, Contract->DawnGraceSeconds, Contract->bDawnEnabled);
	}
	EvaluateObjective();
}

void UNDExpeditionComponent::HandleNightPhaseChanged(ENDNightPhase NewPhase, ENDNightPhase OldPhase)
{
	if (!GetOwner()->HasAuthority() || !bStarted || bSettled)
	{
		return;
	}
	if (NewPhase == ENDNightPhase::DawnGrace)
	{
		EnterDawnGrace();
	}
	else if (NewPhase == ENDNightPhase::DayEnded)
	{
		FinishExpedition(ENDExpeditionEndReason::DayEnded);
	}
}

void UNDExpeditionComponent::EnterDawnGrace()
{
	// Tras el amanecer no hay votaciones nuevas que alarguen la noche (GDD 7.2).
	EndEscapeVote();
	SetPhase(ENDExpeditionPhase::DawnEscapeGrace);
	BroadcastStateChanged();
}

void UNDExpeditionComponent::SetPhase(ENDExpeditionPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	const ENDExpeditionPhase OldPhase = Phase;
	Phase = NewPhase;
	OnPhaseChanged.Broadcast(Phase, OldPhase);
}

void UNDExpeditionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority() || !bStarted || bSettled)
	{
		return;
	}

	const double Now = GetServerNow();

	if (Phase == ENDExpeditionPhase::Insertion && Now >= InsertionEndServerTime)
	{
		BeginInfiltration();
	}

	if (Phase == ENDExpeditionPhase::Insertion || Now - InfiltrationStartServerTime <= LateJoinWindowSeconds)
	{
		RefreshParticipants();
	}
	UpdateDisconnections();

	if (Phase != ENDExpeditionPhase::Insertion && Participants.Num() > 0 &&
		CountParticipantsWithStatus(ENDParticipantStatus::Active) == 0 &&
		CountParticipantsWithStatus(ENDParticipantStatus::Extracted) == 0)
	{
		FinishExpedition(ENDExpeditionEndReason::AllCaptured);
		return;
	}

	if (bVoteActive && Now >= VoteEndServerTime)
	{
		ResolveEscapeVote(true);
	}

	if (bEvacuationActive && Now >= EvacuationEndServerTime)
	{
		CompleteEvacuation();
	}
}

// ---------------------------------------------------------------------------
// Participantes

void UNDExpeditionComponent::RefreshParticipants()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState && !PlayerState->IsSpectator() && !PlayerState->IsInactive())
		{
			RegisterParticipant(PlayerState);
		}
	}
}

void UNDExpeditionComponent::RegisterParticipant(APlayerState* PlayerState)
{
	if (!GetOwner()->HasAuthority() || !PlayerState || bSettled || FindParticipant(PlayerState))
	{
		return;
	}
	FNDParticipant& Participant = Participants.AddDefaulted_GetRef();
	Participant.PlayerState = PlayerState;
	Participant.PlayerName = PlayerState->GetPlayerName();
	Participant.Status = ENDParticipantStatus::Active;
	BroadcastStateChanged();
}

void UNDExpeditionComponent::UpdateDisconnections()
{
	bool bChanged = false;
	for (FNDParticipant& Participant : Participants)
	{
		const bool bGone = !IsValid(Participant.PlayerState) || Participant.PlayerState->IsInactive();
		if (bGone && Participant.Status != ENDParticipantStatus::Disconnected && Participant.Status != ENDParticipantStatus::Extracted)
		{
			Participant.Status = ENDParticipantStatus::Disconnected;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		ResolveEscapeVote(false);
		BroadcastStateChanged();
	}
}

void UNDExpeditionComponent::SetParticipantCaptured(APlayerState* PlayerState, bool bCaptured)
{
	if (!GetOwner()->HasAuthority() || bSettled)
	{
		return;
	}
	FNDParticipant* Participant = FindParticipant(PlayerState);
	if (!Participant)
	{
		return;
	}

	if (bCaptured && Participant->Status == ENDParticipantStatus::Active)
	{
		Participant->Status = ENDParticipantStatus::Captured;
		if (Phase == ENDExpeditionPhase::DawnEscapeGrace)
		{
			++DawnCaptures;
		}
		// Al ser capturado suelta todo el botín (GDD 8.4).
		if (APawn* Pawn = PlayerState->GetPawn())
		{
			if (UNDCarryComponent* Carry = Pawn->FindComponentByClass<UNDCarryComponent>())
			{
				Carry->DropEverything();
			}
		}
		VotersYes.Remove(PlayerState);
		VotersNo.Remove(PlayerState);
		ResolveEscapeVote(false);
	}
	else if (!bCaptured && Participant->Status == ENDParticipantStatus::Captured)
	{
		Participant->Status = ENDParticipantStatus::Active;
		Participant->bWasRescued = true;
	}
	BroadcastStateChanged();
}

// ---------------------------------------------------------------------------
// Botín y objetivo

bool UNDExpeditionComponent::RecordDeposit(ANDLootActor* Loot, APawn* Depositor)
{
	if (!GetOwner()->HasAuthority() || !CanAcceptDeposits() || !Loot)
	{
		return false;
	}
	const UNDLootDefinition* Definition = Loot->GetDefinition();
	const FGuid& Guid = Loot->GetLootGuid();
	if (!Definition || !Guid.IsValid() || DepositedGuids.Contains(Guid))
	{
		return false;
	}

	FNDDepositRecord& Record = Deposits.AddDefaulted_GetRef();
	Record.LootGuid = Guid;
	Record.LootId = Definition->LootId;
	Record.BaseValue = Definition->BaseValue;
	Record.Condition = Loot->GetCondition();
	Record.Fragility = Definition->Fragility;
	Record.ConditionFactor = Definition->GetConditionFactor(Record.Condition);
	Record.Depositor = Depositor ? Depositor->GetPlayerState() : nullptr;
	DepositedGuids.Add(Guid);

	EvaluateObjective();
	BroadcastStateChanged();
	return true;
}

void UNDExpeditionComponent::NotifyLootBroken(ANDLootActor* Loot)
{
	const UNDLootDefinition* Definition = Loot ? Loot->GetDefinition() : nullptr;
	if (GetOwner()->HasAuthority() && Definition && Definition->Fragility != ENDFragility::Sturdy)
	{
		++BrokenFragileCount;
	}
}

void UNDExpeditionComponent::EvaluateObjective()
{
	if (!Contract)
	{
		return;
	}

	bool bMet = GetLootPoints() >= Contract->RequiredQuotaValue;

	// Cada LootId requerido debe llegar sin romperse, tantas veces como aparezca.
	TMap<FName, int32> Needed;
	for (const FName& Id : Contract->RequiredLootIds)
	{
		Needed.FindOrAdd(Id)++;
	}
	for (const FNDDepositRecord& Record : Deposits)
	{
		if (int32* Count = Needed.Find(Record.LootId); Count && Record.Condition != ENDLootCondition::Broken)
		{
			--(*Count);
		}
	}
	for (const TPair<FName, int32>& Pair : Needed)
	{
		bMet &= Pair.Value <= 0;
	}

	bObjectiveMet = bMet;
	if (bObjectiveMet && Phase == ENDExpeditionPhase::Infiltration)
	{
		SetPhase(ENDExpeditionPhase::ExtractionReady);
	}
}

bool UNDExpeditionComponent::ComputeSecondaryMet() const
{
	if (!Contract)
	{
		return false;
	}

	switch (Contract->SecondaryObjective)
	{
	case ENDSecondaryObjective::NoFragileBroken:
		return BrokenFragileCount == 0;

	case ENDSecondaryObjective::RequiredLootIntact:
	{
		if (!bObjectiveMet)
		{
			return false;
		}
		for (const FNDDepositRecord& Record : Deposits)
		{
			if (Contract->RequiredLootIds.Contains(Record.LootId) && Record.Condition != ENDLootCondition::Intact)
			{
				return false;
			}
		}
		return true;
	}

	case ENDSecondaryObjective::AllExtractedBeforeDawn:
	{
		const UNDNightClockComponent* Clock = GetNightClock();
		return Participants.Num() > 0 &&
			CountParticipantsWithStatus(ENDParticipantStatus::Extracted) == Participants.Num() &&
			!(Clock && Clock->IsDawnTriggered());
	}

	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// Votación y evacuación (GDD 7.2 y 7.3)

void UNDExpeditionComponent::HandleVanInteraction(APlayerState* PlayerState)
{
	if (Phase == ENDExpeditionPhase::DawnEscapeGrace)
	{
		RequestDawnEvacuation(PlayerState);
	}
	else if (bVoteActive)
	{
		CastEscapeVote(PlayerState, true);
	}
	else
	{
		RequestEscapeVote(PlayerState);
	}
}

void UNDExpeditionComponent::RequestEscapeVote(APlayerState* PlayerState)
{
	const bool bVotingPhase = Phase == ENDExpeditionPhase::Infiltration || Phase == ENDExpeditionPhase::ExtractionReady;
	if (!GetOwner()->HasAuthority() || !bVotingPhase || bVoteActive || bEvacuationActive)
	{
		return;
	}
	if (!IsActiveParticipant(PlayerState) || !IsInVan(PlayerState))
	{
		return;
	}

	// Un único superviviente sale sin votación, con cuenta atrás.
	if (CountParticipantsWithStatus(ENDParticipantStatus::Active) <= 1)
	{
		StartEvacuation();
		return;
	}

	bVoteActive = true;
	VoteEndServerTime = GetServerNow() + EscapeVoteSeconds;
	VotersYes.Reset();
	VotersNo.Reset();
	VotersYes.Add(PlayerState);
	BroadcastStateChanged();
	ResolveEscapeVote(false);
}

void UNDExpeditionComponent::CastEscapeVote(APlayerState* PlayerState, bool bYes)
{
	if (!GetOwner()->HasAuthority() || !bVoteActive || !IsActiveParticipant(PlayerState))
	{
		return;
	}
	VotersYes.Remove(PlayerState);
	VotersNo.Remove(PlayerState);
	(bYes ? VotersYes : VotersNo).Add(PlayerState);
	BroadcastStateChanged();
	ResolveEscapeVote(false);
}

void UNDExpeditionComponent::ResolveEscapeVote(bool bTimedOut)
{
	if (!bVoteActive)
	{
		return;
	}

	// Solo cuentan votos de supervivientes activos.
	VotersYes.RemoveAll([this](const TObjectPtr<APlayerState>& PS) { return !IsActiveParticipant(PS); });
	VotersNo.RemoveAll([this](const TObjectPtr<APlayerState>& PS) { return !IsActiveParticipant(PS); });

	const int32 Survivors = CountParticipantsWithStatus(ENDParticipantStatus::Active);
	const int32 Yes = VotersYes.Num();
	const int32 No = VotersNo.Num();

	bool bPassed = false;
	bool bDecided = false;
	if (Survivors <= 0)
	{
		bDecided = true;
	}
	else if (bRequireUnanimity)
	{
		bPassed = Yes >= Survivors;
		bDecided = bPassed || No > 0 || bTimedOut;
	}
	else if (Yes * 2 > Survivors)
	{
		bPassed = bDecided = true;
	}
	else if (No * 2 >= Survivors)
	{
		bDecided = true;
	}
	else if (bTimedOut)
	{
		// Las abstenciones no bloquean: mayoría de emitidos si votó al menos la mitad.
		bPassed = (Yes + No) * 2 >= Survivors && Yes > No;
		bDecided = true;
	}

	if (bDecided)
	{
		EndEscapeVote();
		if (bPassed)
		{
			StartEvacuation();
		}
	}
}

void UNDExpeditionComponent::EndEscapeVote()
{
	if (bVoteActive)
	{
		bVoteActive = false;
		VotersYes.Reset();
		VotersNo.Reset();
		BroadcastStateChanged();
	}
}

void UNDExpeditionComponent::RequestDawnEvacuation(APlayerState* PlayerState)
{
	if (!GetOwner()->HasAuthority() || Phase != ENDExpeditionPhase::DawnEscapeGrace || bEvacuationActive)
	{
		return;
	}
	if (IsActiveParticipant(PlayerState) && IsInVan(PlayerState))
	{
		StartEvacuation();
	}
}

void UNDExpeditionComponent::StartEvacuation()
{
	if (bEvacuationActive || bSettled)
	{
		return;
	}
	bEvacuationActive = true;
	EvacuationEndServerTime = GetServerNow() + EvacuationCountdownSeconds;
	BroadcastStateChanged();
}

void UNDExpeditionComponent::CompleteEvacuation()
{
	bEvacuationActive = false;

	int32 Escaped = 0;
	for (FNDParticipant& Participant : Participants)
	{
		if (Participant.Status == ENDParticipantStatus::Active && IsInVan(Participant.PlayerState))
		{
			Participant.Status = ENDParticipantStatus::Extracted;
			++Escaped;
		}
	}

	if (Escaped == 0)
	{
		// Nadie llegó a la camioneta: se cancela y la noche sigue.
		BroadcastStateChanged();
		return;
	}

	FinishExpedition(ENDExpeditionEndReason::Evacuated);
}

// ---------------------------------------------------------------------------
// Cierre y liquidación (GDD 7.9 y 16.3)

void UNDExpeditionComponent::AbortExpedition()
{
	if (GetOwner()->HasAuthority() && bStarted)
	{
		FinishExpedition(ENDExpeditionEndReason::Aborted);
	}
}

void UNDExpeditionComponent::FinishExpedition(ENDExpeditionEndReason Reason)
{
	if (bSettled || !Contract)
	{
		return;
	}
	bSettled = true;
	bEvacuationActive = false;
	EndEscapeVote();

	UNDNightClockComponent* Clock = GetNightClock();
	const bool bDawnTriggered = Clock && Clock->IsDawnTriggered();
	const float RemainingNight = Clock ? Clock->GetSecondsUntilDawn() : 0.f;
	if (Clock)
	{
		Clock->StopClock();
	}

	for (FNDParticipant& Participant : Participants)
	{
		if (Participant.Status != ENDParticipantStatus::Active)
		{
			continue;
		}
		const bool bEscapesAtDayEnd = Reason == ENDExpeditionEndReason::DayEnded && bVanOccupantsEscapeAtDayEnd && IsInVan(Participant.PlayerState);
		Participant.Status = bEscapesAtDayEnd ? ENDParticipantStatus::Extracted : ENDParticipantStatus::LeftBehind;
	}

	EvaluateObjective();
	const bool bSecondaryMet = ComputeSecondaryMet();
	const bool bSecondaryForPerfect = Contract->SecondaryObjective == ENDSecondaryObjective::None || bSecondaryMet;
	const int32 NumExtracted = CountParticipantsWithStatus(ENDParticipantStatus::Extracted);
	const bool bTeamRescue = Participants.ContainsByPredicate([](const FNDParticipant& P) { return P.bWasRescued; });

	FNDRunSummary Summary;
	Summary.RunGuid = RunGuid;
	Summary.ContractId = Contract->ContractId;
	Summary.EndReason = Reason;
	Summary.LootPointsDeposited = GetLootPoints();
	Summary.LootSoldValue = UNDSettlementLibrary::ComputeLootSoldValue(Deposits, Contract->ContractValueMultiplier);
	Summary.Quota = Contract->RequiredQuotaValue;
	Summary.bObjectiveMet = bObjectiveMet;
	Summary.bSecondaryMet = bSecondaryMet;
	Summary.bDawnTriggered = bDawnTriggered;
	Summary.RemainingTimeAtExtraction = RemainingNight;
	Summary.DawnSurvivors = bDawnTriggered ? NumExtracted : 0;
	Summary.DawnCaptures = DawnCaptures;
	Summary.Outcome = Reason == ENDExpeditionEndReason::Aborted
		? ENDExpeditionOutcome::Failure
		: UNDSettlementLibrary::DetermineOutcome(bObjectiveMet, bSecondaryForPerfect, NumExtracted, Participants.Num());

	for (const FNDParticipant& Participant : Participants)
	{
		FNDPlayerSettlement& PlayerResult = Summary.Players.AddDefaulted_GetRef();
		PlayerResult.PlayerState = Participant.PlayerState;
		PlayerResult.PlayerName = Participant.PlayerName;
		PlayerResult.FinalStatus = Participant.Status;
		PlayerResult.Reward = UNDSettlementLibrary::ComputePlayerReward(
			Summary.Outcome, Participant.Status, bSecondaryMet, bTeamRescue,
			Summary.LootSoldValue, Summary.Quota, Contract->BasePayoutPerPlayer, Contract->ReputationReward, Economy);
	}

	RunSummary = Summary;
	SetPhase(ENDExpeditionPhase::Settlement);
	BroadcastStateChanged();

	bSettledBroadcastLocally = true;
	LogRunSummary();
	OnExpeditionSettled.Broadcast(RunSummary);
}

void UNDExpeditionComponent::LogRunSummary() const
{
	UE_LOG(LogTemp, Log, TEXT("NDExpedition [%s]: resultado %s (%s), botin %d de cuota %d, objetivo %s"),
		GetOwner()->HasAuthority() ? TEXT("servidor") : TEXT("cliente"),
		*UEnum::GetValueAsString(RunSummary.Outcome), *UEnum::GetValueAsString(RunSummary.EndReason),
		RunSummary.LootPointsDeposited, RunSummary.Quota, RunSummary.bObjectiveMet ? TEXT("cumplido") : TEXT("no cumplido"));
}

void UNDExpeditionComponent::BroadcastStateChanged()
{
	OnStateChanged.Broadcast();
}

// ---------------------------------------------------------------------------
// Replicación

void UNDExpeditionComponent::OnRep_Phase(ENDExpeditionPhase OldPhase)
{
	OnPhaseChanged.Broadcast(Phase, OldPhase);
}

void UNDExpeditionComponent::OnRep_Contract()
{
	BroadcastStateChanged();
}

void UNDExpeditionComponent::OnRep_VisibleState()
{
	BroadcastStateChanged();
}

void UNDExpeditionComponent::OnRep_RunSummary()
{
	if (!bSettledBroadcastLocally && RunSummary.Outcome != ENDExpeditionOutcome::None)
	{
		bSettledBroadcastLocally = true;
		LogRunSummary();
		OnExpeditionSettled.Broadcast(RunSummary);
	}
}
