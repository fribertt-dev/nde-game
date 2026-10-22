#include "UI/Data/NDUIExpeditionDataProvider.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Contracts/NDContractDefinition.h"
#include "Expedition/NDNightClockComponent.h"
#include "GameFramework/PlayerController.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
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

	// Manos y mochila del jugador local: no tienen evento propio, se avisa cuando cambian.
	if (Director)
	{
		const FNDExpeditionHUDVM HUD = GetExpeditionHUD_Implementation();
		const uint32 CarryHash = HashCombineFast(GetTypeHash(HUD.HandsItem.ToString()),
			HashCombineFast(GetTypeHash(HUD.PocketCount), GetTypeHash(HUD.bHandsNeedHelp)));
		if (CarryHash != LastCarryHash)
		{
			LastCarryHash = CarryHash;
			NotifyDataChanged(NDUITopics::ExpeditionHUD);
		}
	}

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
	VM.OutcomeTitle = GetOutcomeTitle(Summary.Outcome);
	VM.bContractCompleted = Summary.bObjectiveMet;
	VM.LootPoints = Summary.LootPointsDeposited;
	VM.Quota = Summary.Quota;
	if (!Summary.bObjectiveMet)
	{
		VM.FailureReason = LOCTEXT("ObjectiveMissed", "No llegó a la camioneta lo que pedía el contrato");
	}
	// Recompensa del jugador local.
	const UNDUIManagerSubsystem* UIManager = Cast<UNDUIManagerSubsystem>(GetOuter());
	const ULocalPlayer* LocalPlayer = UIManager ? UIManager->GetLocalPlayer() : nullptr;
	const APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController(GetPlayerWorld()) : nullptr;
	for (const FNDPlayerSettlement& Player : Summary.Players)
	{
		if (PC && Player.PlayerState == PC->PlayerState)
		{
			VM.MoneyEarned = Player.Reward.Coins;
			VM.ReputationEarned = Player.Reward.Reputation;
		}
	}
	return VM;
}

FText UNDUIExpeditionDataProvider::GetOutcomeTitle(ENDExpeditionOutcome Outcome)
{
	switch (Outcome)
	{
	case ENDExpeditionOutcome::Perfect: return LOCTEXT("OutcomePerfect", "¡Golpe perfecto!");
	case ENDExpeditionOutcome::Success: return LOCTEXT("OutcomeSuccess", "Éxito");
	case ENDExpeditionOutcome::PartialExtraction: return LOCTEXT("OutcomePartial", "Extracción parcial");
	case ENDExpeditionOutcome::Failure: return LOCTEXT("OutcomeFailure", "Fracaso");
	default: return FText::GetEmpty();
	}
}

FNDExpeditionHUDVM UNDUIExpeditionDataProvider::GetExpeditionHUD_Implementation() const
{
	const ANDExpeditionDirector* Director = GetDirector();
	const UNDContractDefinition* Contract = Director && Director->Expedition ? Director->Expedition->Contract.Get() : nullptr;
	if (!Contract)
	{
		return Super::GetExpeditionHUD_Implementation();
	}

	FNDExpeditionHUDVM VM;
	VM.ContractName = Contract->DisplayName;
	VM.bObjectiveMet = Director->Expedition->IsObjectiveMet();
	TArray<FString> Required;
	for (const FName& LootId : Contract->RequiredLootIds)
	{
		// El nombre visible sale del Data Asset del botín.
		const FString Id = LootId.ToString();
		const FSoftObjectPath Path(FString::Printf(TEXT("/Game/_NDE_/Data/Loot/DA_Loot_%s.DA_Loot_%s"), *Id, *Id));
		const UNDLootDefinition* Definition = Cast<UNDLootDefinition>(Path.TryLoad());
		Required.Add(Definition ? Definition->DisplayName.ToString() : Id);
	}
	if (!Required.IsEmpty())
	{
		VM.ObjectiveText = FText::Format(LOCTEXT("ObjectiveItems", "Llevad a la camioneta: {0}"), FText::FromString(FString::Join(Required, TEXT(", "))));
	}
	else
	{
		VM.ObjectiveText = FText::Format(LOCTEXT("ObjectiveQuota", "Llevad {0} puntos de botín a la camioneta"), Contract->RequiredQuotaValue);
	}

	const UWorld* World = GetPlayerWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const UNDCarryComponent* Carry = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UNDCarryComponent>() : nullptr;
	if (Carry)
	{
		if (const ANDLootActor* Held = Carry->GetHeldLoot())
		{
			VM.HandsItem = Held->GetDefinition() ? Held->GetDefinition()->DisplayName : FText::GetEmpty();
			VM.bHandsNeedHelp = !Held->HasEnoughCarriers();
		}
		VM.PocketCount = Carry->GetPocketLoot().Num();
		VM.PocketSlots = Carry->PocketSlots;
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
	NotifyDataChanged(NDUITopics::ExpeditionHUD);
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
