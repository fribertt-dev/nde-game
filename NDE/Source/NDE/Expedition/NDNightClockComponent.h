// Reloj nocturno autoritativo (GDD 7.1, 7.6 y 24.9).
// El servidor replica solo la marca de inicio y las duraciones; cada máquina calcula
// NightAlpha con el reloj del servidor, sin depender de la hora local ni de los FPS.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDNightClockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDNightPhaseChangedSignature, ENDNightPhase, NewPhase, ENDNightPhase, OldPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDPreDawnWarningSignature, int32, SecondsUntilDawn);

UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDNightClockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDNightClockComponent();

	/** Arranca la noche en el servidor. Duraciones fijas durante toda la ronda (GDD 7.1). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Night")
	void StartNight(float InNightDurationSeconds, float InDawnGraceSeconds, bool bInDawnEnabled);

	/** Congela el reloj (fin de la expedición). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Night")
	void StopClock();

	/** Depuración: adelanta el reloj estos segundos (solo servidor). */
	void DebugSkipSeconds(float Seconds);

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	bool IsRunning() const { return bRunning; }

	/** clamp((ServerNow - NightStart) / NightDuration, 0, 1). */
	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	float GetNightAlpha() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	float GetSecondsUntilDawn() const;

	/** Segundos que quedan de la ventana final. Antes del amanecer devuelve la ventana completa. */
	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	float GetGraceSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	ENDNightPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	bool IsDawnTriggered() const { return Phase == ENDNightPhase::DawnGrace || Phase == ENDNightPhase::DayEnded; }

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	float GetNightDurationSeconds() const { return NightDurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "NDE|Night")
	float GetDawnGraceSeconds() const { return DawnGraceSeconds; }

	/** Fase para un tiempo transcurrido dado. Pura, usada por el tick y por los tests. */
	static ENDNightPhase ComputePhase(double ElapsedSeconds, float NightDuration, float Grace, float MadrugadaStartAlpha, float PreDawnStartAlpha, bool bDawnEnabled);

	/** Se emite en servidor y clientes cuando cambia la fase. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Night")
	FNDNightPhaseChangedSignature OnNightPhaseChanged;

	/** Avisos locales de 120, 60, 30 y 10 s antes del amanecer (GDD 7.6, fase C). */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Night")
	FNDPreDawnWarningSignature OnPreDawnWarning;

	/** Inicio de la madrugada (fase B). CONFIGURABLE. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Night", meta = (ClampMin = 0, ClampMax = 1))
	float MadrugadaStartAlpha = 0.5f;

	/** Inicio del pre-amanecer (fase C). CONFIGURABLE. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Night", meta = (ClampMin = 0, ClampMax = 1))
	float PreDawnStartAlpha = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Night")
	TArray<int32> PreDawnWarningSeconds = { 120, 60, 30, 10 };

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	double GetServerNow() const;
	double GetElapsedSeconds() const;
	void SetPhase(ENDNightPhase NewPhase);
	void UpdateWarnings();

	UFUNCTION()
	void OnRep_Phase(ENDNightPhase OldPhase);

	UPROPERTY(Replicated)
	double NightStartServerTime = 0.0;

	UPROPERTY(Replicated)
	double StoppedElapsedSeconds = -1.0;

	UPROPERTY(Replicated)
	float NightDurationSeconds = 600.f;

	UPROPERTY(Replicated)
	float DawnGraceSeconds = 45.f;

	UPROPERTY(Replicated)
	bool bDawnEnabled = true;

	UPROPERTY(Replicated)
	bool bRunning = false;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ENDNightPhase Phase = ENDNightPhase::NotStarted;

	/** Avisos ya emitidos en esta máquina. */
	TSet<int32> FiredWarnings;
	bool bWarningsPrimed = false;
};
