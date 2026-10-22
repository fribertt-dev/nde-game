#include "UI/Data/NDUIExpeditionDataProvider.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Expedition/NDNightClockComponent.h"
#include "GameFramework/GameStateBase.h"
#include "UI/NDUIFunctionLibrary.h"
#include "UI/NDUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "NDUIExpeditionDataProvider"

namespace NDUIExpeditionData
{
	constexpr float PollSeconds = 0.25f;
}

void UNDUIExpeditionDataProvider::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::PollDirector), NDUIExpeditionData::PollSeconds);
	}
}

void UNDUIExpeditionDataProvider::BeginDestroy()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	Super::BeginDestroy();
}

UWorld* UNDUIExpeditionDataProvider::GetPlayerWorld() const
{
	// El subsistema del jugador local es el Outer (lo crea UNDUIManagerSubsystem).
	const UNDUIManagerSubsystem* UIManager = Cast<UNDUIManagerSubsystem>(GetOuter());
	const ULocalPlayer* LocalPlayer = UIManager ? UIManager->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
}

ANDExpeditionDirector* UNDUIExpeditionDataProvider::GetDirector() const
{
	return ANDExpeditionDirector::Get(GetPlayerWorld());
}

bool UNDUIExpeditionDataProvider::PollDirector(float DeltaTime)
{
	ANDExpeditionDirector* Director = GetDirector();
	if (Director == BoundDirector.Get())
	{
		return true;
	}
	BoundDirector = Director;
	if (Director && Director->Expedition && Director->NightClock)
	{
		Director->Expedition->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleExpeditionStateChanged);
		Director->Expedition->OnExpeditionSettled.AddUniqueDynamic(this, &ThisClass::HandleExpeditionSettled);
		Director->NightClock->OnNightPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleNightPhaseChanged);
		Director->NightClock->OnPreDawnWarning.AddUniqueDynamic(this, &ThisClass::HandlePreDawnWarning);
	}
	NotifyDataChanged(NDUITopics::NightClock);
	return true;
}

bool UNDUIExpeditionDataProvider::IsSimulated_Implementation() const
{
	return GetDirector() == nullptr;
}

ENDUINightPhase UNDUIExpeditionDataProvider::ToUIPhase(ENDExpeditionPhase ExpeditionPhase, ENDNightPhase NightPhase)
{
	switch (ExpeditionPhase)
	{
	case ENDExpeditionPhase::Insertion: return ENDUINightPhase::Insertion;
	case ENDExpeditionPhase::DawnEscapeGrace: return ENDUINightPhase::DawnEscapeGrace;
	case ENDExpeditionPhase::Settlement: return ENDUINightPhase::Settlement;
	default: break;
	}
	switch (NightPhase)
	{
	case ENDNightPhase::NotStarted:
	case ENDNightPhase::DeepNight: return ENDUINightPhase::Infiltration;
	case ENDNightPhase::Madrugada:
	case ENDNightPhase::PreDawn: return ENDUINightPhase::NightEscalation;
	default: return ENDUINightPhase::DawnTriggered;
	}
}

ENDUIRunOutcome UNDUIExpeditionDataProvider::ToUIOutcome(const FNDRunSummary& Summary)
{
	if (Summary.Outcome == ENDExpeditionOutcome::Failure)
	{
		return ENDUIRunOutcome::ContractFailed;
	}
	return Summary.bDawnTriggered ? ENDUIRunOutcome::EscapedDuringDawnChase : ENDUIRunOutcome::EscapedBeforeDawn;
}

FNDNightClockVM UNDUIExpeditionDataProvider::GetNightClock_Implementation() const
{
	const ANDExpeditionDirector* Director = GetDirector();
	if (!Director || !Director->Expedition || !Director->NightClock)
	{
		return Super::GetNightClock_Implementation();
	}
	const UNDExpeditionComponent* Expedition = Director->Expedition;
	const UNDNightClockComponent* Clock = Director->NightClock;
	const AGameStateBase* GameState = Director->GetWorld()->GetGameState();
	const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : Director->GetWorld()->GetTimeSeconds();

	FNDNightClockVM VM;
	VM.Phase = ToUIPhase(Expedition->GetPhase(), Clock->GetPhase());
	VM.NightDurationSeconds = Clock->GetNightDurationSeconds();
	VM.DawnEscapeSeconds = Clock->GetDawnGraceSeconds();
	// La UI interpola desde el inicio de la noche y desde el amanecer; se deducen de lo que queda.
	if (UNDUIFunctionLibrary::IsDawn(VM))
	{
		VM.DawnTriggeredServerTime = ServerNow - (VM.DawnEscapeSeconds - Clock->GetGraceSecondsRemaining());
		VM.NightStartServerTime = VM.DawnTriggeredServerTime - VM.NightDurationSeconds;
	}
	else
	{
		VM.NightStartServerTime = ServerNow - (VM.NightDurationSeconds - Clock->GetSecondsUntilDawn());
		VM.DawnTriggeredServerTime = VM.NightStartServerTime + VM.NightDurationSeconds;
	}
	VM.Quota = Expedition->GetQuota();
	VM.SecuredLootValue = Expedition->GetLootPoints();
	return VM;
}

FNDResultsVM UNDUIExpeditionDataProvider::GetLastResults_Implementation() const
{
	const ANDExpeditionDirector* Director = GetDirector();
	if (!Director || !Director->Expedition || Director->Expedition->GetRunSummary().Outcome == ENDExpeditionOutcome::None)
	{
		return Super::GetLastResults_Implementation();
	}
	const FNDRunSummary& Summary = Director->Expedition->GetRunSummary();
	FNDResultsVM VM;
	VM.Outcome = ToUIOutcome(Summary);
	VM.bContractCompleted = Summary.bObjectiveMet;
	if (!Summary.bObjectiveMet)
	{
		VM.FailureReason = LOCTEXT("ObjectiveMissed", "No llegó a la camioneta lo que pedía el contrato");
	}
	return VM;
}

void UNDUIExpeditionDataProvider::ShowToast(const FText& Message, int32 Priority) const
{
	if (UNDUIManagerSubsystem* UIManager = Cast<UNDUIManagerSubsystem>(GetOuter()))
	{
		FNDUIToast Toast;
		Toast.Message = Message;
		Toast.Priority = Priority;
		UIManager->ShowToast(Toast);
	}
}

void UNDUIExpeditionDataProvider::HandleExpeditionStateChanged()
{
	NotifyDataChanged(NDUITopics::NightClock);
}

void UNDUIExpeditionDataProvider::HandleNightPhaseChanged(ENDNightPhase NewPhase, ENDNightPhase OldPhase)
{
	NotifyDataChanged(NDUITopics::NightClock);
	if (NewPhase == ENDNightPhase::DawnGrace)
	{
		ShowToast(LOCTEXT("Dawn", "¡Amanece! Corred a la camioneta"), 10);
	}
}

void UNDUIExpeditionDataProvider::HandlePreDawnWarning(int32 SecondsUntilDawn)
{
	ShowToast(FText::Format(LOCTEXT("PreDawn", "Amanece en {0} s"), SecondsUntilDawn), 5);
}

void UNDUIExpeditionDataProvider::HandleExpeditionSettled(const FNDRunSummary& Summary)
{
	NotifyDataChanged(NDUITopics::Results);
	ShowToast(Summary.bObjectiveMet ? LOCTEXT("ContractDone", "Contrato cumplido") : LOCTEXT("ContractFailed", "Contrato fallido"), 10);
}

#undef LOCTEXT_NAMESPACE
