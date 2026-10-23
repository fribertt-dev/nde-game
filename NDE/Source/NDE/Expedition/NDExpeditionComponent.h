// Reglas de una expedición (GDD 7.1 a 7.9): fases, participantes, depósitos,
// votación de escape, evacuación al amanecer, resultado y liquidación única.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDExpeditionComponent.generated.h"

class ANDExtractionVan;
class ANDLootActor;
class APawn;
class APlayerState;
class UNDContractDefinition;
class UNDNightClockComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDExpeditionPhaseChangedSignature, ENDExpeditionPhase, NewPhase, ENDExpeditionPhase, OldPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDExpeditionSettledSignature, const FNDRunSummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNDExpeditionStateChangedSignature);

UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDExpeditionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDExpeditionComponent();

	// --- Configuración ---

	/** Contrato de la ronda. En el corte gris se elige en el nivel; luego lo inyectará la sesión. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Contract, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<UNDContractDefinition> Contract;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	FNDEconomyTuning Economy;

	/** Arranca sola al cargar el nivel (útil para el nivel de prueba). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition", meta = (ClampMin = 0))
	float AutoStartDelaySeconds = 2.f;

	/** Inserción: el reloj empieza al entrar el primero en la casa o al pasar este tiempo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition", meta = (ClampMin = 0))
	float InsertionSeconds = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition", meta = (ClampMin = 1))
	float EscapeVoteSeconds = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition", meta = (ClampMin = 0))
	float EvacuationCountdownSeconds = 5.f;

	/** Opción de salas privadas (GDD 7.3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	bool bRequireUnanimity = false;

	/** Quien entra en los primeros segundos de la noche se suma como participante (GDD 4.3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition", meta = (ClampMin = 0))
	float LateJoinWindowSeconds = 90.f;

	/** Al terminar la ventana del amanecer, quien esté dentro de la camioneta escapa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	bool bVanOccupantsEscapeAtDayEnd = true;

	// --- API de servidor ---

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void SetContract(UNDContractDefinition* InContract);

	/** La sesión de red inyecta aquí su RunGuid. Si nadie lo hace se genera uno. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void SetRunGuid(const FGuid& InRunGuid);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void StartExpedition();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void NotifyPlayerEnteredHouse();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void RegisterParticipant(APlayerState* PlayerState);

	/** Captura (true) o rescate (false). Lo llamarán el gigante y la jaula (GDD 7.4). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void SetParticipantCaptured(APlayerState* PlayerState, bool bCaptured);

	/** Cierre forzado para desarrollo o caída de sesión. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Expedition")
	void AbortExpedition();

	/** Interacción con la camioneta: inicia votación, vota sí o evacúa al amanecer, según el momento. */
	void HandleVanInteraction(APlayerState* PlayerState);
	void RequestEscapeVote(APlayerState* PlayerState);
	void CastEscapeVote(APlayerState* PlayerState, bool bYes);
	void RequestDawnEvacuation(APlayerState* PlayerState);

	bool RecordDeposit(ANDLootActor* Loot, APawn* Depositor);
	void NotifyLootBroken(ANDLootActor* Loot);
	void RegisterVan(ANDExtractionVan* InVan);

	// --- Consultas ---

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	ENDExpeditionPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool HasStarted() const { return bStarted; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool CanPlayerAct(const APawn* Pawn) const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool CanAcceptDeposits() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	const TArray<FNDParticipant>& GetParticipants() const { return Participants; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	ENDParticipantStatus GetParticipantStatus(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	const TArray<FNDDepositRecord>& GetDeposits() const { return Deposits; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 GetLootPoints() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 GetQuota() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool IsObjectiveMet() const { return bObjectiveMet; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	float GetInsertionSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool IsEscapeVoteActive() const { return bVoteActive; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	float GetEscapeVoteSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 GetVotesYes() const { return VotersYes.Num(); }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 GetVotesNo() const { return VotersNo.Num(); }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	bool IsEvacuationActive() const { return bEvacuationActive; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	float GetEvacuationSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	const FNDRunSummary& GetRunSummary() const { return RunSummary; }

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition")
	int32 CountParticipantsWithStatus(ENDParticipantStatus Status) const;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDExpeditionPhaseChangedSignature OnPhaseChanged;

	/** Se emite una sola vez, en servidor y clientes, con el resumen final. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDExpeditionSettledSignature OnExpeditionSettled;

	/** Cambió algo visible: depósitos, participantes, votación o evacuación. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Expedition")
	FNDExpeditionStateChangedSignature OnStateChanged;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UNDNightClockComponent* GetNightClock() const;
	ANDExtractionVan* GetVan() const;
	double GetServerNow() const;
	FNDParticipant* FindParticipant(const APlayerState* PlayerState);
	bool IsActiveParticipant(const APlayerState* PlayerState) const;
	bool IsInVan(const APlayerState* PlayerState) const;

	void SetPhase(ENDExpeditionPhase NewPhase);
	void BeginInfiltration();
	void EnterDawnGrace();
	void RefreshParticipants();
	void UpdateDisconnections();
	void EvaluateObjective();
	bool ComputeSecondaryMet() const;

	void ResolveEscapeVote(bool bTimedOut);
	void EndEscapeVote();
	void StartEvacuation();
	void CompleteEvacuation();
	void FinishExpedition(ENDExpeditionEndReason Reason);
	void BroadcastStateChanged();
	void LogRunSummary() const;

	UFUNCTION()
	void HandleNightPhaseChanged(ENDNightPhase NewPhase, ENDNightPhase OldPhase);

	UFUNCTION()
	void OnRep_Phase(ENDExpeditionPhase OldPhase);

	UFUNCTION()
	void OnRep_Contract();

	UFUNCTION()
	void OnRep_VisibleState();

	UFUNCTION()
	void OnRep_RunSummary();

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ENDExpeditionPhase Phase = ENDExpeditionPhase::Insertion;

	UPROPERTY(Replicated)
	bool bStarted = false;
	/** StartExpedition llegó antes que el contrato: arranca al recibirlo (SetContract). */
	bool bStartPending = false;

	UPROPERTY(Replicated)
	FGuid RunGuid;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	TArray<FNDParticipant> Participants;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	TArray<FNDDepositRecord> Deposits;

	UPROPERTY(Replicated)
	bool bObjectiveMet = false;

	UPROPERTY(Replicated)
	double InsertionEndServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	bool bVoteActive = false;

	UPROPERTY(Replicated)
	double VoteEndServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	TArray<TObjectPtr<APlayerState>> VotersYes;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	TArray<TObjectPtr<APlayerState>> VotersNo;

	UPROPERTY(ReplicatedUsing = OnRep_VisibleState)
	bool bEvacuationActive = false;

	UPROPERTY(Replicated)
	double EvacuationEndServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_RunSummary)
	FNDRunSummary RunSummary;

	UPROPERTY(Transient)
	TObjectPtr<ANDExtractionVan> Van;

	/** Libro de depósitos: garantiza un único depósito por LootGuid. Solo servidor. */
	TSet<FGuid> DepositedGuids;

	int32 BrokenFragileCount = 0;
	int32 DawnCaptures = 0;
	double InfiltrationStartServerTime = 0.0;
	bool bSettled = false;
	bool bSettledBroadcastLocally = false;
	FTimerHandle AutoStartTimer;
};
