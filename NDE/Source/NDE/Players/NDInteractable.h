// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NDInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, BlueprintType)
class UNDInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Algo del mundo con lo que un Desvelado interactúa con E (puertas, interruptores, la máquina de sueños...).
 * El botín, los escondites y la camioneta ya los resuelve UNDInteractionComponent sin esta interfaz.
 * El cliente solo lo enfoca y muestra el texto; Interact se ejecuta en el servidor.
 */
class NDE_API INDInteractable
{
	GENERATED_BODY()

public:
	/** Texto de la acción para el jugador, en español ("Abrir puerta"). Vacío = no se puede ahora. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|Interaction")
	FText GetInteractionText(APawn* Interactor) const;

	/** Solo servidor: ejecuta la acción. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|Interaction")
	void Interact(APawn* Interactor);
};
