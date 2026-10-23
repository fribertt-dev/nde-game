// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include <atomic>
#include "NDSnoreSoundWave.generated.h"

/**
 * Ronquido sintetizado en tiempo real (ruido grave con vibración del paladar), para tener sonido sin
 * assets de audio. La criatura le pasa cada frame lo fuerte que ronca según su ciclo replicado.
 * Se sustituirá por los sonidos reales cuando existan.
 */
UCLASS()
class NDE_API UNDSnoreSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UNDSnoreSoundWave(const FObjectInitializer& ObjectInitializer);

	/** 0-1. Hilo de juego. */
	void SetLevel(float InLevel) { TargetLevel.store(FMath::Clamp(InLevel, 0.f, 1.f)); }

	/** Tono del ronquido (1 = normal). Hilo de juego. */
	void SetPitch(float InPitch) { TargetPitch.store(FMath::Clamp(InPitch, 0.5f, 2.f)); }

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	static constexpr int32 Rate = 22050;

	std::atomic<float> TargetLevel{ 0.f };
	std::atomic<float> TargetPitch{ 1.f };

	// Solo hilo de audio.
	float Level = 0.f;
	float FlutterPhase = 0.f;
	float TonePhase = 0.f;
	float LowPass = 0.f;
	float LowPass2 = 0.f;
	uint32 NoiseSeed = 0x2545F491u;
};
