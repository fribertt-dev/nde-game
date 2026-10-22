#include "Contracts/NDSettlementLibrary.h"

int32 UNDSettlementLibrary::ComputeLootPoints(const TArray<FNDDepositRecord>& Deposits)
{
	int32 Points = 0;
	for (const FNDDepositRecord& Record : Deposits)
	{
		Points += Record.GetLootPoints();
	}
	return Points;
}

int32 UNDSettlementLibrary::ComputeLootSoldValue(const TArray<FNDDepositRecord>& Deposits, float ContractValueMultiplier)
{
	double Sum = 0.0;
	for (const FNDDepositRecord& Record : Deposits)
	{
		Sum += static_cast<double>(Record.BaseValue) * Record.ConditionFactor * ContractValueMultiplier;
	}
	return FMath::Max(0, FMath::RoundToInt(Sum));
}

int32 UNDSettlementLibrary::ComputeExtraLootBonus(int32 LootSoldValue, int32 Quota, int32 BasePayout, const FNDEconomyTuning& Tuning)
{
	if (Tuning.BonusStep <= 0)
	{
		return 0;
	}
	const int32 Surplus = FMath::Max(0, LootSoldValue - Quota);
	const int32 Bonus = (Surplus / Tuning.BonusStep) * Tuning.CoinsPerStep;
	const int32 Cap = FMath::FloorToInt(BasePayout * Tuning.ExtraLootBonusCapRate);
	return FMath::Clamp(Bonus, 0, FMath::Max(0, Cap));
}

ENDExpeditionOutcome UNDSettlementLibrary::DetermineOutcome(bool bObjectiveMet, bool bSecondaryMet, int32 NumExtracted, int32 NumParticipants)
{
	if (NumExtracted <= 0)
	{
		return ENDExpeditionOutcome::Failure;
	}
	if (!bObjectiveMet)
	{
		return ENDExpeditionOutcome::PartialExtraction;
	}
	const bool bEveryoneOut = NumParticipants > 0 && NumExtracted >= NumParticipants;
	return (bEveryoneOut && bSecondaryMet) ? ENDExpeditionOutcome::Perfect : ENDExpeditionOutcome::Success;
}

FNDPlayerReward UNDSettlementLibrary::ComputePlayerReward(
	ENDExpeditionOutcome Outcome,
	ENDParticipantStatus FinalStatus,
	bool bSecondaryMet,
	bool bTeamRescueHappened,
	int32 LootSoldValue,
	int32 Quota,
	int32 BasePayout,
	int32 TierReputation,
	const FNDEconomyTuning& Tuning)
{
	FNDPlayerReward Reward;

	switch (Outcome)
	{
	case ENDExpeditionOutcome::Perfect:
	case ENDExpeditionOutcome::Success:
	{
		const bool bExtracted = FinalStatus == ENDParticipantStatus::Extracted;
		const int32 ExtraLoot = ComputeExtraLootBonus(LootSoldValue, Quota, BasePayout, Tuning);
		const int32 Optional = (bExtracted && bSecondaryMet) ? FMath::RoundToInt(BasePayout * Tuning.OptionalObjectiveBonusRate) : 0;
		const int32 Rescue = (bExtracted && bTeamRescueHappened) ? FMath::RoundToInt(BasePayout * Tuning.TeamRescueBonusRate) : 0;
		Reward.Coins = FMath::Max(0, BasePayout + ExtraLoot + Optional + Rescue);

		// Sin reputación por abandono (GDD 16.3).
		if (FinalStatus != ENDParticipantStatus::Disconnected)
		{
			const int32 SecondaryRep = (bExtracted && bSecondaryMet) ? FMath::RoundToInt(TierReputation * Tuning.SecondaryReputationRate) : 0;
			Reward.Reputation = TierReputation + SecondaryRep;
		}
		break;
	}
	case ENDExpeditionOutcome::PartialExtraction:
		// Fracción del valor vendido, sin pago fijo ni progreso de contrato.
		Reward.Coins = FMath::Max(0, FMath::RoundToInt(LootSoldValue * Tuning.PartialLootReturnRate));
		break;
	case ENDExpeditionOutcome::Failure:
		Reward.Coins = FMath::Max(0, FMath::RoundToInt(LootSoldValue * Tuning.FailedDepositReturnRate));
		break;
	default:
		break;
	}

	return Reward;
}
