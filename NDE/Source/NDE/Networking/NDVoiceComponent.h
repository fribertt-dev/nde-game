// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NDVoiceComponent.generated.h"

class UVOIPTalker;
class USoundAttenuation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNDOnVoiceNoise, APawn*, Speaker, FVector, Location, float, Loudness);

/**
 * Voz de proximidad (GDD 31), colgada del PlayerController.
 *
 * Cliente: pulsar para hablar y, en la expedición, voz de cada compañero
 * posicionada en su personaje con atenuación por distancia y paredes.
 * Servidor: mientras alguien transmite en la expedición, emite ruido de voz
 * en la posición oficial de su personaje para que los monstruos lo oigan.
 * No se envía ni analiza audio: solo "transmitiendo sí/no" (GDD 31.4).
 */
UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDVoiceComponent();

	/** Tag del ruido de voz para AIPerception (UAISense_Hearing). */
	static const FName NoiseTag;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Solo jugador local: empieza o deja de transmitir (pulsar para hablar). */
	UFUNCTION(BlueprintCallable, Category = "NDE|Voice")
	void SetTransmitting(bool bTransmit);

	/** true mientras el jugador local está hablando. */
	UFUNCTION(BlueprintPure, Category = "NDE|Voice")
	bool IsTransmitting() const { return bLocalTransmitting; }

	/** Servidor: cada pulso de ruido de voz, además del aviso a AIPerception. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Voice")
	FNDOnVoiceNoise OnVoiceNoise;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetTransmitting(bool bTransmit);

private:
	APlayerController* GetPC() const;
	bool IsExpeditionInProgress() const;

	/** Cliente: registra o actualiza la voz posicional de cada compañero. */
	void UpdateRemoteTalkers();
	USoundAttenuation* GetAttenuation();

	/** Servidor: un pulso de ruido según cuánto lleva hablando (GDD 31.3). */
	void EmitVoiceNoise();

	bool bLocalTransmitting = false;

	// Servidor
	bool bServerTransmitting = false;
	double ServerTransmitStartTime = 0.0;
	double LastServerToggleTime = -1.0;
	int32 TogglesThisSecond = 0;
	FTimerHandle NoiseTimer;

	// Cliente
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<UVOIPTalker>> RemoteTalkers;
	float TalkerRefreshAccumulator = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> RuntimeAttenuation;
};
