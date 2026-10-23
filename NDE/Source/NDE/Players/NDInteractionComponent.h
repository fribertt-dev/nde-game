// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NDInteractionComponent.generated.h"

class ANDLootActor;

/** Qué haría E ahora mismo. */
UENUM(BlueprintType)
enum class ENDInteractionKind : uint8
{
	None UMETA(ToolTip = "Nada a mano."),
	Van UMETA(ToolTip = "Camioneta: votar la huida o evacuar."),
	ExitHiding UMETA(ToolTip = "Salir del escondite."),
	Loot UMETA(ToolTip = "Coger botín o ayudar a cargarlo."),
	HidingSpot UMETA(ToolTip = "Entrar en un escondite."),
	Interactable UMETA(ToolTip = "Un actor con la interfaz NDInteractable (puerta, interruptor...).")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDOnInteractionFocusChanged, ENDInteractionKind, Kind, const FText&, PromptText);

/**
 * La acción contextual de E (GDD 3.3): decide en el jugador local qué tiene a mano (camioneta,
 * escondite, botín o un actor NDInteractable) y lo ejecuta pidiéndolo al servidor por el componente
 * que corresponde. La UI lee GetPromptText para el texto bajo la retícula.
 * Prioridad: salir del escondite > camioneta > botín delante > escondite cercano > interactuable.
 */
UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDInteractionComponent();

	/** Texto para el jugador ("E  Coger Reloj"); vacío si no hay nada. */
	UFUNCTION(BlueprintPure, Category = "NDE|Interaction")
	FText GetPromptText() const { return PromptText; }

	UFUNCTION(BlueprintPure, Category = "NDE|Interaction")
	bool HasFocus() const { return Kind != ENDInteractionKind::None; }

	UFUNCTION(BlueprintPure, Category = "NDE|Interaction")
	ENDInteractionKind GetFocusKind() const { return Kind; }

	/** Ejecuta lo enfocado (lo llama E). */
	UFUNCTION(BlueprintCallable, Category = "NDE|Interaction")
	void Interact();

	/** Cambió lo que haría E (solo en el jugador local). */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Interaction")
	FNDOnInteractionFocusChanged OnFocusChanged;

	/** Distancia para actores NDInteractable, en cm (GDD 3.3: 180). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Interaction", meta = (ClampMin = "50"))
	float InteractableRange = 180.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerInteractWith(AActor* Target);

	/** Recalcula lo enfocado (local). */
	void UpdateFocus();

	/** Actor NDInteractable al que se apunta con la cámara, o nullptr. */
	AActor* TraceInteractable(FText& OutText) const;

	/** Pinta el texto si no hay UI que lo haga (nde.Interaction.DrawPrompt). */
	void DrawFallbackPrompt() const;

	ENDInteractionKind Kind = ENDInteractionKind::None;
	FText PromptText;

	UPROPERTY(Transient)
	TWeakObjectPtr<ANDLootActor> FocusedLoot;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> FocusedActor;
};
