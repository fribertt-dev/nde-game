// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NDNoiseEmitterComponent.generated.h"

class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNDOnNoiseEmitted, AActor*, Instigator, FVector, Location, float, Loudness, FName, Tag);

/**
 * Ruido del cuerpo del Desvelado (GDD 9.2): pasos, aterrizajes y lo que pidan otros sistemas.
 * Solo el servidor emite, con UAISense_Hearing (la criatura lo oye con su AIPerception).
 * Loudness va de 0 a 1 = escala del GDD (0-100) / 100: agachado 0.04, andar 0.10, correr 0.22.
 * Escondido no hace ruido y la espuma silenciosa (T17) lo amortigua.
 */
UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDNoiseEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDNoiseEmitterComponent();

	static const FName FootstepTag;
	static const FName LandTag;
	static const FName ActionTag;

	/** Solo servidor: emite un ruido (0-1) en un punto. Aplica la espuma T17. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Noise")
	void EmitNoise(FVector Location, float Loudness, FName Tag);

	/** Último ruido emitido (0-1), para depuración y la UI. Se apaga solo. */
	UFUNCTION(BlueprintPure, Category = "NDE|Noise")
	float GetRecentLoudness() const { return RecentLoudness; }

	/** Servidor: cada ruido que emite (además del aviso a AIPerception). */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Noise")
	FNDOnNoiseEmitted OnNoiseEmitted;

	/** Ruido de un paso agachado (GDD 9.2: 4/100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchStepLoudness = 0.04f;

	/** Ruido de un paso andando (GDD 9.2: 10/100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float WalkStepLoudness = 0.10f;

	/** Ruido de un paso corriendo (GDD 9.2: 22/100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float SprintStepLoudness = 0.22f;

	/** Ruido al aterrizar de un salto normal; crece con la caída hasta el doble. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float LandLoudness = 0.15f;

	/** Centímetros entre pasos andando (agachado son menos, corriendo más). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "10"))
	float StrideLength = 55.f;

	/** Hasta dónde llega un ruido de 1, en cm. Los más flojos llegan proporcionalmente menos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Noise", meta = (ClampMin = "100"))
	float MaxRange = 2500.f;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	ACharacter* GetCharacter() const;
	bool IsSilenced() const;

	float StrideAccumulator = 0.f;
	float RecentLoudness = 0.f;
};
