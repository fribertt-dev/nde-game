// Tarro de rescate (contrato C06): guarda un objeto bloqueado hasta que alguien lo abre.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NDRescueContainer.generated.h"

class ANDLootActor;
class UMaterialInterface;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Tarro de cristal con algo dentro (p. ej. Trasto en C06). Se abre quedándose a su lado unos segundos
 * (servidor); entonces el tarro desaparece y el objeto se puede coger. Placeholder greybox del motor.
 */
UCLASS()
class NDE_API ANDRescueContainer : public AActor
{
	GENERATED_BODY()

public:
	ANDRescueContainer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Rescue")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Rescue")
	TObjectPtr<UStaticMeshComponent> Jar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Rescue")
	TObjectPtr<UStaticMeshComponent> Lid;

	/** Zona donde hay que quedarse para abrirlo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Rescue")
	TObjectPtr<USphereComponent> OpenZone;

	/** Segundos que hay que pasar junto al tarro para abrirlo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Rescue", meta = (ClampMin = "0"))
	float OpenSeconds = 2.f;

	/** Solo servidor: guarda el objeto dentro, bloqueado. */
	void SetContainedLoot(ANDLootActor* InLoot);

	UFUNCTION(BlueprintPure, Category = "NDE|Rescue")
	bool IsOpened() const { return bOpened; }

	UFUNCTION(BlueprintPure, Category = "NDE|Rescue")
	ANDLootActor* GetContainedLoot() const { return ContainedLoot; }

	/** Solo servidor: abre el tarro y libera el objeto. */
	void Open();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	void CheckOpening();

	UFUNCTION()
	void OnRep_Opened();

	void ApplyOpenedVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_Opened)
	bool bOpened = false;

	UPROPERTY(Replicated)
	TObjectPtr<ANDLootActor> ContainedLoot;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	float OpenProgress = 0.f;
	FTimerHandle CheckTimer;
};
