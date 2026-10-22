#pragma once

#include "CoreMinimal.h"
#include "Core/NDDesveladoTypes.h"
#include "NDUIViewModels.generated.h"

class UTexture2D;

// Modelos de solo lectura que la UI recibe del proveedor de datos (GDD 32.6).
// Los WBP los muestran; los cambios se piden con comandos al proveedor, nunca mutando estos datos.

USTRUCT(BlueprintType)
struct FNDPlayerCardVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FString PlayerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	ENDDesveladoSpecies Species = ENDDesveladoSpecies::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsLocalPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsHost = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Reputation = 0;
};

USTRUCT(BlueprintType)
struct FNDContractCardVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName ContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText Family;

	// 1 = I, 2 = II...
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "1"))
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText MainObjective;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText SecondaryObjective;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	float NightDurationMinutes = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Votes = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bLocked = false;
};

USTRUCT(BlueprintType)
struct FNDShopItemVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText Drawback;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bOwned = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FNDInventorySlotVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsQuickConsumable = false;

	// NAME_None si el slot está vacío.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Charges = -1;
};

// Fase del reloj tal como la muestra la UI. Las reglas usan ENDNightPhase y ENDExpeditionPhase (Expedition/NDExpeditionTypes.h).
UENUM(BlueprintType)
enum class ENDUINightPhase : uint8
{
	Insertion,
	Infiltration,
	NightEscalation,
	DawnTriggered,
	DawnEscapeGrace,
	Settlement,
};

// La UI interpola localmente desde NightStartServerTime y se recalibra con el GameState (GDD 32.6).
USTRUCT(BlueprintType)
struct FNDNightClockVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	ENDUINightPhase Phase = ENDUINightPhase::Infiltration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	double NightStartServerTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	float NightDurationSeconds = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	double DawnTriggeredServerTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	float DawnEscapeSeconds = 45.f;

	// Umbrales de WBP_DawnWarning, en segundos restantes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TArray<float> DawnWarningThresholds = { 120.f, 60.f, 30.f, 10.f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Quota = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 SecuredLootValue = 0;
};

// Lo que el HUD de expedición enseña además del reloj: objetivo del contrato y carga del jugador local.
USTRUCT(BlueprintType)
struct FNDExpeditionHUDVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText ContractName;

	// Qué hay que llevar a la camioneta, en una frase.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText ObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bObjectiveMet = false;

	// Nombre de lo que lleva en las manos. Vacío = nada.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText HandsItem;

	// Lo que lleva en las manos necesita otro portador para moverse a velocidad normal.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bHandsNeedHelp = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 PocketCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 PocketSlots = 3;
};

USTRUCT(BlueprintType)
struct FNDPlayerVoiceVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FString PlayerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsSpeaking = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsAudible = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bIsMuted = false;

	// Riesgo de que el dueño oiga, 0..1 y por tramos: nunca la amplitud exacta (GDD 32.7).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0", ClampMax = "1"))
	float MonsterHearingRisk = 0.f;
};

// Resultado tal como lo cuenta la pantalla de resultados. Las reglas usan ENDExpeditionOutcome.
UENUM(BlueprintType)
enum class ENDUIRunOutcome : uint8
{
	EscapedBeforeDawn,
	EscapedDuringDawnChase,
	CapturedAtDawn,
	ContractFailed,
};

USTRUCT(BlueprintType)
struct FNDResultsVM
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	ENDUIRunOutcome Outcome = ENDUIRunOutcome::EscapedBeforeDawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bContractCompleted = false;

	// Explicación visible si el contrato falló.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText FailureReason;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 MoneyEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 ReputationEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TArray<FText> FunnyIncidents;

	// Título del resultado: Perfecto, Éxito, Extracción parcial o Fracaso (GDD 7.2).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText OutcomeTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 LootPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Quota = 0;
};

// Respuesta a un comando de la UI. bSimulated indica que no hubo escritura persistente.
USTRUCT(BlueprintType)
struct FNDUICommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	FName Command;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	bool bSimulated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	FText Message;
};
