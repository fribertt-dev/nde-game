// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NDPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDOnReadyChanged, class ANDPlayerState*, PlayerState, bool, bIsReady);

/**
 * Estado público por jugador (GDD 22.3): listo, y más adelante equipamiento
 * visible y resultados. La moneda personal no va aquí: llega por el perfil.
 */
UCLASS()
class NDE_API ANDPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	/** true si el jugador está listo para salir de expedición. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	bool IsReady() const { return bIsReady; }

	/** Solo servidor. Los clientes piden el cambio con ANDPlayerController::ServerSetReady. */
	void SetReady(bool bNewReady);

	/** Se dispara cuando el jugador cambia su estado de listo. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Lobby")
	FNDOnReadyChanged OnReadyChanged;

	/** Índice del contrato votado en ANDLobbyGameState::GetContractOptions; INDEX_NONE sin voto. */
	UFUNCTION(BlueprintPure, Category = "NDE|Lobby")
	int32 GetVotedContractIndex() const { return VotedContractIndex; }

	/** Solo servidor (un voto por jugador, el último cuenta). */
	void SetVotedContractIndex(int32 NewIndex) { VotedContractIndex = NewIndex; }

private:
	UFUNCTION()
	void OnRep_IsReady();

	UPROPERTY(ReplicatedUsing = OnRep_IsReady)
	bool bIsReady = false;

	UPROPERTY(Replicated)
	int32 VotedContractIndex = INDEX_NONE;
};
