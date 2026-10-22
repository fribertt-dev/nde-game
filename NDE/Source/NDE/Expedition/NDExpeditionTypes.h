// Tipos compartidos del bucle de expedición (GDD caps. 7, 8, 12 y 16).

#pragma once

#include "CoreMinimal.h"
#include "NDExpeditionTypes.generated.h"

class APlayerState;

/** Categoría de transporte del botín (GDD 8.2). La categoría manda sobre el peso. */
UENUM(BlueprintType)
enum class ENDLootCategory : uint8
{
	Pocket,
	Light,
	Medium,
	Heavy,
	Huge,
	Unique,
	Objective
};

/** Banda de fragilidad (GDD 8.3). */
UENUM(BlueprintType)
enum class ENDFragility : uint8
{
	Sturdy,
	Delicate,
	Critical
};

UENUM(BlueprintType)
enum class ENDLootCondition : uint8
{
	Intact,
	Damaged,
	Broken
};

/** Estado de un objeto de botín en el mundo (GDD 8.4). */
UENUM(BlueprintType)
enum class ENDLootState : uint8
{
	WorldLoose,
	Carried,
	Pocketed,
	Deposited
};

/** Fase visual y de reglas del reloj nocturno (GDD 7.6). */
UENUM(BlueprintType)
enum class ENDNightPhase : uint8
{
	NotStarted,
	DeepNight,
	Madrugada,
	PreDawn,
	DawnGrace,
	DayEnded
};

/** Fase de reglas de la expedición (GDD 7.1). */
UENUM(BlueprintType)
enum class ENDExpeditionPhase : uint8
{
	Insertion,
	Infiltration,
	ExtractionReady,
	DawnEscapeGrace,
	Settlement
};

UENUM(BlueprintType)
enum class ENDObjectiveType : uint8
{
	Quota,
	UniqueItem,
	HeavyItem
};

UENUM(BlueprintType)
enum class ENDSecondaryObjective : uint8
{
	None,
	/** Ningún objeto delicado o crítico roto durante la expedición. */
	NoFragileBroken,
	/** Los objetos requeridos llegan intactos a la camioneta. */
	RequiredLootIntact,
	/** Todos extraídos antes de que salga el sol. */
	AllExtractedBeforeDawn
};

UENUM(BlueprintType)
enum class ENDParticipantStatus : uint8
{
	Active,
	Captured,
	Extracted,
	LeftBehind,
	Disconnected
};

/** Resultado de la expedición (GDD 7.2). */
UENUM(BlueprintType)
enum class ENDExpeditionOutcome : uint8
{
	None,
	Perfect,
	Success,
	PartialExtraction,
	Failure
};

UENUM(BlueprintType)
enum class ENDExpeditionEndReason : uint8
{
	None,
	Evacuated,
	DayEnded,
	AllCaptured,
	Aborted
};

/** Entrada del libro de depósitos de la camioneta. Una por LootGuid, nunca dos. */
USTRUCT(BlueprintType)
struct NDE_API FNDDepositRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	FGuid LootGuid;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	FName LootId;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	int32 BaseValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	ENDLootCondition Condition = ENDLootCondition::Intact;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	ENDFragility Fragility = ENDFragility::Sturdy;

	/** Factor de valor según el estado (1, 0.65, 0.2 por defecto). */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	float ConditionFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Loot")
	TObjectPtr<APlayerState> Depositor = nullptr;

	/** Puntos válidos para la cuota: BaseValue * ConditionFactor. */
	int32 GetLootPoints() const { return FMath::RoundToInt(BaseValue * ConditionFactor); }
};

USTRUCT(BlueprintType)
struct NDE_API FNDParticipant
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	ENDParticipantStatus Status = ENDParticipantStatus::Active;

	/** Fue capturado y rescatado al menos una vez (bono de rescate de equipo). */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	bool bWasRescued = false;
};

/** Parámetros económicos iniciales (GDD 16.3). Todo CONFIGURABLE. */
USTRUCT(BlueprintType)
struct NDE_API FNDEconomyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	int32 BonusStep = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	int32 CoinsPerStep = 10;

	/** Tope del bono por botín extra, como fracción del pago base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float ExtraLootBonusCapRate = 0.35f;

	/** Fracción del valor vendido en una extracción parcial (0.10 a 0.25). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float PartialLootReturnRate = 0.2f;

	/** Fracción del botín depositado que se rescata en un fracaso (GDD 16.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float FailedDepositReturnRate = 0.05f;

	/** Bono por objetivo secundario, como fracción del pago base (0.10 a 0.20). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float OptionalObjectiveBonusRate = 0.15f;

	/** Bono de rescate de equipo, como fracción del pago base (0.05 a 0.10). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float TeamRescueBonusRate = 0.075f;

	/** Reputación extra por objetivo secundario, como fracción de la del tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|Economy")
	float SecondaryReputationRate = 0.2f;
};

USTRUCT(BlueprintType)
struct NDE_API FNDPlayerReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	int32 Coins = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	int32 Reputation = 0;
};

USTRUCT(BlueprintType)
struct NDE_API FNDPlayerSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	ENDParticipantStatus FinalStatus = ENDParticipantStatus::Active;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Economy")
	FNDPlayerReward Reward;
};

/** Resumen de la expedición, calculado una sola vez en el servidor (GDD 7.9 y 16.3). */
USTRUCT(BlueprintType)
struct NDE_API FNDRunSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	FGuid RunGuid;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	FName ContractId;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	ENDExpeditionOutcome Outcome = ENDExpeditionOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	ENDExpeditionEndReason EndReason = ENDExpeditionEndReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	int32 LootPointsDeposited = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	int32 LootSoldValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	int32 Quota = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	bool bObjectiveMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	bool bSecondaryMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	bool bDawnTriggered = false;

	/** Segundos de noche que quedaban al cerrarse la expedición (0 si ya amaneció). */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	float RemainingTimeAtExtraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	int32 DawnSurvivors = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	int32 DawnCaptures = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Expedition")
	TArray<FNDPlayerSettlement> Players;
};
