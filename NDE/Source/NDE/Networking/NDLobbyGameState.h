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

	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	ENDLobbyPhase GetPhase() const { return Phase; }

	/** Hora de servidor a la que sale la expedición; 0 si no hay cuenta atrás. */
	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	double GetTravelServerTime() const { return TravelServerTime; }

	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	int32 GetNumReadyPlayers() const;

	/** Solo servidor. */
	void SetPhase(ENDLobbyPhase NewPhase, double InTravelServerTime = 0.0);

	UPROPERTY(BlueprintAssignable, Category = "NDE|Sala")
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
};
