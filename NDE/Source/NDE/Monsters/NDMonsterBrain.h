// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Monsters/NDMonsterTypes.h"

class UNDMonsterDefinition;

/**
 * Cuentas de la criatura sin estado ni mundo: sospecha, umbrales con histéresis, oído y ronquido.
 * Las usa ANDMonsterAIController y las prueban los tests NDE.Monster.
 */
namespace NDMonsterBrain
{
	/** true en DeepSleep, LightSleep, Stirring y HalfAwake (en la cama). */
	NDE_API bool IsInBed(ENDMonsterState State);

	/** true si ronca o puede roncar (DeepSleep y LightSleep). */
	NDE_API bool IsSnoringState(ENDMonsterState State);

	/** Parte del sonido que llega a una distancia (1 pegado, 0,5 a HearingHalfDistance, 0 tras HearingMaxDistance). */
	NDE_API float GetDistanceAttenuation(const UNDMonsterDefinition& Def, float DistanceCm);

	/** Sensibilidad del oído según el estado. */
	NDE_API float GetHearingSensitivity(const UNDMonsterDefinition& Def, ENDMonsterState State);

	/** Parte del ruido que pasa por el ronquido en ese instante (1 = nada tapado). */
	NDE_API float GetSnoreMask(const UNDMonsterDefinition& Def, ENDMonsterState State, const FNDSnoreCycle& Cycle, double ServerTime, float BaseLoudness);

	/**
	 * Intensidad que llega a la criatura (GDD 9.2): base x distancia x paredes x ronquido.
	 * BaseLoudness en escala 0-100; Transmission 0-1 es lo que dejan pasar las puertas.
	 */
	NDE_API float GetPerceivedLoudness(const UNDMonsterDefinition& Def, float BaseLoudness, float DistanceCm, float Transmission, float SnoreMask);

	/** Sospecha que suma un ruido ya percibido: (Percibido / 10) ^ Exponente x Ganancia x Sensibilidad. */
	NDE_API float GetSuspicionGain(const UNDMonsterDefinition& Def, ENDMonsterState State, float Perceived);

	/** Sospecha que pierde por segundo. */
	NDE_API float GetDecayPerSecond(const UNDMonsterDefinition& Def, ENDMonsterState State);

	/**
	 * Estado en la cama que toca con esta sospecha, partiendo de Current (GDD 9.3, con histéresis).
	 * Devuelve WakingUp si llega al umbral de despertar. Current tiene que ser un estado de cama.
	 */
	NDE_API ENDMonsterState GetBedState(const UNDMonsterDefinition& Def, ENDMonsterState Current, float Suspicion);

	/** Nuevo ciclo de ronquido para ese estado (sin ronquido fuera de DeepSleep y LightSleep). */
	NDE_API FNDSnoreCycle MakeSnoreCycle(const UNDMonsterDefinition& Def, ENDMonsterState State, double StartServerTime, int32 CycleIndex, FRandomStream& Rng);

	/** Cuánto se llena el aviso de 'te veo' por segundo según visibilidad (0-1), distancia y alcance. */
	NDE_API float GetDetectionRate(const UNDMonsterDefinition& Def, float Visibility, float DistanceCm, float Range);

	/** Texto corto en español para depuración y el cartel sobre la cabeza. */
	NDE_API FString GetStateLabel(ENDMonsterState State);
}
