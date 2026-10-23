// No Despiertes Eso

#include "Monsters/NDMonsterCharacter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HouseGen/NDHouseLot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Monsters/NDMonsterAIController.h"
#include "Monsters/NDMonsterBrain.h"
#include "Monsters/NDMonsterDefinition.h"
#include "Monsters/NDMonsterUtils.h"
#include "Monsters/NDSnoreSoundWave.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace NDMonsterCharacter
{
	constexpr float CapsuleRadius = 60.f;
	constexpr float CapsuleHalfHeight = 210.f;
	const FVector RightHandRest(0.f, 95.f, -40.f);
	const FVector RightHandRaised(80.f, 70.f, 150.f);
}

ANDMonsterCharacter::ANDMonsterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);

	AIControllerClass = ANDMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(NDMonsterCharacter::CapsuleRadius, NDMonsterCharacter::CapsuleHalfHeight);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 220.f, 0.f);
	Move->MaxWalkSpeed = 200.f;
	Move->MaxStepHeight = 60.f;
	Move->bUseRVOAvoidance = false;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());

	StateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateText"));
	StateText->SetupAttachment(GetCapsuleComponent());
	StateText->SetRelativeLocation(FVector(0.f, 0.f, 270.f));
	StateText->SetHorizontalAlignment(EHTA_Center);
	StateText->SetVerticalAlignment(EVRTA_TextCenter);
	StateText->SetWorldSize(70.f);
	StateText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SnoreAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SnoreAudio"));
	SnoreAudio->SetupAttachment(GetCapsuleComponent());
	SnoreAudio->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	SnoreAudio->bAutoActivate = false;

	BuildPlaceholderBody();
}

void ANDMonsterCharacter::BuildPlaceholderBody()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = MaterialFinder.Object;

	// Medidas relativas al centro de la cápsula (el suelo está en Z = -210). Las formas del motor miden 100 cm.
	BodyMesh = AddShape(TEXT("Body"), CylinderFinder.Object, FVector(0.f, 0.f, -70.f), FVector(1.5f, 1.5f, 2.6f));
	BellyMesh = AddShape(TEXT("Belly"), SphereFinder.Object, FVector(45.f, 0.f, -40.f), FVector(0.9f, 1.3f, 1.4f));
	HeadMesh = AddShape(TEXT("Head"), SphereFinder.Object, FVector(0.f, 0.f, 125.f), FVector(1.2f));
	NoseMesh = AddShape(TEXT("Nose"), SphereFinder.Object, FVector(58.f, 0.f, 118.f), FVector(0.38f));
	CapMesh = AddShape(TEXT("NightCap"), ConeFinder.Object, FVector(-10.f, 0.f, 215.f), FVector(1.05f, 1.05f, 1.1f), FRotator(-20.f, 0.f, 0.f));
	EyeLeft = AddShape(TEXT("EyeLeft"), SphereFinder.Object, FVector(48.f, -24.f, 148.f), FVector(0.22f));
	EyeRight = AddShape(TEXT("EyeRight"), SphereFinder.Object, FVector(48.f, 24.f, 148.f), FVector(0.22f));
	HandLeft = AddShape(TEXT("HandLeft"), SphereFinder.Object, FVector(0.f, -95.f, -40.f), FVector(0.55f));
	HandRight = AddShape(TEXT("HandRight"), SphereFinder.Object, NDMonsterCharacter::RightHandRest, FVector(0.55f));
	RightHandRest = NDMonsterCharacter::RightHandRest;
}

UStaticMeshComponent* ANDMonsterCharacter::AddShape(FName Name, UStaticMesh* ShapeMesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* Shape = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Shape->SetupAttachment(VisualRoot);
	Shape->SetStaticMesh(ShapeMesh);
	Shape->SetRelativeLocation(Location);
	Shape->SetRelativeRotation(Rotation);
	Shape->SetRelativeScale3D(Scale);
	// Solo se ve: la colisión es la cápsula.
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetGenerateOverlapEvents(false);
	Shape->SetCanEverAffectNavigation(false);
	return Shape;
}

