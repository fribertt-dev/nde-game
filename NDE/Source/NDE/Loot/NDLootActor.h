// Objeto de botín en el mundo (GDD 8.1, 8.3 y 8.4).
// El servidor posee el estado; cada objeto tiene un LootGuid único y se deposita una sola vez.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDLootActor.generated.h"

class ANDLootActor;
class UNDLootDefinition;
class UStaticMeshComponent;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDLootConditionChangedSignature, ANDLootActor*, Loot, ENDLootCondition, NewCondition);

UCLASS(Blueprintable)
class NDE_API ANDLootActor : public AActor
{
	GENERATED_BODY()

public:
	ANDLootActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	UNDLootDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	const FGuid& GetLootGuid() const { return LootGuid; }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	ENDLootState GetLootState() const { return LootState; }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	ENDLootCondition GetCondition() const { return Condition; }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	int32 GetCarrierCount() const { return Carriers.Num(); }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	APawn* GetLeadCarrier() const { return Carriers.Num() > 0 ? Carriers[0].Get() : nullptr; }

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	bool IsCarriedBy(const APawn* Pawn) const;

	/** Portadores efectivos: jugadores más un carrito si alguno lo lleva y el objeto lo admite. */
	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	int32 GetEffectiveCarrierCount() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	bool HasEnoughCarriers() const;

	/** Valor en puntos según su estado actual. */
	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	int32 GetCurrentLootPoints() const;

	// --- Solo servidor. Las llama UNDCarryComponent y ANDExtractionVan. ---

	/**
	 * Para objetos que aparecen al generar la casa: definición y LootGuid estable (derivado de la semilla
	 * y del hueco de botín). Llamar antes de FinishSpawning.
	 */
	void InitializeSpawn(UNDLootDefinition* InDefinition, const FGuid& InStableGuid);

	bool CanBeGrabbedBy(const APawn* Pawn) const;
	bool AddCarrier(APawn* Pawn);
	void RemoveCarrier(APawn* Pawn, const FVector& ReleaseVelocity);
	void SetPocketed(APawn* Pawn);
	void DropFromPocket(const FVector& Location);
	void MarkDeposited();

	/** Aplica un impacto (cambio de velocidad en uu/s) con histéresis. */
	void ApplyImpact(float DeltaSpeed, const FVector& Location);

	UPROPERTY(BlueprintAssignable, Category = "NDE|Loot")
	FNDLootConditionChangedSignature OnConditionChanged;

	/** Distancia máxima entre portadores de un bulto compartido antes de que se suelte. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Loot")
	float MaxCoCarryDistance = 260.f;

	/** Tiempo mínimo entre dos impactos que cuenten (evita el temblor de físicas). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Loot")
	float ImpactCooldownSeconds = 0.25f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Definition, BlueprintReadOnly, Category = "NDE|Loot")
	TObjectPtr<UNDLootDefinition> Definition;

	UPROPERTY(Replicated)
	FGuid LootGuid;

	UPROPERTY(ReplicatedUsing = OnRep_LootState)
	ENDLootState LootState = ENDLootState::WorldLoose;

	UPROPERTY(ReplicatedUsing = OnRep_Condition)
	ENDLootCondition Condition = ENDLootCondition::Intact;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<APawn>> Carriers;

	UFUNCTION()
	void OnRep_Definition();

	UFUNCTION()
	void OnRep_LootState();

	UFUNCTION()
	void OnRep_Condition();

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ApplyDefinitionVisuals();
	void ApplyStateLocally();
	void AttachToLeader();
	void ReleaseToWorld(const FVector& ReleaseVelocity);
	void SetCondition(ENDLootCondition NewCondition);
	FVector GetCarryOffset() const;

	/** Jugador que lo lleva en la mochila (solo servidor). */
	TWeakObjectPtr<APawn> PocketOwner;

	FVector LastSafeLocation = FVector::ZeroVector;
	double LastImpactTime = -1000.0;
};
