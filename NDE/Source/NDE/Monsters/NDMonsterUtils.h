// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"

class APawn;
class APlayerController;
class UNDMonsterDefinition;
class UObject;

/** Ayudas de la criatura que hablan con otros sistemas (casa, expedición, personaje, interfaz). */
namespace NDMonsterUtils
{
	/** Paso del sonido entre dos puntos por las puertas de la casa (1 si no hay casa generada). */
	float GetSoundTransmission(const UObject* WorldContext, const FVector& A, const FVector& B);

	/**
	 * 0-1 (hasta 1,5 con linterna): cuánto se ve a un Desvelado. Usa UNDHidingComponent del personaje si
	 * existe (por reflexión, GetVisibilityFactor e IsHidden); si no, agachado = CrouchedVisibility.
	 */
	float GetPawnVisibility(const APawn* Pawn, const UNDMonsterDefinition& Def);

	/** true si está metido en un escondite (no se le ve ni se le puede coger). */
	bool IsPawnHidden(const APawn* Pawn);

	/** true si el jugador puede ser objetivo: vivo, en la expedición y no capturado ni fuera. */
	bool IsTargetablePlayer(const APawn* Pawn);

	/** Aviso en pantalla del jugador local (toast de la interfaz). */
	void ShowLocalToast(APlayerController* PC, const FText& Message, float Seconds, int32 Priority = 5);
}
