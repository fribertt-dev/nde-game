// Cálculo puro de resultado y liquidación (GDD 7.2 y 16.3). Sin estado, apto para tests.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDSettlementLibrary.generated.h"

UCLASS()
class NDE_API UNDSettlementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Suma de puntos válidos (BaseValue * ConditionFactor). Se usa para la cuota. */
	UFUNCTION(BlueprintPure, Category = "NDE|Settlement")
	static int32 ComputeLootPoints(const TArray<FNDDepositRecord>& Deposits);

	/** LootSoldValue = SUM(BaseValue * ConditionFactor * ContractValueMultiplier). */
	UFUNCTION(BlueprintPure, Category = "NDE|Settlement")
	static int32 ComputeLootSoldValue(const TArray<FNDDepositRecord>& Deposits, float ContractValueMultiplier);

	/** clamp(floor(max(0, Sold - Quota) / BonusStep) * CoinsPerStep, 0, BasePayout * CapRate). */
	UFUNCTION(BlueprintPure, Category = "NDE|Settlement")
	static int32 ComputeExtraLootBonus(int32 LootSoldValue, int32 Quota, int32 BasePayout, const FNDEconomyTuning& Tuning);

	/** Éxito exige objetivo y al menos un extraído; sin objetivo pero con extraídos es parcial. */
	UFUNCTION(BlueprintPure, Category = "NDE|Settlement")
	static ENDExpeditionOutcome DetermineOutcome(bool bObjectiveMet, bool bSecondaryMet, int32 NumExtracted, int32 NumParticipants);

	/** Recompensa de un jugador. Capturado o abandonado pierde los bonos personales. */
	UFUNCTION(BlueprintPure, Category = "NDE|Settlement")
	static FNDPlayerReward ComputePlayerReward(
		ENDExpeditionOutcome Outcome,
		ENDParticipantStatus FinalStatus,
		bool bSecondaryMet,
		bool bTeamRescueHappened,
		int32 LootSoldValue,
		int32 Quota,
		int32 BasePayout,
		int32 TierReputation,
		const FNDEconomyTuning& Tuning);
};
