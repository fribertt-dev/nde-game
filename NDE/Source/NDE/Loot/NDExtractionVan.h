// Camioneta: punto de depósito autoritativo y zona de extracción (GDD 5.7 y 7.9).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NDExtractionVan.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ANDLootActor;
class APawn;

UCLASS(Blueprintable)
class NDE_API ANDExtractionVan : public AActor
{
	GENERATED_BODY()

public:
	ANDExtractionVan();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Van")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Van")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Volumen seguro: deposita botín y define quién está "en la camioneta". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Van")
	TObjectPtr<UBoxComponent> SafeZone;

	UFUNCTION(BlueprintPure, Category = "NDE|Van")
	bool IsPawnInside(const APawn* Pawn) const;

	UFUNCTION(BlueprintPure, Category = "NDE|Van")
	TArray<APawn*> GetOccupants() const;

	/** Servidor: deposita un objeto si la expedición lo admite. Exactamente una vez por LootGuid. */
	bool TryDeposit(ANDLootActor* Loot, APawn* Depositor);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Deposita lo que llevan los ocupantes (por si cogieron algo dentro). */
	void SweepOccupants();
	void DepositCarriedBy(APawn* Pawn);

	UPROPERTY(Replicated)
	TArray<TObjectPtr<APawn>> Occupants;

	FTimerHandle SweepTimer;
};
