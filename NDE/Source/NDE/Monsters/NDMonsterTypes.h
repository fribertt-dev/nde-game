// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "NDMonsterTypes.generated.h"

class AActor;
class APawn;

/**
 * Estado de la criatura gigante (GDD 9.3). Los cuatro primeros son de sueño y dependen de la
 * sospecha; los demás son de vigilia y los decide el árbol de decisión del controlador.
 */
UENUM(BlueprintType)
enum class ENDMonsterState : uint8
{
	DeepSleep UMETA(ToolTip = "Sueño profundo: ronquidos amplios, oído poco sensible."),
	LightSleep UMETA(ToolTip = "Sueño ligero: ronquido corto, oye más."),
	Stirring UMETA(ToolTip = "Se revuelve: cambia de postura y murmura. No ronca."),
	HalfAwake UMETA(ToolTip = "Medio despierto: abre un ojo y mira hacia el último ruido."),
	WakingUp UMETA(ToolTip = "Se levanta de la cama (aviso antes de moverse)."),
	Investigate UMETA(ToolTip = "Despierto: va a mirar el sitio del ruido o de lo que vio."),
	Search UMETA(ToolTip = "Despierto: busca por las salas cercanas sin saber dónde estás."),
	Chase UMETA(ToolTip = "Despierto: te ha visto y te persigue."),
	ReturnToSleep UMETA(ToolTip = "No encontró nada: vuelve a la cama."),
	NightWalk UMETA(ToolTip = "Rutina: se levanta medio dormido y da una vuelta por la casa."),
	DawnHunt UMETA(ToolTip = "Amanecer: se despierta del todo y ronda la casa hasta el final (GDD 7.7).")
};

/** Qué le pasa a un Desvelado que ha tocado la criatura. */
UENUM(BlueprintType)
enum class ENDMonsterVictimState : uint8
{
	None UMETA(ToolTip = "Libre."),
	Dazed UMETA(ToolTip = "Aturdido unos segundos tras el primer manotazo: no se puede mover."),
	Captured UMETA(ToolTip = "Atrapado en la jaula hasta que un compañero lo rescate.")
};

/**
 * Ciclo de ronquido replicado (GDD 9.4 y 24.3). Todas las máquinas calculan la fase con la hora del
 * servidor, sin RPC: inspiración (el pecho se hincha, aviso) -> ronquido (ventana que tapa ruidos) -> pausa.
 */
USTRUCT(BlueprintType)
struct FNDSnoreCycle
{
	GENERATED_BODY()

	/** Hora del servidor en que empieza el ciclo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	double StartServerTime = 0.0;

	/** Segundos de inspiración antes del ronquido. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	float InhaleSeconds = 1.f;

	/** Segundos que dura el ronquido (ventana de máscara). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	float SnoreSeconds = 2.f;

	/** Duración total del ciclo, con la pausa. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	float PeriodSeconds = 0.f;

	/** Fuerza del ronquido (0 = no ronca). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	float Amplitude = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	int32 CycleIndex = 0;

	bool IsActive() const { return Amplitude > 0.f && PeriodSeconds > 0.f; }

	/** Segundos desde el inicio del ciclo (puede pasar de PeriodSeconds si el servidor aún no mandó el siguiente). */
	float GetElapsed(double ServerTime) const { return static_cast<float>(ServerTime - StartServerTime); }

	/** true si en ese momento suena el ronquido fuerte. */
	bool IsInMaskWindow(double ServerTime) const
	{
		const float T = GetElapsed(ServerTime);
		return IsActive() && T >= InhaleSeconds && T < InhaleSeconds + SnoreSeconds;
	}

	/** 0-1: cuánto suena ahora (sube al inspirar, fuerte al roncar, cae en la pausa). Sirve para sonido y pecho. */
	float GetLoudness(double ServerTime) const;

	/** Segundos hasta la próxima ventana de ronquido (0 si estamos dentro; -1 si no ronca). */
	float GetSecondsToNextWindow(double ServerTime) const;
};

/** Un Desvelado aturdido por la criatura (replicado para que su máquina bloquee el movimiento). */
USTRUCT(BlueprintType)
struct FNDMonsterVictim
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<APawn> Pawn;

	/** Hora del servidor en que se le pasa el aturdimiento. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	double DazedUntilServerTime = 0.0;
};

/** Ruido que ha llegado a la criatura, ya calculado (para depuración y la lógica). */
struct FNDHeardNoise
{
	FVector Location = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Instigator;
	/** Intensidad base 0-100 (GDD 9.2). */
	float BaseLoudness = 0.f;
	/** Intensidad que llega a la criatura, tras distancia, paredes y ronquido. */
	float Perceived = 0.f;
	/** Sospecha que añade. */
	float SuspicionGain = 0.f;
	FName Tag;
	double ServerTime = 0.0;
};
