// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NDReadyZone.generated.h"

class AController;
class UBoxComponent;
class UTextRenderComponent;

/**
 * Zona de salida de La Madriguera: entrar marca "listo" y salir lo quita.
 * Solo el servidor decide; el GameMode valida la petición. Si el mapa no
 * tiene ninguna, ANDLobbyGameMode crea una delante del primer PlayerStart.
 */
UCLASS()
class NDE_API ANDReadyZone : public AActor
{
	GENERATED_BODY()

public:
	ANDReadyZone();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	/** true si el pawn de este controlador está dentro de la zona. */
	bool ContainsController(const AController* Controller) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NDE|Lobby")
	TObjectPtr<UBoxComponent> Zone;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Lobby")
	TObjectPtr<UTextRenderComponent> Label;

private:
	void HandleOverlap(AActor* OtherActor, bool bInside);
};
