// No Despiertes Eso

#include "Players/NDNoiseEmitterComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Perception/AISense_Hearing.h"
#include "Players/NDCharacter.h"
#include "Players/NDHidingComponent.h"
#include "Tools/NDToolDeployables.h"

const FName UNDNoiseEmitterComponent::FootstepTag(TEXT("NDE.Noise.Footstep"));
const FName UNDNoiseEmitterComponent::LandTag(TEXT("NDE.Noise.Land"));
const FName UNDNoiseEmitterComponent::ActionTag(TEXT("NDE.Noise.Action"));

namespace NDNoise
{
	TAutoConsoleVariable<int32> CVarDebug(
		TEXT("nde.Noise.Debug"),
		0,
		TEXT("Dibuja cada ruido del Desvelado (pasos, aterrizajes) con un círculo de su alcance: 1 = sí, 0 = no."));
}

UNDNoiseEmitterComponent::UNDNoiseEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

ACharacter* UNDNoiseEmitterComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UNDNoiseEmitterComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ACharacter* Character = GetCharacter())
	{
		Character->LandedDelegate.AddDynamic(this, &UNDNoiseEmitterComponent::HandleLanded);
	}
}

bool UNDNoiseEmitterComponent::IsSilenced() const
{
	const UNDHidingComponent* Hiding = GetOwner() ? GetOwner()->FindComponentByClass<UNDHidingComponent>() : nullptr;
	return Hiding && Hiding->IsHidden();
}

void UNDNoiseEmitterComponent::EmitNoise(FVector Location, float Loudness, FName Tag)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || IsSilenced())
	{
		return;
	}
	const float Final = FMath::Clamp(Loudness * ANDSilentFoamPatch::GetNoiseMultiplierAt(this, Location), 0.f, 1.f);
	if (Final <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	RecentLoudness = FMath::Max(RecentLoudness, Final);

	// Alcance proporcional a la fuerza, con un mínimo para que los pasos cercanos se oigan.
	const float Range = FMath::Max(150.f, MaxRange * FMath::Sqrt(Final));
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), Location, Final, Owner, Range, Tag);
	OnNoiseEmitted.Broadcast(Owner, Location, Final, Tag);

	if (NDNoise::CVarDebug.GetValueOnGameThread() != 0)
	{
		const FColor Color = Tag == LandTag ? FColor::Orange : (Final >= SprintStepLoudness ? FColor::Red : FColor::Yellow);
		DrawDebugCircle(GetWorld(), Location + FVector(0.f, 0.f, 2.f), Range, 32, Color, false, 0.6f, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
}

void UNDNoiseEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RecentLoudness = FMath::FInterpTo(RecentLoudness, 0.f, DeltaTime, 3.f);

	ACharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Speed = Character->GetVelocity().Size2D();
	if (!Movement->IsMovingOnGround() || Speed < 20.f || IsSilenced())
	{
		StrideAccumulator = 0.f;
		return;
	}

	const ANDCharacter* Desvelado = Cast<ANDCharacter>(Character);
	const bool bSprinting = Desvelado && Desvelado->IsSprinting() && Speed > Desvelado->WalkSpeed * 1.05f;
	float Stride = StrideLength;
	float Loudness = WalkStepLoudness;
	if (Character->bIsCrouched)
	{
		Stride *= 0.75f;
		Loudness = CrouchStepLoudness;
	}
	else if (bSprinting)
	{
		Stride *= 1.4f;
		Loudness = SprintStepLoudness;
	}

	StrideAccumulator += Speed * DeltaTime;
	if (StrideAccumulator >= Stride)
	{
		StrideAccumulator -= Stride;
		const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		EmitNoise(Feet, Loudness, FootstepTag);
	}
}

void UNDNoiseEmitterComponent::HandleLanded(const FHitResult& Hit)
{
	ACharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	// Velocidad de caída: un salto normal (unos 450 cm/s) da LandLoudness; una caída larga, hasta el doble.
	const float FallSpeed = FMath::Abs(Character->GetVelocity().Z);
	const float Scale = FMath::GetMappedRangeValueClamped(FVector2D(300.f, 900.f), FVector2D(0.6f, 2.f), FallSpeed);
	const float Crouched = Character->bIsCrouched ? 0.5f : 1.f;
	EmitNoise(Hit.ImpactPoint, LandLoudness * Scale * Crouched, LandTag);
}
