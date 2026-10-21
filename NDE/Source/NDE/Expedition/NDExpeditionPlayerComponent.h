// Acciones del jugador sobre las reglas de la expedición (camioneta y votación).
// Va en el pawn para tener conexión propietaria y poder enviar RPC al servidor.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NDExpeditionPlayerComponent.generated.h"

UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDExpeditionPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDExpeditionPlayerComponent();

	/** Interactuar con la camioneta: iniciar votación, votar sí o evacuar al amanecer. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Expedition")
	void RequestVanInteraction();

	UFUNCTION(BlueprintCallable, Category = "NDE|Expedition")
	void RequestEscapeVote(bool bYes);

	/** True si este pawn está dentro de la zona segura de la camioneta. */
	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool IsInsideVan() const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerVanInteraction();

	UFUNCTION(Server, Reliable)
	void ServerEscapeVote(bool bYes);

	class APlayerState* GetOwnerPlayerState() const;
};
