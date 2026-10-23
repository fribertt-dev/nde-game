// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Monsters/NDMonsterTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "NDMonsterAIController.generated.h"

class ANDMonsterCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UNDMonsterDefinition;

/**
 * Cerebro de la criatura (solo servidor). Piensa ThinkRate veces por segundo (GDD 10.4: 5-10 Hz) con este
 * árbol de decisión, de más a menos prioritario:
 *
 *  1. Amanecer -> DawnHunt: despierto hasta el final, ronda la casa (GDD 7.7). Nunca vuelve a dormir.
 *  2. Manotazo en curso -> termina el aviso y golpea si sigue al alcance y a la vista (GDD 24.7).
 *  3. Te ve del todo (aviso de vista a 1) -> Chase. Si estaba en la cama, primero WakingUp.
 *  4. En la cama: la sospecha (ruido, tacto) decide DeepSleep / LightSleep / Stirring / HalfAwake con
 *     histéresis; al llegar a WakeThreshold se levanta a investigar. Sin sospecha, a ratos NightWalk.
 *  5. Despierto: un ruido o algo visto a medias -> Investigate a ese punto; al llegar mira alrededor y
 *     pasa a Search (salas cercanas); sin pistas durante GiveUpSeconds -> ReturnToSleep.
 *  6. Persiguiendo: te sigue mientras te ve (y ChaseMemorySeconds más); luego va a donde te vio por
 *     última vez y busca. Nunca sabe dónde estás sin verte u oírte (regla anti-injusticia, GDD 10.4).
 *
 * Oye por UAIPerceptionComponent (sentido Hearing): pasos, voz, golpes de botín y herramientas llegan
 * con ReportNoiseEvent o MakeNoise, Loudness 0-1 = escala 0-100 del GDD. La vista es propia (traza de
 * visibilidad y factor de visibilidad del personaje) para que los muebles y escondites tapen.
 */
UCLASS()
class NDE_API ANDMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANDMonsterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Ruido que llega a la criatura (0-100). Lo usan la percepción y los comandos de depuración. */
	void HearNoise(const FVector& Location, float BaseLoudness, AActor* NoiseInstigator, FName Tag);

	// --- Depuración.
	void DebugWake();
	void DebugSleep();
	void DebugSetSuspicion(float Value);
	void DebugNightWalk();
	FString GetDebugStatus() const;

	ANDMonsterCharacter* GetMonster() const { return Monster; }

protected:
	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void Think();

	const UNDMonsterDefinition& GetDef() const;
	double Now() const;

	// Pasos del árbol.
	void UpdateDawn();
	void ProcessNoises(float& OutGain);
	void UpdateTouch(float DeltaSeconds, float& InOutGain);
	void UpdateSight(float DeltaSeconds);
	void UpdateBed(float DeltaSeconds, float Gain);
	void UpdateWakingUp();
	void UpdateInvestigate();
	void UpdateSearch();
	void UpdateChase();
	void UpdateGrab();
	void UpdateReturnToSleep();
	void UpdateNightWalk();
	void UpdateDawnHunt();
	void UpdateSnore();
	void UpdateStuck();

	void EnterState(ENDMonsterState NewState);
	void StartInvestigate(const FVector& Location);
	void StartChase(APawn* Target);
	void StartSearch(const FVector& Around);
	void StartReturnToSleep();
	void LieDown();

	// Movimiento.
	void MoveToPoint(const FVector& Goal, float Acceptance = 120.f);
	void FollowActor(AActor* Goal);
	void HaltMovement();
	bool IsMoving() const { return bMoving; }
	void FaceTowards(const FVector& Location, float DeltaSeconds, float DegreesPerSecond = 160.f);
	void ApplySpeed();

	bool CanSee(const APawn* Target, float Range, float HalfAngle, const FVector& Gaze, float& OutDistance) const;
	bool HasLineOfSight(const APawn* Target) const;
	FVector GetEyeLocation() const;

	void DrawDebug() const;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(Transient)
	TObjectPtr<ANDMonsterCharacter> Monster;

	// Estado del cerebro (servidor).
	ENDMonsterState State = ENDMonsterState::DeepSleep;
	/** A qué estado pasa al terminar WakingUp. */
	ENDMonsterState AfterWakeState = ENDMonsterState::Investigate;
	double StateStartTime = 0.0;
	double LastThinkTime = 0.0;
	float Suspicion = 0.f;
	bool bDawn = false;

	TArray<FNDHeardNoise> PendingNoises;
	TArray<FNDHeardNoise> RecentNoises;
	FVector LastNoiseLocation = FVector::ZeroVector;
	double LastNoiseTime = -1000.0;
	/** Última vez que oyó o vio algo que merece seguir despierto. */
	double LastEvidenceTime = -1000.0;

	/** Aviso de vista de cada Desvelado (0-1). */
	TMap<TWeakObjectPtr<APawn>, float> Detection;
	TWeakObjectPtr<APawn> SeenThisTick;
	FVector SeenLocation = FVector::ZeroVector;

	TWeakObjectPtr<APawn> ChaseTarget;
	FVector LastSeenLocation = FVector::ZeroVector;
	double LastSeenTime = -1000.0;
	bool bChaseFollowingActor = false;

	FVector InvestigateLocation = FVector::ZeroVector;
	bool bLookingAround = false;
	double LookAroundStart = 0.0;
	float LookBaseYaw = 0.f;

	TArray<FVector> SearchPoints;
	int32 SearchIndex = 0;
	double PauseUntil = 0.0;

	TArray<FVector> WalkPoints;
	int32 WalkIndex = 0;
	double NextNightWalkTime = 0.0;
	int32 DawnRouteIndex = 0;

	TWeakObjectPtr<APawn> GrabTarget;
	double GrabStartTime = -1.0;
	double NextGrabTime = 0.0;

	// Movimiento.
	bool bMoving = false;
	bool bArrived = false;
	bool bMoveFailed = false;
	FAIRequestID CurrentMoveId;
	FVector MoveGoal = FVector::ZeroVector;
	FVector StuckCheckLocation = FVector::ZeroVector;
	double StuckCheckTime = 0.0;
	int32 BedFailures = 0;

	FRandomStream Rng;
	int32 SnoreIndex = 0;
	FTimerHandle ThinkTimer;
};
