// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Monsters/NDMonsterTypes.h"
#include "NDMonsterCharacter.generated.h"

class UAudioComponent;
class UMaterialInterface;
class UNDMonsterDefinition;
class UNDSnoreSoundWave;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDOnMonsterStateChanged, ENDMonsterState, NewState, ENDMonsterState, OldState);

/**
 * Criatura gigante, dueña de la casa (GDD 10.1). El cuerpo y lo que ven los jugadores: postura (tumbada o
 * de pie), ronquido sincronizado, ojos, mano que avisa del manotazo y un cartel con su estado. La decisión
 * (qué oye, qué ve y qué hace) vive en ANDMonsterAIController, solo en el servidor.
 * El cuerpo es provisional con formas del motor hasta que tenga su skeletal mesh (hereda en un Blueprint y
 * oculta VisualRoot). Mide unos 4,2 m: cabe por las puertas de 160 x 480 cm.
 */
UCLASS()
class NDE_API ANDMonsterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANDMonsterCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Datos del monstruo. Sin asset usa los valores por defecto (M01, El Dormilón). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Data Asset del monstruo. Vacío = El Dormilón (M01) con los valores por defecto."))
	TObjectPtr<UNDMonsterDefinition> Definition;

	/** Ángulo del cuerpo tumbado en la cama: 90 = cabeza hacia atrás de la cama, -90 = hacia delante. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Giro del cuerpo provisional al dormir (90 o -90 según hacia dónde quede el cabecero)."))
	float SleepPitch = 90.f;

	/** Altura del cuerpo tumbado sobre el suelo (cm): la de la cama. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Altura (cm) a la que queda el cuerpo provisional tumbado: la del colchón."))
	float SleepBodyHeight = 150.f;

	/** Sonido de ronquido sintetizado (sin assets). Desactívalo cuando haya sonidos reales. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Ronquido sintetizado mientras no haya sonidos de verdad."))
	bool bSynthSnore = true;

	/** Muestra encima de la cabeza el estado (Zzz, ?, !). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Cartel sobre la cabeza con el estado (Zzz, ?, !)."))
	bool bShowStateText = true;

	/** Cada máquina, al cambiar de estado. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Monster")
	FNDOnMonsterStateChanged OnMonsterStateChanged;

	// --- Consultas (todas las máquinas).

	const UNDMonsterDefinition* GetDefinition() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	ENDMonsterState GetMonsterState() const { return MonsterState; }

	/** Sospecha 0-100. */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	float GetSuspicion() const { return Suspicion; }

	/** 0-1: cuánto le falta para verte del todo (lo más alto entre los jugadores). */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	float GetSightAlert() const { return SightAlert; }

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	FNDSnoreCycle GetSnoreCycle() const { return SnoreCycle; }

	/** true mientras suena el ronquido fuerte (tus ruidos se oyen menos). */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	bool IsSnoreMaskActive() const;

	/** Segundos hasta el próximo ronquido fuerte (0 = ahora; -1 = no ronca). Para el Roncómetro. */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	float GetSecondsToNextSnore() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	bool IsInBed() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	bool IsGrabTelegraphing() const { return bGrabTelegraph; }

	/** Estado de un Desvelado frente a esta criatura (aturdido o libre). La jaula lleva los capturados. */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	bool IsPawnDazed(const APawn* Pawn) const;

	/** Dónde duerme (la cama). */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	FTransform GetBedTransform() const { return BedTransform; }

	/**
	 * 0-1: riesgo de despertarle con un ruido de esta fuerza (0-100, andar = 10) en Location. Aproximado,
	 * para la interfaz (oreja del HUD). En el servidor y en los clientes.
	 */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	float GetNoiseRiskAt(FVector Location, float BaseLoudness = 10.f) const;

	double GetServerTime() const;

	// --- Solo servidor (lo usa el controlador).

	void SetMonsterState(ENDMonsterState NewState);
	void SetSuspicion(float NewSuspicion);
	void SetSightAlert(float NewAlert);
	void SetSnoreCycle(const FNDSnoreCycle& NewCycle);
	void SetGrabTelegraph(bool bNewTelegraph);
	void SetBedTransform(const FTransform& InBed) { BedTransform = InBed; }
	void AddDazedVictim(APawn* Pawn, double UntilServerTime);
	/** Quita los aturdimientos que ya pasaron. */
	void PruneVictims();

protected:
	UFUNCTION()
	void OnRep_MonsterState(ENDMonsterState OldState);

	UFUNCTION()
	void OnRep_Victims();

	void BuildPlaceholderBody();
	UStaticMeshComponent* AddShape(FName Name, UStaticMesh* ShapeMesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
	void PaintShapes();

	/** Postura, pecho, ojos, mano, cartel y sonido. Cosmético, en cada máquina. */
	void UpdateVisuals(float DeltaSeconds);
	void UpdateStateText();
	/** Bloquea o devuelve el movimiento del jugador local si esta criatura le ha aturdido. */
	void UpdateLocalVictim();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> BellyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> NoseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> CapMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> EyeLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> EyeRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> HandLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> HandRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UTextRenderComponent> StateText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Monster")
	TObjectPtr<UAudioComponent> SnoreAudio;

	UPROPERTY(Transient)
	TObjectPtr<UNDSnoreSoundWave> SnoreWave;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_MonsterState)
	ENDMonsterState MonsterState = ENDMonsterState::DeepSleep;

	UPROPERTY(Replicated)
	float Suspicion = 0.f;

	UPROPERTY(Replicated)
	float SightAlert = 0.f;

	UPROPERTY(Replicated)
	FNDSnoreCycle SnoreCycle;

	UPROPERTY(Replicated)
	bool bGrabTelegraph = false;

	UPROPERTY(Replicated)
	FTransform BedTransform;

	UPROPERTY(ReplicatedUsing = OnRep_Victims)
	TArray<FNDMonsterVictim> Victims;

	/** 0 de pie, 1 tumbado (se interpola). */
	float LyingAlpha = 1.f;
	float StirTime = 0.f;
	bool bLocalInputBlocked = false;
	FVector RightHandRest = FVector::ZeroVector;
};