void ANDMonsterCharacter::PaintShapes()
{
	if (!ShapeMaterial)
	{
		return;
	}
	auto Paint = [this](UStaticMeshComponent* ShapeMesh, const FLinearColor& Color)
	{
		if (ShapeMesh)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			ShapeMesh->SetMaterial(0, Material);
		}
	};
	// Pijama azul de rayas imaginarias, piel rosada, nariz roja de tanto roncar y gorro de dormir.
	Paint(BodyMesh, FLinearColor(0.18f, 0.25f, 0.55f));
	Paint(BellyMesh, FLinearColor(0.25f, 0.33f, 0.65f));
	Paint(HeadMesh, FLinearColor(0.85f, 0.6f, 0.5f));
	Paint(NoseMesh, FLinearColor(0.8f, 0.15f, 0.12f));
	Paint(CapMesh, FLinearColor(0.75f, 0.12f, 0.15f));
	Paint(EyeLeft, FLinearColor(0.95f, 0.95f, 0.9f));
	Paint(EyeRight, FLinearColor(0.95f, 0.95f, 0.9f));
	Paint(HandLeft, FLinearColor(0.85f, 0.6f, 0.5f));
	Paint(HandRight, FLinearColor(0.85f, 0.6f, 0.5f));
}

void ANDMonsterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANDMonsterCharacter, Definition);
	DOREPLIFETIME(ANDMonsterCharacter, MonsterState);
	DOREPLIFETIME(ANDMonsterCharacter, Suspicion);
	DOREPLIFETIME(ANDMonsterCharacter, SightAlert);
	DOREPLIFETIME(ANDMonsterCharacter, SnoreCycle);
	DOREPLIFETIME(ANDMonsterCharacter, bGrabTelegraph);
	DOREPLIFETIME(ANDMonsterCharacter, BedTransform);
	DOREPLIFETIME(ANDMonsterCharacter, Victims);
}

void ANDMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	PaintShapes();
	if (HasAuthority() && BedTransform.Equals(FTransform::Identity))
	{
		// Colocada a mano en un mapa: duerme donde está.
		FTransform Bed = GetActorTransform();
		Bed.SetLocation(Bed.GetLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		Bed.SetScale3D(FVector::OneVector);
		BedTransform = Bed;
	}
	LyingAlpha = IsInBed() ? 1.f : 0.f;

	if (bSynthSnore && GetNetMode() != NM_DedicatedServer)
	{
		SnoreWave = NewObject<UNDSnoreSoundWave>(this);
		SnoreAudio->SetSound(SnoreWave);
		SnoreAudio->bOverrideAttenuation = true;
		SnoreAudio->AttenuationOverrides.bAttenuate = true;
		SnoreAudio->AttenuationOverrides.bSpatialize = true;
		SnoreAudio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		SnoreAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(500.f);
		SnoreAudio->AttenuationOverrides.FalloffDistance = 3500.f;
		SnoreAudio->Play();
	}
	UpdateStateText();

	if (GetNetMode() != NM_DedicatedServer)
	{
		// Al llegar a la casa, el jugador sabe que hay alguien durmiendo y hacia dónde.
		FTimerHandle WarnTimer;
		GetWorldTimerManager().SetTimer(WarnTimer, this, &ANDMonsterCharacter::ShowArrivalWarning, 4.f, false);
	}
}

void ANDMonsterCharacter::ShowArrivalWarning()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->IsLocalController() || !LocalPawn)
	{
		return;
	}
	const float Height = BedTransform.GetLocation().Z - (LocalPawn->GetActorLocation().Z - LocalPawn->GetSimpleCollisionHalfHeight());
	const FText Where = Height > 300.f ? NSLOCTEXT("NDMonster", "BedUp", "en el piso de arriba")
		: Height < -300.f ? NSLOCTEXT("NDMonster", "BedDown", "en el piso de abajo")
		: NSLOCTEXT("NDMonster", "BedSame", "en este piso");
	NDMonsterUtils::ShowLocalToast(PC, FText::Format(NSLOCTEXT("NDMonster", "Arrival", "El Dueño duerme {0}. Escucha sus ronquidos y no hagas ruido."), Where), 6.f);
}

void ANDMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLocalInputBlocked)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetIgnoreMoveInput(false);
		}
		bLocalInputBlocked = false;
	}
	if (SnoreAudio)
	{
		SnoreAudio->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

const UNDMonsterDefinition* ANDMonsterCharacter::GetDefinition() const
{
	return Definition ? Definition.Get() : GetDefault<UNDMonsterDefinition>();
}

double ANDMonsterCharacter::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

bool ANDMonsterCharacter::IsSnoreMaskActive() const
{
	return NDMonsterBrain::IsSnoringState(MonsterState) && SnoreCycle.IsInMaskWindow(GetServerTime());
}

float ANDMonsterCharacter::GetSecondsToNextSnore() const
{
	return NDMonsterBrain::IsSnoringState(MonsterState) ? SnoreCycle.GetSecondsToNextWindow(GetServerTime()) : -1.f;
}

bool ANDMonsterCharacter::IsInBed() const
{
	return NDMonsterBrain::IsInBed(MonsterState);
}

bool ANDMonsterCharacter::IsPawnDazed(const APawn* Pawn) const
{
	const double Now = GetServerTime();
	return Pawn && Victims.ContainsByPredicate([Pawn, Now](const FNDMonsterVictim& V) { return V.Pawn.Get() == Pawn && V.DazedUntilServerTime > Now; });
}

float ANDMonsterCharacter::GetNoiseRiskAt(FVector Location, float BaseLoudness) const
{
	const UNDMonsterDefinition* Def = GetDefinition();
	const FVector Ear = GetActorLocation();
	const float Transmission = NDMonsterUtils::GetSoundTransmission(this, Location, Ear);
	const float Mask = NDMonsterBrain::GetSnoreMask(*Def, MonsterState, SnoreCycle, GetServerTime(), BaseLoudness);
	const float Perceived = NDMonsterBrain::GetPerceivedLoudness(*Def, BaseLoudness, FVector::Dist(Location, Ear), Transmission, Mask);
	const float Gain = NDMonsterBrain::GetSuspicionGain(*Def, MonsterState, Perceived);
	// Un paso que sume 4 puntos de sospecha ya es riesgo alto.
	return FMath::Clamp(Gain / 4.f, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// Servidor

void ANDMonsterCharacter::SetMonsterState(ENDMonsterState NewState)
{
	if (!HasAuthority() || NewState == MonsterState)
	{
		return;
	}
	const ENDMonsterState OldState = MonsterState;
	MonsterState = NewState;
	OnRep_MonsterState(OldState);
	ForceNetUpdate();
}

void ANDMonsterCharacter::SetSuspicion(float NewSuspicion)
{
	Suspicion = FMath::Clamp(NewSuspicion, 0.f, 100.f);
}

void ANDMonsterCharacter::SetSightAlert(float NewAlert)
{
	SightAlert = FMath::Clamp(NewAlert, 0.f, 1.f);
}

void ANDMonsterCharacter::SetSnoreCycle(const FNDSnoreCycle& NewCycle)
{
	SnoreCycle = NewCycle;
	ForceNetUpdate();
}

void ANDMonsterCharacter::SetGrabTelegraph(bool bNewTelegraph)
{
	if (bGrabTelegraph != bNewTelegraph)
	{
		bGrabTelegraph = bNewTelegraph;
		ForceNetUpdate();
	}
}

void ANDMonsterCharacter::AddDazedVictim(APawn* Pawn, double UntilServerTime)
{
	if (!HasAuthority() || !Pawn)
	{
		return;
	}
	PruneVictims();
	FNDMonsterVictim* Found = Victims.FindByPredicate([Pawn](const FNDMonsterVictim& V) { return V.Pawn.Get() == Pawn; });
	if (!Found)
	{
		Found = &Victims.AddDefaulted_GetRef();
		Found->Pawn = Pawn;
	}
	Found->DazedUntilServerTime = UntilServerTime;
	OnRep_Victims();
	ForceNetUpdate();
}

void ANDMonsterCharacter::PruneVictims()
{
	const double Now = GetServerTime();
	const int32 Removed = Victims.RemoveAll([Now](const FNDMonsterVictim& V) { return !IsValid(V.Pawn) || V.DazedUntilServerTime <= Now; });
	if (Removed > 0)
	{
		OnRep_Victims();
	}
}

// ---------------------------------------------------------------------------
// Todas las máquinas

void ANDMonsterCharacter::OnRep_MonsterState(ENDMonsterState OldState)
{
	UpdateStateText();
	OnMonsterStateChanged.Broadcast(MonsterState, OldState);
}

void ANDMonsterCharacter::OnRep_Victims()
{
	UpdateLocalVictim();
}

void ANDMonsterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
	UpdateLocalVictim();
}

void ANDMonsterCharacter::UpdateLocalVictim()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	const bool bShouldBlock = IsPawnDazed(PC->GetPawn());
	if (bShouldBlock != bLocalInputBlocked)
	{
		// SetIgnoreMoveInput cuenta llamadas: siempre por parejas.
		PC->SetIgnoreMoveInput(bShouldBlock);
		bLocalInputBlocked = bShouldBlock;
		if (bShouldBlock)
		{
			NDMonsterUtils::ShowLocalToast(PC, NSLOCTEXT("NDE", "MonsterDazed", "¡Manotazo! Estás aturdido. Si te vuelve a coger, te encierra."), 3.f);
		}
	}
}

void ANDMonsterCharacter::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double Now = GetServerTime();
	// Al levantarse (WakingUp) se incorpora poco a poco: es el aviso antes de andar.
	LyingAlpha = FMath::FInterpConstantTo(LyingAlpha, IsInBed() ? 1.f : 0.f, DeltaSeconds, 0.8f);

	// Postura: tumbado en la cama o de pie. Al revolverse se gira un poco.
	StirTime += DeltaSeconds;
	const float Wobble = MonsterState == ENDMonsterState::Stirring ? FMath::Sin(StirTime * 2.2f) * 12.f : 0.f;
	const FQuat Standing = FQuat::Identity;
	const FQuat Lying = FRotator(SleepPitch, 0.f, Wobble).Quaternion();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	VisualRoot->SetRelativeRotation(FQuat::Slerp(Standing, Lying, LyingAlpha));
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, (SleepBodyHeight - HalfHeight) * LyingAlpha));

	// Pecho: se hincha al inspirar y vibra con el ronquido; despierto respira normal.
	const float SnoreLoudness = NDMonsterBrain::IsSnoringState(MonsterState) ? SnoreCycle.GetLoudness(Now) : 0.f;
	const float Breath = SnoreCycle.IsActive() ? SnoreLoudness : 0.15f * (0.5f + 0.5f * FMath::Sin(StirTime * 1.5f));
	BellyMesh->SetRelativeScale3D(FVector(0.9f + 0.35f * Breath, 1.3f + 0.1f * Breath, 1.4f));

	// Ojos: cerrados al dormir, uno abierto medio despierto, los dos despierto.
	const bool bAwake = !IsInBed();
	const float OpenLeft = bAwake ? 1.f : 0.f;
	const float OpenRight = bAwake || MonsterState == ENDMonsterState::HalfAwake ? 1.f : 0.f;
	EyeLeft->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.03f + 0.19f * OpenLeft));
	EyeRight->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.03f + 0.19f * OpenRight));

	// Mano: se levanta antes del manotazo (el aviso).
	const FVector HandTarget = bGrabTelegraph ? NDMonsterCharacter::RightHandRaised : RightHandRest;
	HandRight->SetRelativeLocation(FMath::VInterpTo(HandRight->GetRelativeLocation(), HandTarget, DeltaSeconds, bGrabTelegraph ? 8.f : 4.f));
	HandRight->SetRelativeScale3D(FVector(bGrabTelegraph ? 0.8f : 0.55f));

	// Cartel: encima de la cabeza, mirando a la cámara local; los Zzz laten con el ronquido.
	const FVector TextUp(0.f, 0.f, FMath::Lerp(270.f, SleepBodyHeight - HalfHeight + 170.f, LyingAlpha));
	StateText->SetRelativeLocation(TextUp);
	StateText->SetVisibility(bShowStateText);
	if (bShowStateText)
	{
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - StateText->GetComponentLocation();
			StateText->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
		const float Size = NDMonsterBrain::IsSnoringState(MonsterState) ? 50.f + 50.f * SnoreLoudness : 80.f;
		StateText->SetWorldSize(Size);
	}

	if (SnoreWave)
	{
		SnoreWave->SetLevel(SnoreLoudness);
		SnoreWave->SetPitch(MonsterState == ENDMonsterState::LightSleep ? 1.25f : 1.f);
	}
}

