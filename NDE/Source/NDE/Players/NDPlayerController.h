// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NDPlayerController.generated.h"

/**
 * Entrada del jugador propietario y RPC autorizadas (GDD 22.2). Cada RPC solo
 * pide; el GameMode decide.
 */
UCLASS()
class NDE_API ANDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NDE|Sala")
	void RequestSetReady(bool bReady);

	/** Atajo de consola: alterna "listo" en La Madriguera. */
	UFUNCTION(Exec)
	void NDReady();

	/** Desarrollo: termina la expedición y vuelve a La Madriguera. Sin efecto en Shipping. */
	UFUNCTION(Exec)
	void NDDebugReturnToLobby();

	/** Sale de la sala y vuelve al menú principal. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Sala")
	void LeaveSession();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerDebugReturnToLobby();

private:
	/** Muestra el estado de la sala en La Madriguera y lo quita fuera de ella. */
	void UpdateLobbyStatusWidget();

	/** Evita que un cliente sature al servidor alternando "listo". */
	double LastReadyRequestTime = -1.0;

	UPROPERTY(Transient)
	TObjectPtr<class UNDLobbyStatusWidget> LobbyStatusWidget;

	bool bWarnedMissingLobbyWidget = false;
};
