// No Despiertes Eso

#include "Monsters/NDSnoreSoundWave.h"

UNDSnoreSoundWave::UNDSnoreSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(Rate);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
}

int32 UNDSnoreSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());

	const float Target = TargetLevel.load();
	const float Pitch = TargetPitch.load();
	// Vibración del paladar (~30 Hz) que corta el ruido grave en "golpes": eso suena a ronquido.
	const float FlutterHz = 30.f * Pitch;
	const float ToneHz = 85.f * Pitch;
	const float Smooth = 0.0012f;

	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Level += (Target - Level) * Smooth;

		// Ruido blanco barato (xorshift) y dos pasos bajos: ruido grave.
		NoiseSeed ^= NoiseSeed << 13;
		NoiseSeed ^= NoiseSeed >> 17;
		NoiseSeed ^= NoiseSeed << 5;
		const float White = (NoiseSeed & 0xFFFF) / 32767.5f - 1.f;
		LowPass += (White - LowPass) * 0.06f;
		LowPass2 += (LowPass - LowPass2) * 0.12f;

		FlutterPhase += FlutterHz / Rate;
		FlutterPhase -= FMath::FloorToFloat(FlutterPhase);
		TonePhase += ToneHz / Rate;
		TonePhase -= FMath::FloorToFloat(TonePhase);

		const float FlutterSin = 0.5f + 0.5f * FMath::Sin(2.f * PI * FlutterPhase);
		const float Flutter = FlutterSin * FlutterSin * FlutterSin;
		// A poco volumen es casi solo respiración (ruido); fuerte, vibra.
		const float Body = LowPass2 * (0.4f + 2.2f * Flutter * Level) + 0.25f * Flutter * FMath::Sin(2.f * PI * TonePhase) * Level;
		const float Breath = (White - LowPass) * 0.03f;
		const float Sample = FMath::Clamp((Body * 3.f + Breath) * Level, -1.f, 1.f);
		Out[Index] = static_cast<int16>(Sample * 26000.f);
	}
	return NumSamples;
}
