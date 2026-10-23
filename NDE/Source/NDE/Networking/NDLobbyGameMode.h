// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDLobbyGameMode.generated.h"

class ANDPlayerController;

/**
 * Reglas de La Madriguera (GDD 22.2): quién entra, "listo", votación de
 * contrato y salida a la expedición.
 */
UCLASS()
class NDE_API ANDLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDLobbyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Si el mapa tiene zonas de salida, solo cuenta "listo" dentro de una de ellas. */
	void HandleReadyRequest(ANDPlayerController* Requester, bool bReady);

	void HandleVoteRequest(ANDPlayerController* Requester, int32 ContractIndex);

	/** Pruebas (nde.Lobby.Start): sale ya hacia la expedición con el contrato más votado, sin zona ni "listo". */
	void DebugStartExpedition() { TravelToExpedition(); }

protected:
	bool IsInsideReadyZone(const ANDPlayerController* Requester) const;
	void SpawnReadyZoneIfMissing();

	void EvaluateReadyState();
	void StartTravelCountdown();
	void CancelTravelCountdown();
	void TravelToExpedition();

	/** Más votado; empate o sin votos, al azar entre los empatados (o entre todos). */
	int32 PickWinningContract() const;

	FTimerHandle TravelTimer;
};
