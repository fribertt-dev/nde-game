// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Monsters/NDMonsterTypes.h"
#include "NDMonsterDefinition.generated.h"

/**
 * Datos de un dueño de la casa (GDD 10.1: un Data Asset por monstruo, no un Blueprint por cada uno).
 * Los valores por defecto son los de M01, El Dormilón, así que la criatura funciona sin crear ningún asset.
 * Todos los números son provisionales hasta probarlos jugando.
 */
UCLASS(BlueprintType)
class NDE_API UNDMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Id del monstruo (M01...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ToolTip = "Id del monstruo del GDD (M01 a M08)."))
	FName MonsterId = TEXT("M01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ToolTip = "Nombre que ve el jugador."))
	FText DisplayName = NSLOCTEXT("NDE", "MonsterM01", "El Dormilón");

	// --- Sospecha (GDD 9.3). Umbrales de 0 a 100; al bajar se usa el umbral menos la histéresis.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Sospecha a partir de la que pasa de sueño profundo a sueño ligero."))
	float LightSleepThreshold = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Sospecha a partir de la que se revuelve en la cama (deja de roncar)."))
	float StirringThreshold = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Sospecha a partir de la que abre un ojo y mira hacia el ruido (ya puede verte)."))
	float HalfAwakeThreshold = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Sospecha a partir de la que se levanta a investigar."))
	float WakeThreshold = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ClampMax = "30", ToolTip = "Puntos por debajo del umbral que hay que bajar para volver al estado anterior (evita que oscile)."))
	float Hysteresis = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ToolTip = "Sospecha que se pierde por segundo en sueño profundo."))
	float DecayDeepSleep = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ToolTip = "Sospecha que se pierde por segundo en sueño ligero."))
	float DecayLightSleep = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ToolTip = "Sospecha que se pierde por segundo al revolverse o con un ojo abierto."))
	float DecayStirring = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspicion", meta = (ClampMin = "0", ToolTip = "Sospecha que se pierde por segundo despierto (si no oye ni ve nada)."))
	float DecayAwake = 1.f;

	// --- Oído (GDD 9.2). Sospecha por ruido = (Percibido / 10) ^ Exponente x Ganancia x Sensibilidad del estado.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Multiplica la sospecha que da cada ruido."))
	float HearingGain = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "1", ClampMax = "3", ToolTip = "Curva del ruido: con más de 1, un golpe fuerte pesa mucho más que muchos pasos suaves."))
	float HearingExponent = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "100", ToolTip = "Distancia (cm) a la que un ruido llega a la mitad de fuerte."))
	float HearingHalfDistance = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "100", ToolTip = "Más lejos de esto (cm) no oye nada."))
	float HearingMaxDistance = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Sensibilidad del oído en sueño profundo."))
	float HearingDeepSleep = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Sensibilidad del oído en sueño ligero."))
	float HearingLightSleep = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Sensibilidad del oído al revolverse."))
	float HearingStirring = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Sensibilidad del oído con un ojo abierto o despierto."))
	float HearingAwake = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Despierto: sospecha mínima que tiene que dar un ruido para que vaya a mirarlo."))
	float AwakeInvestigateGain = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Golpes del mismo jugador en menos de esto (s) cuentan como uno solo, el más fuerte (GDD 9.2)."))
	float NoiseMergeSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Dormido: un Desvelado a menos de esta distancia (cm) lo nota aunque no haga ruido (tacto, GDD 9.1)."))
	float TouchRadius = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0", ToolTip = "Sospecha por segundo de cada Desvelado pegado a la criatura dormida."))
	float TouchSuspicionPerSecond = 6.f;

	// --- Ronquidos (GDD 9.4 y 10.3 M01).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "1", ToolTip = "Duración mínima de cada ciclo de ronquido (s)."))
	float SnorePeriodMin = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "1", ToolTip = "Duración máxima de cada ciclo de ronquido (s)."))
	float SnorePeriodMax = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0.2", ToolTip = "Inspiración antes de roncar (s): el pecho se hincha; es el aviso."))
	float SnoreInhaleSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0.5", ToolTip = "Ronquido mínimo en sueño profundo (s): ventana que tapa tus ruidos."))
	float SnoreWindowMin = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0.5", ToolTip = "Ronquido máximo en sueño profundo (s)."))
	float SnoreWindowMax = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Parte del ruido que pasa durante el ronquido en sueño profundo (0,3 = solo el 30 %)."))
	float SnoreMaskDeepSleep = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Parte del ruido que pasa durante el ronquido en sueño ligero (más corto y flojo)."))
	float SnoreMaskLightSleep = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snore", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Ruidos de esta intensidad o más casi no se tapan (nunca hay silencio total, GDD 9.4)."))
	float SnoreLoudNoise = 50.f;

	// --- Vista (GDD 9.5).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ToolTip = "Alcance de la vista despierto (cm)."))
	float SightRange = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "180", ToolTip = "Medio ángulo de visión despierto (grados)."))
	float SightHalfAngle = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ToolTip = "Alcance de la vista con un ojo abierto en la cama (cm)."))
	float HalfAwakeSightRange = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "180", ToolTip = "Medio ángulo con un ojo abierto (grados)."))
	float HalfAwakeSightHalfAngle = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Medio dormido de ronda (NightWalk) ve peor: multiplica el alcance."))
	float NightWalkSightScale = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ToolTip = "Cuánto se llena por segundo el aviso de 'te veo' con visibilidad total y de cerca. Al llegar a 1 te persigue."))
	float DetectionRate = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ToolTip = "Cuánto se vacía por segundo el aviso de 'te veo' cuando no te ve."))
	float DetectionDecay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Con el aviso por encima de esto se gira y va a mirar (todavía sin perseguir)."))
	float DetectionInvestigate = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Visibilidad de un Desvelado agachado si su personaje no la da (0-1)."))
	float CrouchedVisibility = 0.45f;

	// --- Movimiento.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ToolTip = "Velocidad de ronda y al volver a la cama (cm/s)."))
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ToolTip = "Velocidad al ir a mirar un ruido (cm/s)."))
	float InvestigateSpeed = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ToolTip = "Velocidad de persecución (cm/s). El Desvelado corre a 470: M01 persigue despacio."))
	float ChaseSpeed = 390.f;

	// --- Decisiones (GDD 10.4 y 24.3).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ToolTip = "Aviso al despertarse: se incorpora y bosteza antes de andar (s)."))
	float WakeUpSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ToolTip = "Segundos mirando alrededor al llegar al sitio del ruido."))
	float LookAroundSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ToolTip = "Sitios cercanos que revisa al buscar."))
	int32 SearchPoints = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "5", ToolTip = "Sin oír ni ver nada durante esto (s), vuelve a la cama (GDD 24.3: 20-40 s)."))
	float GiveUpSeconds = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ToolTip = "Al perderte de vista te sigue de verdad solo este tiempo (s); luego va a la última posición vista."))
	float ChaseMemorySeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ToolTip = "Sin verte durante esto (s) deja de perseguir y te busca."))
	float LoseTargetSeconds = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decisions", meta = (ClampMin = "0", ClampMax = "100", ToolTip = "Sospecha con la que se acuesta de nuevo (sueño ligero)."))
	float BackToBedSuspicion = 30.f;

	// --- Rutina nocturna (patrulla por la casa generada).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ToolTip = "Si se levanta a veces a dar una vuelta medio dormido."))
	bool bNightWalks = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ClampMin = "10", ToolTip = "Tiempo mínimo durmiendo tranquilo antes de una ronda (s)."))
	float NightWalkIntervalMin = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ClampMin = "10", ToolTip = "Tiempo máximo durmiendo tranquilo antes de una ronda (s)."))
	float NightWalkIntervalMax = 130.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ClampMin = "1", ToolTip = "Salas mínimas que visita en una ronda."))
	int32 NightWalkRoomsMin = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ClampMin = "1", ToolTip = "Salas máximas que visita en una ronda."))
	int32 NightWalkRoomsMax = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routine", meta = (ClampMin = "0", ToolTip = "Segundos que se queda en cada sala de la ronda."))
	float NightWalkPauseSeconds = 2.5f;

	// --- Golpe y captura (GDD 7.4 y 24.7).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "50", ToolTip = "Alcance de la mano (cm, en planta)."))
	float GrabRange = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0.5", ToolTip = "Aviso antes del manotazo (s). El GDD pide al menos 0,5."))
	float GrabTelegraphSeconds = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0", ToolTip = "Tras un manotazo no vuelve a intentarlo durante esto (s)."))
	float GrabCooldownSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0", ToolTip = "Primer manotazo: segundos aturdido sin poder moverse (GDD 7.4: 2-3 s)."))
	float DazedSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0", ToolTip = "Empujón del primer manotazo (cm/s)."))
	float DazedKnockback = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0", ToolTip = "Tras aturdir a alguien le deja escapar durante esto (s) antes de poder atraparlo."))
	float DazedGraceSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "0", ToolTip = "El primer manotazo se olvida tras esto (s) sin otro golpe; entonces el siguiente vuelve a aturdir."))
	float StrikeMemorySeconds = 60.f;
};
