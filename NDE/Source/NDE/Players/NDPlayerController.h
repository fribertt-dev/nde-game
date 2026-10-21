// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NDPlayerController.generated.h"

class UNDVoiceComponent;

/**
 * Entrada del jugador propietario y RPC autorizadas (GDD 22.2). Cada RPC solo
 * pide; el GameMode decide.
 */
UCLASS()
class NDE_API ANDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANDPlayerController();

	UFUNCTION(BlueprintCallable, Category = "NDE|Sala")
	void RequestSetReady(bool bReady);

	/** Vota un contrato de La Madriguera (índice en ANDLobbyGameState::GetContractOptions). */
	UFUNCTION(BlueprintCallable, Category = "NDE|Sala")
	void RequestVote(int32 ContractIndex);

	/** Atajo de consola: NDVote 1 vota el primer contrato. */
	UFUNCTION(Exec)
	void NDVote(int32 OptionNumber);

	/** El servidor entrega la recompensa de una expedición; el perfil local la guarda una sola vez. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveRunReward(const FGuid& RunGuid, int32 Coins, int32 Reputation);

	UFUNCTION(BlueprintPure, Category = "NDE|Voz")
	UNDVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

	/** Atajo de consola: alterna "listo" en La Madriguera. */
	UFUNCTION(Exec)
	void NDReady();

	/** Desarrollo: termina la expedición, da monedas de prueba a todos y vuelve a La Madriguera. Sin efecto en Shipping. */
	UFUNCTION(Exec)
	void NDDebugReturnToLobby(int32 Coins = 100);

	/** Sale de la sala y vuelve al menú principal. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Sala")
	void LeaveSession();

	/** El host avisa de que cierra la partida, para que el cliente no ofrezca volver a ella. */
	UFUNCTION(Client, Reliable)
	void ClientHostClosedSession();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void PawnLeavingGame() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerDebugReturnToLobby(int32 Coins);

	UFUNCTION(Server, Reliable)
	void ServerSubmitVote(int32 ContractIndex);

private:
	/** Muestra el estado de la sala en La Madriguera y lo quita fuera de ella. */
	void UpdateLobbyStatusWidget();

	/** Evita que un cliente sature al servidor alternando "listo". */
	double LastReadyRequestTime = -1.0;

	UPROPERTY(Transient)
	TObjectPtr<class UNDLobbyStatusWidget> LobbyStatusWidget;

	bool bWarnedMissingLobbyWidget = false;

	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();
	void HandleVoteKey1() { RequestVote(0); }
	void HandleVoteKey2() { RequestVote(1); }
	void HandleVoteKey3() { RequestVote(2); }

	UPROPERTY(VisibleAnywhere, Category = "NDE|Voz")
	TObjectPtr<UNDVoiceComponent> VoiceComponent;
};
