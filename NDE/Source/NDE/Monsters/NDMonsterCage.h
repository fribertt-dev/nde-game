// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NDMonsterCage.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class APawn;

/**
 * Jaula donde la criatura encierra a quien atrapa (GDD 7.4). Un compañero libre que se queda a su lado
 * RescueSeconds la abre (con ruido medio) y salen todos. Mientras estás dentro puedes mirar, pero no
 * moverte ni coger nada. Placeholder con formas del motor; las paredes de colisión son invisibles.
 */
UCLASS()
class NDE_API ANDMonsterCage : public AActor
{
	GENERATED_BODY()

public:
	ANDMonsterCage();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Solo servidor: mete al Desvelado dentro. */
	void Imprison(APawn* Pawn);

	/** Solo servidor: abre y saca a todos (Rescuer hace el ruido; puede ser nullptr). */
	void ReleaseAll(APawn* Rescuer);

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	bool IsImprisoned(const APawn* Pawn) const;

	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	int32 GetPrisonerCount() const { return Prisoners.Num(); }

	/** 0-1. */
	UFUNCTION(BlueprintPure, Category = "NDE|Monster")
	float GetRescueProgress() const { return RescueProgress; }

	/** Radio (cm) desde el centro en el que un compañero abre la jaula. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Monster", meta = (ToolTip = "Distancia (cm) a la jaula a la que hay que quedarse para abrirla."))
	float RescueRadius = 200.f;

protected:
	void CheckRescue();
	FVector GetSlotLocation(int32 Index) const;
	FVector FindExitLocation(int32 Index) const;
	void UpdateLocalPrisoner();
	void UpdateLabel();

	UFUNCTION()
	void OnRep_Prisoners();

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TArray<TObjectPtr<UStaticMeshComponent>> Bars;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TArray<TObjectPtr<UBoxComponent>> Walls;

	UPROPERTY(VisibleAnywhere, Category = "NDE|Monster")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Prisoners)
	TArray<TObjectPtr<APawn>> Prisoners;

	UPROPERTY(Replicated)
	float RescueProgress = 0.f;

	FTimerHandle CheckTimer;
	bool bLocalInputBlocked = false;
	bool bLocalWasPrisoner = false;
};
