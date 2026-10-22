// Desplegables de consumibles: espuma silenciosa (T17) y ratón de cuerda (T18).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NDToolDeployables.generated.h"

class UStaticMeshComponent;

/** T17: amortigua el ruido de golpes y pasos sobre su superficie durante un tiempo. */
UCLASS(Blueprintable)
class NDE_API ANDSilentFoamPatch : public AActor
{
	GENERATED_BODY()

public:
	ANDSilentFoamPatch();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0))
	float Radius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0, ClampMax = 1))
	float NoiseMultiplier = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 1))
	float DurationSeconds = 30.f;

	/** Multiplicador de ruido en un punto: 1 si no hay espuma. Para el sistema de sigilo. */
	UFUNCTION(BlueprintPure, Category = "NDE|Tools", meta = (WorldContext = "WorldContextObject"))
	static float GetNoiseMultiplierAt(const UObject* WorldContextObject, FVector Location);

	virtual void BeginPlay() override;
};

/** T18: señuelo que avanza haciendo ruido y puede volver hacia el jugador. */
UCLASS(Blueprintable)
class NDE_API ANDWindUpMouse : public AActor
{
	GENERATED_BODY()

public:
	ANDWindUpMouse();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0))
	float Speed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0.1))
	float NoiseInterval = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0, ClampMax = 1))
	float NoiseLoudness = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 1))
	float DurationSeconds = 8.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	float NoiseAccumulator = 0.f;
	int32 Bounces = 0;
};
