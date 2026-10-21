// No Despiertes Eso

#include "Networking/NDVoiceComponent.h"

#include "Core/NDLog.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/VoiceConfig.h"
#include "Networking/NDExpeditionGameState.h"
#include "Networking/NDNetworkSettings.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

const FName UNDVoiceComponent::NoiseTag(TEXT("Noise.Voice"));

namespace NDVoice
{
	constexpr float TalkerRefreshSeconds = 0.5f;
	/** Un pulso por segundo y por jugador, para no sumar decenas de eventos iguales (GDD 31.3). */
	constexpr float NoisePulseSeconds = 1.f;
	/** Más cambios por segundo se ignoran (cliente modificado que alterna sin parar). */
	constexpr int32 MaxTogglesPerSecond = 6;
}

UNDVoiceComponent::UNDVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

APlayerController* UNDVoiceComponent::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UNDVoiceComponent::IsExpeditionInProgress() const
{
	const ANDExpeditionGameState* ExpState = GetWorld() ? GetWorld()->GetGameState<ANDExpeditionGameState>() : nullptr;
	return ExpState && ExpState->GetSessionPhase() == ENDExpeditionSessionPhase::InProgress;
}

void UNDVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	// Solo el PlayerController local necesita gestionar la escucha.
	const APlayerController* PC = GetPC();
	SetComponentTickEnabled(PC && PC->IsLocalController());
}

void UNDVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(NoiseTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UNDVoiceComponent::SetTransmitting(bool bTransmit)
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->IsLocalController() || bLocalTransmitting == bTransmit)
	{
		return;
	}
	bLocalTransmitting = bTransmit;
	if (bTransmit)
	{
		PC->StartTalking();
	}
	else
	{
		PC->StopTalking();
	}
	ServerSetTransmitting(bTransmit);
}

void UNDVoiceComponent::ServerSetTransmitting_Implementation(bool bTransmit)
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Now - LastServerToggleTime >= 1.0)
	{
		LastServerToggleTime = Now;
		TogglesThisSecond = 0;
	}
	if (++TogglesThisSecond > NDVoice::MaxTogglesPerSecond || bServerTransmitting == bTransmit)
	{
		return;
	}

	bServerTransmitting = bTransmit;
	FTimerManager& Timers = World->GetTimerManager();
	if (bTransmit)
	{
		ServerTransmitStartTime = Now;
		Timers.SetTimer(NoiseTimer, this, &ThisClass::EmitVoiceNoise, NDVoice::NoisePulseSeconds, true, 0.f);
	}
	else
	{
		Timers.ClearTimer(NoiseTimer);
	}
}

void UNDVoiceComponent::EmitVoiceNoise()
{
	// En La Madriguera la voz no tiene peligro (GDD 31.1).
	const APlayerController* PC = GetPC();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!bServerTransmitting || !Pawn || !IsExpeditionInProgress())
	{
		return;
	}

	// Comentario breve: tier 1. Conversación sostenida: tier 2 (GDD 31.3).
	const UNDNetworkSettings* NetSettings = UNDNetworkSettings::Get();
	const double Duration = GetWorld()->GetTimeSeconds() - ServerTransmitStartTime;
	const float Loudness = Duration >= NetSettings->VoiceSustainedSeconds
		? NetSettings->VoiceSustainedLoudness
		: NetSettings->VoiceBriefLoudness;

	// Posición oficial del personaje en el servidor, nunca la que diga el cliente.
	const FVector Location = Pawn->GetActorLocation();
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), Location, Loudness, Pawn, NetSettings->VoiceNoiseMaxRange, NoiseTag);
	OnVoiceNoise.Broadcast(Pawn, Location, Loudness);
	UE_LOG(LogNDNet, Verbose, TEXT("Ruido de voz de %s: %.2f"), *Pawn->GetName(), Loudness);
}

void UNDVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TalkerRefreshAccumulator += DeltaTime;
	if (TalkerRefreshAccumulator >= NDVoice::TalkerRefreshSeconds)
	{
		TalkerRefreshAccumulator = 0.f;
		UpdateRemoteTalkers();
	}
}

void UNDVoiceComponent::UpdateRemoteTalkers()
{
	const APlayerController* PC = GetPC();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!PC || !GameState)
	{
		return;
	}

	// En La Madriguera la voz es de sala, sin posición; en la casa, de proximidad.
	const bool bProximity = GetWorld()->GetGameState<ANDExpeditionGameState>() != nullptr;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (!PS || PS == PC->PlayerState)
		{
			continue;
		}

		TWeakObjectPtr<UVOIPTalker>& Talker = RemoteTalkers.FindOrAdd(PS);
		if (!Talker.IsValid())
		{
			Talker = UVOIPTalker::CreateTalkerForPlayer(PS);
		}
		if (!Talker.IsValid())
		{
			continue;
		}

		APawn* Pawn = PS->GetPawn();
		USceneComponent* Attach = (bProximity && Pawn) ? Pawn->GetRootComponent() : nullptr;
		if (Talker->Settings.ComponentToAttachTo != Attach)
		{
			Talker->Settings.ComponentToAttachTo = Attach;
			Talker->Settings.AttenuationSettings = Attach ? GetAttenuation() : nullptr;
		}
	}

	for (auto It = RemoteTalkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

USoundAttenuation* UNDVoiceComponent::GetAttenuation()
{
	const UNDNetworkSettings* NetSettings = UNDNetworkSettings::Get();
	if (USoundAttenuation* Configured = NetSettings->VoiceAttenuation.LoadSynchronous())
	{
		return Configured;
	}
	if (!RuntimeAttenuation)
	{
		// Distancias de diseño del GDD 31.2: plena hasta 300 uu, inaudible a 1400 uu.
		RuntimeAttenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& Att = RuntimeAttenuation->Attenuation;
		Att.bAttenuate = true;
		Att.bSpatialize = true;
		Att.AttenuationShape = EAttenuationShape::Sphere;
		Att.AttenuationShapeExtents = FVector(NetSettings->VoiceFullVolumeRadius, 0.f, 0.f);
		Att.FalloffDistance = FMath::Max(1.f, NetSettings->VoiceNoiseMaxRange - NetSettings->VoiceFullVolumeRadius);
		Att.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
		// Paredes y puertas apagan la voz (traza de visibilidad).
		Att.bEnableOcclusion = true;
		Att.OcclusionTraceChannel = ECC_Visibility;
		Att.OcclusionVolumeAttenuation = 0.3f;
		Att.OcclusionLowPassFilterFrequency = 1200.f;
	}
	return RuntimeAttenuation;
}
