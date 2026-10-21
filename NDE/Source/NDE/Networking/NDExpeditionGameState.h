// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/NDTypes.h"
#include "NDExpeditionGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnExpeditionSessionPhaseChanged, ENDExpeditionSessionPhase, NewPhase);

/**
 * Estado público de la expedición (GDD 22.2). Aquí se colgarán los componentes
 * de reglas (reloj de noche, contrato, cuota) del hilo de expedición.
 */
UCLASS()
class NDE_API ANDExpeditionGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	ENDExpeditionSessionPhase GetSessionPhase() const { return SessionPhase; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 GetPhaseSequence() const { return PhaseSequence; }

	/** Hora de servidor en que empezó la fase actual (usar GetServerWorldTimeSeconds para comparar). */
	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	double GetPhaseStartServerTime() const { return PhaseStartServerTime; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	FGuid GetRunGuid() const { return RunGuid; }

	/** Contrato votado en La Madriguera (NAME_None si se abrió el mapa directamente). */
	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	FName GetContractId() const { return ContractId; }

	void SetContractId(FName InContractId) { if (HasAuthority()) { ContractId = InContractId; } }

	/** Solo servidor. */
	void SetSessionPhase(ENDExpeditionSessionPhase NewPhase);
	void SetRunGuid(const FGuid& InRunGuid);

	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDOnExpeditionSessionPhaseChanged OnSessionPhaseChanged;

private:
	UFUNCTION()
	void OnRep_PhaseSequence();

	UPROPERTY(Replicated)
	ENDExpeditionSessionPhase SessionPhase = ENDExpeditionSessionPhase::Loading;

	/** Monotónica. El aviso salta con ella para que cada transición se emita una sola vez, también tras rejoin. */
	UPROPERTY(ReplicatedUsing = OnRep_PhaseSequence)
	int32 PhaseSequence = 0;

	UPROPERTY(Replicated)
	double PhaseStartServerTime = 0.0;

	UPROPERTY(Replicated)
	FGuid RunGuid;

	UPROPERTY(Replicated)
	FName ContractId;

	int32 LastNotifiedSequence = 0;
};
