// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDExpeditionGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnExpeditionPlayerConnection, APlayerState*, PlayerState);

/**
 * Orquesta la expedición en el servidor (GDD 22.2): espera a que carguen todos,
 * abre la partida, congela resultados y vuelve a La Madriguera.
 * Las reglas de juego (fases 7.1, amanecer, cuota) las decide UNDExpeditionComponent.
 */
UCLASS()
class NDE_API ANDExpeditionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDExpeditionGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Cierra la expedición y vuelve a La Madriguera. Se ejecuta una sola vez por RunGuid. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void EndExpedition();

	/**
	 * Entrega a un jugador su recompensa de esta expedición; su perfil la guarda una
	 * sola vez por RunGuid. Llamar antes de EndExpedition con los datos del resumen.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void GrantRunReward(APlayerController* Player, int32 Coins, int32 Reputation);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void GrantRunRewardToAll(int32 Coins, int32 Reputation);

	/**
	 * Un jugador se cayó o salió en plena expedición. Su PlayerState aún es válido
	 * durante el aviso: es el momento de soltar lo que cargaba y marcarlo como desconectado.
	 */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDOnExpeditionPlayerConnection OnPlayerDisconnected;

	/** Un jugador caído volvió dentro de la ventana de rejoin. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDOnExpeditionPlayerConnection OnPlayerRejoined;

protected:
	void CheckAllPlayersLoaded();
	void BeginExpedition();
	void TravelToLobby();

	FTimerHandle LoadCheckTimer;
	FTimerHandle ReturnTimer;
	double LoadingStartTime = 0.0;
	bool bSettlementStarted = false;

private:
	/** Id de plataforma de quien se cayó → hora (GetTimeSeconds) en que se cayó. */
	TMap<FString, double> DisconnectedPlayers;

	bool IsWithinRejoinWindow(const FUniqueNetIdRepl& UniqueId) const;
};