void ANDMonsterCharacter::UpdateStateText()
{
	if (!StateText)
	{
		return;
	}
	FString Label;
	FColor Color = FColor::White;
	switch (MonsterState)
	{
	case ENDMonsterState::DeepSleep: Label = TEXT("Zzz"); Color = FColor(170, 200, 255); break;
	case ENDMonsterState::LightSleep: Label = TEXT("zz"); Color = FColor(200, 220, 255); break;
	case ENDMonsterState::Stirring: Label = TEXT("?"); Color = FColor(255, 230, 120); break;
	case ENDMonsterState::HalfAwake: Label = TEXT("??"); Color = FColor(255, 200, 60); break;
	case ENDMonsterState::WakingUp: Label = TEXT("!?"); Color = FColor(255, 160, 40); break;
	case ENDMonsterState::Investigate:
	case ENDMonsterState::Search: Label = TEXT("?!"); Color = FColor(255, 140, 30); break;
	case ENDMonsterState::Chase: Label = TEXT("!!"); Color = FColor(255, 40, 30); break;
	case ENDMonsterState::ReturnToSleep: Label = TEXT("..."); Color = FColor(200, 200, 200); break;
	case ENDMonsterState::NightWalk: Label = TEXT("zZ~"); Color = FColor(170, 200, 255); break;
	case ENDMonsterState::DawnHunt: Label = TEXT("!!"); Color = FColor(255, 90, 30); break;
	default: break;
	}
	StateText->SetText(FText::FromString(Label));
	StateText->SetTextRenderColor(Color);
}
