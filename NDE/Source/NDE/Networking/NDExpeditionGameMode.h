// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDExpeditionGameMode.generated.h"

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

	/** Cierra la expedición y vuelve a La Madriguera. Se ejecuta una sola vez por RunGuid. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedicion")
	void EndExpedition();

protected:
	void CheckAllPlayersLoaded();
	void BeginExpedition();
	void TravelToLobby();

	FTimerHandle LoadCheckTimer;
	FTimerHandle ReturnTimer;
	double LoadingStartTime = 0.0;
	bool bSettlementStarted = false;
};
