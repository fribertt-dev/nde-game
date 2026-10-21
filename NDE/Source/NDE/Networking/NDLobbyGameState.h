// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/NDTypes.h"
#include "NDLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnLobbyPhaseChanged, ENDLobbyPhase, NewPhase);

/** Estado público de la sala en La Madriguera (GDD 22.2). */
UCLASS()
class NDE_API ANDLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fase actual de La Madriguera. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	ENDLobbyPhase GetPhase() const { return Phase; }

	/** Hora de servidor a la que sale la expedición; 0 si no hay cuenta atrás. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	double GetTravelServerTime() const { return TravelServerTime; }

	/** Jugadores marcados como listos. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	int32 GetNumReadyPlayers() const;

	/** Contratos que se pueden votar. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	const TArray<FNDContractOption>& GetContractOptions() const { return ContractOptions; }

	/** Votos que lleva el contrato con este índice. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	int32 GetVoteCount(int32 ContractIndex) const;

	/** Contrato elegido al salir de expedición; INDEX_NONE hasta entonces. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	int32 GetSelectedContractIndex() const { return SelectedContractIndex; }

	/** Solo servidor. */
	void SetContractOptions(const TArray<FNDContractOption>& InOptions) { ContractOptions = InOptions; }
	void SetSelectedContractIndex(int32 Index) { SelectedContractIndex = Index; }

	/** Solo servidor. */
	void SetPhase(ENDLobbyPhase NewPhase, double InTravelServerTime = 0.0);

	/** Se dispara al cambiar la fase de La Madriguera. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Lobby")
	FNDOnLobbyPhaseChanged OnPhaseChanged;

private:
	UFUNCTION()
	void OnRep_Phase();

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ENDLobbyPhase Phase = ENDLobbyPhase::Waiting;

	/** Sube en cada cambio de fase, para que los clientes detecten saltos (GDD 22.5). */
	UPROPERTY(Replicated)
	int32 PhaseSequence = 0;

	UPROPERTY(Replicated)
	double TravelServerTime = 0.0;

	UPROPERTY(Replicated)
	TArray<FNDContractOption> ContractOptions;

	UPROPERTY(Replicated)
	int32 SelectedContractIndex = INDEX_NONE;
};
