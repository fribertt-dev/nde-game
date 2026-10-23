// No Despiertes Eso

#include "Monsters/NDMonsterAIController.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Monsters/NDMonsterBrain.h"
#include "Monsters/NDMonsterCharacter.h"
#include "Monsters/NDMonsterDefinition.h"
#include "Monsters/NDMonsterSettings.h"
#include "Monsters/NDMonsterSubsystem.h"
#include "Monsters/NDMonsterUtils.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDMonsterAI, Log, All);

static TAutoConsoleVariable<int32> CVarNDMonsterAI(
	TEXT("nde.Monster.AI"),
	1,
	TEXT("1 = la criatura piensa (por defecto). 0 = congelada donde está (para probar sin que moleste)."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarNDMonsterDebug(
	TEXT("nde.Monster.Debug"),
	0,
	TEXT("1 = dibuja estado, sospecha, ruidos oídos, vista y destino de la criatura (en el servidor)."),
	ECVF_Cheat);

namespace NDMonsterAI
{
	/** Sitios a menos de esto (cm) cuentan como alcanzados. */
	constexpr float Acceptance = 120.f;
	/** Cada cuánto se comprueba si está atascado (s) y cuánto tiene que haberse movido (cm). */
	constexpr float StuckSeconds = 3.f;
	constexpr float StuckDistance = 40.f;
	/** Ruidos recientes que se guardan para depuración. */
	constexpr int32 MaxRecentNoises = 12;
	/** Pausa en cada sitio al buscar (s). */
	constexpr float SearchPauseSeconds = 1.5f;
	/** Sospecha extra por segundo con alguien a la vista medio despierto, por cada punto de aviso. */
	constexpr float SightSuspicionPerSecond = 30.f;
	/** De cerca ve de reojo aunque no le mires de frente (cm y grados). */
	constexpr float PeripheralDistance = 250.f;
	constexpr float PeripheralHalfAngle = 110.f;
}

ANDMonsterAIController::ANDMonsterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = false;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 8000.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(1.f);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(HearingConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);

	Rng.Initialize(0x4E44454D);
}

const UNDMonsterDefinition& ANDMonsterAIController::GetDef() const
{
	const UNDMonsterDefinition* Def = Monster ? Monster->GetDefinition() : nullptr;
	return Def ? *Def : *GetDefault<UNDMonsterDefinition>();
}

double ANDMonsterAIController::Now() const
{
	return Monster ? Monster->GetServerTime() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void ANDMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Monster = Cast<ANDMonsterCharacter>(InPawn);
	if (!Monster)
	{
		return;
	}
	Rng.Initialize(static_cast<int32>(GetTypeHash(Monster->GetName()) ^ static_cast<uint32>(FPlatformTime::Cycles())));
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::HandlePerceptionUpdated);

	State = ENDMonsterState::DeepSleep;
	Monster->SetMonsterState(State);
	StateStartTime = Now();
	LastThinkTime = StateStartTime;
	const UNDMonsterDefinition& Def = GetDef();
	NextNightWalkTime = Now() + Rng.FRandRange(Def.NightWalkIntervalMin, FMath::Max(Def.NightWalkIntervalMin, Def.NightWalkIntervalMax));
	ApplySpeed();

	const float Rate = FMath::Clamp(UNDMonsterSettings::Get()->ThinkRate, 2.f, 30.f);
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, 1.f / Rate, true);
	UE_LOG(LogNDMonsterAI, Log, TEXT("%s: duerme (%s)."), *Monster->GetName(), *Def.MonsterId.ToString());
}

void ANDMonsterAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandlePerceptionUpdated);
	}
	Monster = nullptr;
	Super::OnUnPossess();
}

// ---------------------------------------------------------------------------
// Percepción

void ANDMonsterAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Monster || !Stimulus.WasSuccessfullySensed() || Stimulus.Type != UAISense::GetSenseID<UAISense_Hearing>())
	{
		return;
	}
	HearNoise(Stimulus.StimulusLocation, Stimulus.Strength * 100.f, Actor, Stimulus.Tag);
}

void ANDMonsterAIController::HearNoise(const FVector& Location, float BaseLoudness, AActor* NoiseInstigator, FName Tag)
{
	if (!Monster || NoiseInstigator == Monster.Get() || BaseLoudness <= 0.f)
	{
		return;
	}
	const UNDMonsterDefinition& Def = GetDef();
	const double Time = Now();
	// Golpes del mismo cuerpo muy seguidos: cuenta solo el más fuerte (GDD 9.2).
	if (NoiseInstigator)
	{
		for (FNDHeardNoise& Pending : PendingNoises)
		{
			if (Pending.Instigator.Get() == NoiseInstigator && Time - Pending.ServerTime < Def.NoiseMergeSeconds)
			{
				if (BaseLoudness > Pending.BaseLoudness)
				{
					Pending.BaseLoudness = BaseLoudness;
					Pending.Location = Location;
					Pending.Tag = Tag;
				}
				return;
			}
		}
	}
	FNDHeardNoise& Noise = PendingNoises.AddDefaulted_GetRef();
	Noise.Location = Location;
	Noise.Instigator = NoiseInstigator;
	Noise.BaseLoudness = BaseLoudness;
	Noise.Tag = Tag;
	Noise.ServerTime = Time;
}

void ANDMonsterAIController::ProcessNoises(float& OutGain)
{
	OutGain = 0.f;
	if (PendingNoises.IsEmpty())
	{
		return;
	}
	const UNDMonsterDefinition& Def = GetDef();
	const double Time = Now();
	const FVector Ear = Monster->GetActorLocation();
	TArray<FNDHeardNoise> Ready;
	for (int32 Index = PendingNoises.Num() - 1; Index >= 0; --Index)
	{
		// Se espera el tiempo de fusión para no contar dos veces el mismo golpe.
		if (Time - PendingNoises[Index].ServerTime >= Def.NoiseMergeSeconds)
		{
			Ready.Add(PendingNoises[Index]);
			PendingNoises.RemoveAtSwap(Index);
		}
	}

	const FNDHeardNoise* Loudest = nullptr;
	for (FNDHeardNoise& Noise : Ready)
	{
		const float Transmission = NDMonsterUtils::GetSoundTransmission(this, Noise.Location, Ear);
		const float Mask = NDMonsterBrain::GetSnoreMask(Def, State, Monster->GetSnoreCycle(), Time, Noise.BaseLoudness);
		Noise.Perceived = NDMonsterBrain::GetPerceivedLoudness(Def, Noise.BaseLoudness, FVector::Dist(Noise.Location, Ear), Transmission, Mask);
		Noise.SuspicionGain = NDMonsterBrain::GetSuspicionGain(Def, State, Noise.Perceived);
		OutGain += Noise.SuspicionGain;
		if (!Loudest || Noise.SuspicionGain > Loudest->SuspicionGain)
		{
			Loudest = &Noise;
		}
		RecentNoises.Add(Noise);
		UE_LOG(LogNDMonsterAI, Verbose, TEXT("Oye %s: base %.0f, llega %.1f (paso %.2f, ronquido %.2f) -> +%.1f"), *Noise.Tag.ToString(), Noise.BaseLoudness, Noise.Perceived, Transmission, Mask, Noise.SuspicionGain);
	}
	if (RecentNoises.Num() > NDMonsterAI::MaxRecentNoises)
	{
		RecentNoises.RemoveAt(0, RecentNoises.Num() - NDMonsterAI::MaxRecentNoises);
	}
	if (!Loudest || Loudest->SuspicionGain <= 0.f)
	{
		return;
	}

	LastNoiseLocation = Loudest->Location;
	LastNoiseTime = Time;
	// Despierto: un ruido claro le hace ir a mirar (si no está persiguiendo o golpeando).
	const bool bAwake = !NDMonsterBrain::IsInBed(State) && State != ENDMonsterState::WakingUp;
	if (bAwake && Loudest->SuspicionGain >= Def.AwakeInvestigateGain && State != ENDMonsterState::Chase)
	{
		LastEvidenceTime = Time;
		StartInvestigate(Loudest->Location);
	}
}

void ANDMonsterAIController::UpdateTouch(float DeltaSeconds, float& InOutGain)
{
	if (!NDMonsterBrain::IsInBed(State))
	{
		return;
	}
	const UNDMonsterDefinition& Def = GetDef();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState || Def.TouchRadius <= 0.f)
	{
		return;
	}
	// Notar a alguien pegado a la cama aunque no haga ruido (GDD 9.1, ProximityTouch).
	const FVector Body = Monster->GetActorLocation();
	for (const APlayerState* OtherPlayerState : GameState->PlayerArray)
	{
		const APawn* OtherPawn = OtherPlayerState ? OtherPlayerState->GetPawn() : nullptr;
		if (OtherPawn && NDMonsterUtils::IsTargetablePlayer(OtherPawn) && !NDMonsterUtils::IsPawnHidden(OtherPawn)
			&& FVector::Dist2D(OtherPawn->GetActorLocation(), Body) < Def.TouchRadius + Monster->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			InOutGain += Def.TouchSuspicionPerSecond * DeltaSeconds;
			LastNoiseLocation = OtherPawn->GetActorLocation();
			LastNoiseTime = Now();
		}
	}
}

FVector ANDMonsterAIController::GetEyeLocation() const
{
	if (!Monster)
	{
		return FVector::ZeroVector;
	}
	const float HalfHeight = Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// Tumbado mira desde la almohada; de pie, desde los ojos (unos 3,6 m).
	return Monster->GetActorLocation() + FVector(0.f, 0.f, Monster->IsInBed() ? -HalfHeight + 200.f : HalfHeight * 0.7f);
}

bool ANDMonsterAIController::HasLineOfSight(const APawn* Target) const
{
	if (!Target || !Monster)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDMonsterSight), true, Monster);
	Params.AddIgnoredActor(Target);
	const FVector Eye = GetEyeLocation();
	const FVector Center = Target->GetActorLocation();
	const FVector Head = Center + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight() * 0.8f);
	// Se le ve si se ve el centro o la cabeza (asomarse por encima de un mueble delata).
	for (const FVector& Point : { Center, Head })
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

bool ANDMonsterAIController::CanSee(const APawn* Target, float Range, float HalfAngle, const FVector& Gaze, float& OutDistance) const
{
	const FVector Eye = GetEyeLocation();
	const FVector ToTarget = Target->GetActorLocation() - Eye;
	OutDistance = ToTarget.Size();
	if (OutDistance > Range)
	{
		return false;
	}
	const FVector Flat = FVector(ToTarget.X, ToTarget.Y, 0.f).GetSafeNormal();
	const FVector GazeFlat = FVector(Gaze.X, Gaze.Y, 0.f).GetSafeNormal();
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Flat, GazeFlat), -1.f, 1.f)));
	const float Allowed = FVector::Dist2D(Target->GetActorLocation(), Monster->GetActorLocation()) < NDMonsterAI::PeripheralDistance ? NDMonsterAI::PeripheralHalfAngle : HalfAngle;
	return Angle <= Allowed && HasLineOfSight(Target);
}

void ANDMonsterAIController::UpdateSight(float DeltaSeconds)
{
	SeenThisTick.Reset();
	const UNDMonsterDefinition& Def = GetDef();

	// Dormido o revolviéndose no ve. Medio despierto, con un ojo hacia el último ruido.
	float Range = 0.f;
	float HalfAngle = 0.f;
	FVector Gaze = Monster->GetActorForwardVector();
	switch (State)
	{
	case ENDMonsterState::DeepSleep:
	case ENDMonsterState::LightSleep:
	case ENDMonsterState::Stirring:
		break;
	case ENDMonsterState::HalfAwake:
		Range = Def.HalfAwakeSightRange;
		HalfAngle = Def.HalfAwakeSightHalfAngle;
		if (Now() - LastNoiseTime < 10.0)
		{
			Gaze = LastNoiseLocation - Monster->GetActorLocation();
		}
		break;
	case ENDMonsterState::NightWalk:
		Range = Def.SightRange * Def.NightWalkSightScale;
		HalfAngle = Def.SightHalfAngle;
		break;
	default:
		Range = Def.SightRange;
		HalfAngle = Def.SightHalfAngle;
		break;
	}

	float BestMeter = 0.f;
	APawn* BestPawn = nullptr;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	TSet<TWeakObjectPtr<APawn>> Updated;
	if (GameState && Range > 0.f)
	{
		for (const APlayerState* OtherPlayerState : GameState->PlayerArray)
		{
			APawn* OtherPawn = OtherPlayerState ? OtherPlayerState->GetPawn() : nullptr;
			if (!OtherPawn || !NDMonsterUtils::IsTargetablePlayer(OtherPawn))
			{
				continue;
			}
			const float Visibility = NDMonsterUtils::GetPawnVisibility(OtherPawn, Def);
			float Distance = 0.f;
			if (Visibility <= 0.f || !CanSee(OtherPawn, Range, HalfAngle, Gaze, Distance))
			{
				continue;
			}
			float& Meter = Detection.FindOrAdd(TWeakObjectPtr<APawn>(OtherPawn));
			Meter = FMath::Min(1.f, Meter + NDMonsterBrain::GetDetectionRate(Def, Visibility, Distance, Range) * DeltaSeconds);
			Updated.Add(TWeakObjectPtr<APawn>(OtherPawn));
			if (Meter > BestMeter)
			{
				BestMeter = Meter;
				BestPawn = OtherPawn;
			}
			if (OtherPawn == ChaseTarget.Get())
			{
				LastSeenLocation = OtherPawn->GetActorLocation();
				LastSeenTime = Now();
			}
		}
	}
	// Quien no se ve, se va olvidando.
	for (auto It = Detection.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		if (!Updated.Contains(It.Key()))
		{
			It.Value() = FMath::Max(0.f, It.Value() - Def.DetectionDecay * DeltaSeconds);
		}
	}
	float MaxMeter = 0.f;
	for (const TPair<TWeakObjectPtr<APawn>, float>& Pair : Detection)
	{
		MaxMeter = FMath::Max(MaxMeter, Pair.Value);
	}
	Monster->SetSightAlert(MaxMeter);

	if (!BestPawn)
	{
		return;
	}
	SeenThisTick = BestPawn;
	SeenLocation = BestPawn->GetActorLocation();

	if (BestMeter >= 1.f && State != ENDMonsterState::Chase)
	{
		LastEvidenceTime = Now();
		if (NDMonsterBrain::IsInBed(State))
		{
			// Te ha visto desde la cama: se levanta (aviso) y luego persigue.
			ChaseTarget = BestPawn;
			LastSeenLocation = SeenLocation;
			LastSeenTime = Now();
			AfterWakeState = ENDMonsterState::Chase;
			EnterState(ENDMonsterState::WakingUp);
		}
		else if (State != ENDMonsterState::WakingUp)
		{
			StartChase(BestPawn);
		}
		return;
	}
	if (State == ENDMonsterState::HalfAwake)
	{
		// Algo se mueve ahí: sube la sospecha aunque no haga ruido.
		Suspicion += NDMonsterAI::SightSuspicionPerSecond * BestMeter * DeltaSeconds;
	}
	else if (BestMeter >= Def.DetectionInvestigate && State != ENDMonsterState::Chase && State != ENDMonsterState::WakingUp)
	{
		LastEvidenceTime = Now();
		if (State != ENDMonsterState::Investigate || FVector::Dist2D(InvestigateLocation, SeenLocation) > 300.f)
		{
			StartInvestigate(SeenLocation);
		}
	}
}

// ---------------------------------------------------------------------------
// Árbol de decisión

void ANDMonsterAIController::Think()
{
	if (!Monster)
	{
		return;
	}
	const double Time = Now();
	const float DeltaSeconds = FMath::Clamp(static_cast<float>(Time - LastThinkTime), 0.f, 0.5f);
	LastThinkTime = Time;

	if (CVarNDMonsterAI.GetValueOnGameThread() == 0)
	{
		HaltMovement();
		return;
	}
	// La expedición ha terminado: se queda quieto.
	if (const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this))
	{
		if (Expedition->GetPhase() == ENDExpeditionPhase::Settlement)
		{
			HaltMovement();
			return;
		}
	}

	Monster->PruneVictims();
	UpdateDawn();

	float Gain = 0.f;
	ProcessNoises(Gain);
	UpdateTouch(DeltaSeconds, Gain);
	UpdateSight(DeltaSeconds);

	switch (State)
	{
	case ENDMonsterState::DeepSleep:
	case ENDMonsterState::LightSleep:
	case ENDMonsterState::Stirring:
	case ENDMonsterState::HalfAwake:
		UpdateBed(DeltaSeconds, Gain);
		break;
	case ENDMonsterState::WakingUp:
		Suspicion += Gain;
		UpdateWakingUp();
		break;
	default:
		Suspicion += Gain;
		if (Gain <= 0.f)
		{
			Suspicion -= GetDef().DecayAwake * DeltaSeconds;
		}
		break;
	}

	switch (State)
	{
	case ENDMonsterState::Investigate: UpdateInvestigate(); break;
	case ENDMonsterState::Search: UpdateSearch(); break;
	case ENDMonsterState::Chase: UpdateChase(); break;
	case ENDMonsterState::ReturnToSleep: UpdateReturnToSleep(); break;
	case ENDMonsterState::NightWalk: UpdateNightWalk(); break;
	case ENDMonsterState::DawnHunt: UpdateDawnHunt(); break;
	default: break;
	}
	UpdateGrab();

	// Mirar alrededor y apuntar el golpe: gira en el sitio.
	if (bLookingAround && !bMoving)
	{
		const float Elapsed = static_cast<float>(Time - LookAroundStart);
		const float Yaw = LookBaseYaw + 60.f * FMath::Sin(Elapsed * 1.6f);
		FaceTowards(Monster->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * 300.f, DeltaSeconds, 120.f);
	}

	Suspicion = FMath::Clamp(Suspicion, 0.f, 100.f);
	Monster->SetSuspicion(Suspicion);
	UpdateSnore();
	UpdateStuck();
	if (CVarNDMonsterDebug.GetValueOnGameThread() != 0)
	{
		DrawDebug();
	}
}

void ANDMonsterAIController::UpdateDawn()
{
	if (bDawn)
	{
		return;
	}
	const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	if (!Expedition || Expedition->GetPhase() != ENDExpeditionPhase::DawnEscapeGrace)
	{
		return;
	}
	// Amanecer: se despierta del todo y ya no vuelve a dormir (GDD 7.7). Si perseguía, sigue.
	bDawn = true;
	Suspicion = 100.f;
	UE_LOG(LogNDMonsterAI, Log, TEXT("%s: amanece, se despierta del todo."), *Monster->GetName());
	if (NDMonsterBrain::IsInBed(State))
	{
		AfterWakeState = ENDMonsterState::DawnHunt;
		EnterState(ENDMonsterState::WakingUp);
	}
	else if (State != ENDMonsterState::Chase && State != ENDMonsterState::WakingUp)
	{
		EnterState(ENDMonsterState::DawnHunt);
	}
}

void ANDMonsterAIController::UpdateBed(float DeltaSeconds, float Gain)
{
	const UNDMonsterDefinition& Def = GetDef();
	Suspicion += Gain;
	if (Gain <= 0.f)
	{
		Suspicion -= NDMonsterBrain::GetDecayPerSecond(Def, State) * DeltaSeconds;
	}
	Suspicion = FMath::Clamp(Suspicion, 0.f, 100.f);

	const ENDMonsterState Next = NDMonsterBrain::GetBedState(Def, State, Suspicion);
	if (Next == ENDMonsterState::WakingUp)
	{
		LastEvidenceTime = Now();
		InvestigateLocation = LastNoiseLocation;
		AfterWakeState = ENDMonsterState::Investigate;
		EnterState(ENDMonsterState::WakingUp);
		return;
	}
	if (Next != State)
	{
		EnterState(Next);
		return;
	}

	// Rutina: tras dormir tranquilo un rato se levanta medio dormido a dar una vuelta por la casa.
	if (Def.bNightWalks && State == ENDMonsterState::DeepSleep && Now() >= NextNightWalkTime && Suspicion < Def.LightSleepThreshold * 0.5f)
	{
		DebugNightWalk();
	}
}

void ANDMonsterAIController::UpdateWakingUp()
{
	if (Now() - StateStartTime < GetDef().WakeUpSeconds)
	{
		return;
	}
	switch (AfterWakeState)
	{
	case ENDMonsterState::Chase:
		if (ChaseTarget.IsValid())
		{
			StartChase(ChaseTarget.Get());
		}
		else
		{
			StartInvestigate(LastSeenLocation);
		}
		break;
	case ENDMonsterState::NightWalk:
		EnterState(ENDMonsterState::NightWalk);
		break;
	case ENDMonsterState::DawnHunt:
		EnterState(ENDMonsterState::DawnHunt);
		break;
	default:
		StartInvestigate(InvestigateLocation.IsNearlyZero() ? Monster->GetActorLocation() + Monster->GetActorForwardVector() * 400.f : InvestigateLocation);
		break;
	}
}

void ANDMonsterAIController::StartInvestigate(const FVector& Location)
{
	InvestigateLocation = Location;
	bLookingAround = false;
	if (State != ENDMonsterState::Investigate)
	{
		EnterState(ENDMonsterState::Investigate);
	}
	MoveToPoint(Location, NDMonsterAI::Acceptance + 60.f);
}

void ANDMonsterAIController::UpdateInvestigate()
{
	const UNDMonsterDefinition& Def = GetDef();
	if (!bLookingAround)
	{
		if (bArrived || bMoveFailed || (!bMoving && FVector::Dist2D(Monster->GetActorLocation(), InvestigateLocation) < NDMonsterAI::Acceptance * 2.f))
		{
			// En el sitio (o no se puede llegar): mira alrededor.
			HaltMovement();
			bLookingAround = true;
			LookAroundStart = Now();
			LookBaseYaw = (InvestigateLocation - Monster->GetActorLocation()).Rotation().Yaw;
			if (FVector::Dist2D(InvestigateLocation, Monster->GetActorLocation()) < 50.f)
			{
				LookBaseYaw = Monster->GetActorRotation().Yaw;
			}
		}
		return;
	}
	if (Now() - LookAroundStart >= Def.LookAroundSeconds)
	{
		bLookingAround = false;
		StartSearch(InvestigateLocation);
	}
}

void ANDMonsterAIController::StartSearch(const FVector& Around)
{
	const UNDMonsterDefinition& Def = GetDef();
	EnterState(ENDMonsterState::Search);
	if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this))
	{
		Monsters->GetSearchPoints(Around, Def.SearchPoints, SearchPoints);
	}
	SearchIndex = 0;
	PauseUntil = 0.0;
	if (SearchPoints.IsValidIndex(0))
	{
		MoveToPoint(SearchPoints[0]);
	}
}

void ANDMonsterAIController::UpdateSearch()
{
	const UNDMonsterDefinition& Def = GetDef();
	if (Now() - LastEvidenceTime > Def.GiveUpSeconds || !SearchPoints.IsValidIndex(SearchIndex))
	{
		StartReturnToSleep();
		return;
	}
	if (PauseUntil > 0.0)
	{
		if (Now() >= PauseUntil)
		{
			PauseUntil = 0.0;
			bLookingAround = false;
			if (SearchPoints.IsValidIndex(++SearchIndex))
			{
				MoveToPoint(SearchPoints[SearchIndex]);
			}
		}
		return;
	}
	if (bArrived || bMoveFailed || !bMoving)
	{
		HaltMovement();
		PauseUntil = Now() + NDMonsterAI::SearchPauseSeconds;
		bLookingAround = true;
		LookAroundStart = Now();
		LookBaseYaw = Monster->GetActorRotation().Yaw;
	}
}

void ANDMonsterAIController::StartChase(APawn* Target)
{
	if (!Target)
	{
		return;
	}
	ChaseTarget = Target;
	LastSeenLocation = Target->GetActorLocation();
	LastSeenTime = Now();
	LastEvidenceTime = Now();
	bLookingAround = false;
	if (State != ENDMonsterState::Chase)
	{
		EnterState(ENDMonsterState::Chase);
		UE_LOG(LogNDMonsterAI, Log, TEXT("%s: persigue a %s."), *Monster->GetName(), *Target->GetName());
	}
	FollowActor(Target);
}

void ANDMonsterAIController::UpdateChase()
{
	const UNDMonsterDefinition& Def = GetDef();
	APawn* Target = ChaseTarget.Get();
	if (!Target || !NDMonsterUtils::IsTargetablePlayer(Target) || NDMonsterUtils::IsPawnHidden(Target))
	{
		// Escapó, entró en un escondite o ya no está: busca donde lo vio.
		ChaseTarget.Reset();
		StartSearch(LastSeenLocation);
		return;
	}
	if (GrabStartTime >= 0.0)
	{
		return;
	}
	const double SinceSeen = Now() - LastSeenTime;
	if (SinceSeen <= Def.ChaseMemorySeconds)
	{
		LastEvidenceTime = Now();
		if (!bChaseFollowingActor || !bMoving)
		{
			FollowActor(Target);
		}
		return;
	}
	if (SinceSeen > Def.LoseTargetSeconds)
	{
		ChaseTarget.Reset();
		StartSearch(LastSeenLocation);
		return;
	}
	// Ya no te ve: va a donde te vio por última vez, no a donde estás (GDD 10.4).
	if (bChaseFollowingActor || (!bMoving && !bArrived && FVector::Dist2D(MoveGoal, LastSeenLocation) > 50.f))
	{
		MoveToPoint(LastSeenLocation, NDMonsterAI::Acceptance);
	}
	else if (bArrived || bMoveFailed)
	{
		ChaseTarget.Reset();
		StartSearch(LastSeenLocation);
	}
}

void ANDMonsterAIController::UpdateGrab()
{
	const UNDMonsterDefinition& Def = GetDef();
	const double Time = Now();
	UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this);

	// Aviso en curso: gira hacia el objetivo y, al terminar, golpea si sigue ahí.
	if (GrabStartTime >= 0.0)
	{
		APawn* Target = GrabTarget.Get();
		if (Target)
		{
			FaceTowards(Target->GetActorLocation(), 0.1f, 400.f);
		}
		if (Time - GrabStartTime < Def.GrabTelegraphSeconds)
		{
			return;
		}
		Monster->SetGrabTelegraph(false);
		GrabStartTime = -1.0;
		NextGrabTime = Time + Def.GrabCooldownSeconds;
		const bool bInReach = Target && FVector::Dist2D(Target->GetActorLocation(), Monster->GetActorLocation()) <= Def.GrabRange * 1.2f
			&& FMath::Abs(Target->GetActorLocation().Z - Monster->GetActorLocation().Z) < 350.f;
		if (bInReach && NDMonsterUtils::IsTargetablePlayer(Target) && !NDMonsterUtils::IsPawnHidden(Target) && HasLineOfSight(Target) && Monsters)
		{
			const bool bCaged = Monsters->HandleGrab(Monster, Target);
			if (bCaged)
			{
				// Encerrado: sigue despierto un rato buscando al resto.
				ChaseTarget.Reset();
				Detection.Remove(TWeakObjectPtr<APawn>(Target));
				LastEvidenceTime = Time;
				StartSearch(Monster->GetActorLocation());
			}
		}
		else if (State == ENDMonsterState::Chase && ChaseTarget.IsValid())
		{
			FollowActor(ChaseTarget.Get());
		}
		return;
	}

	// Empezar un manotazo: solo despierto, a la vista y al alcance (nunca a través de paredes).
	if (State != ENDMonsterState::Chase && State != ENDMonsterState::Investigate && State != ENDMonsterState::Search && State != ENDMonsterState::DawnHunt)
	{
		return;
	}
	APawn* Candidate = State == ENDMonsterState::Chase ? ChaseTarget.Get() : SeenThisTick.Get();
	if (!Candidate || Time < NextGrabTime || SeenThisTick.Get() != Candidate)
	{
		return;
	}
	// Tras aturdir a alguien le da unos segundos para huir.
	if (Monsters && Time - Monsters->GetLastStrikeTime(Candidate) < Def.DazedGraceSeconds)
	{
		return;
	}
	const FVector Delta = Candidate->GetActorLocation() - Monster->GetActorLocation();
	if (FVector2D(Delta).Size() > Def.GrabRange || FMath::Abs(Delta.Z) > 350.f)
	{
		return;
	}
	GrabTarget = Candidate;
	GrabStartTime = Time;
	HaltMovement();
	Monster->SetGrabTelegraph(true);
}

void ANDMonsterAIController::StartReturnToSleep()
{
	if (bDawn)
	{
		EnterState(ENDMonsterState::DawnHunt);
		return;
	}
	EnterState(ENDMonsterState::ReturnToSleep);
	BedFailures = 0;
	MoveToPoint(Monster->GetBedTransform().GetLocation(), 80.f);
}

void ANDMonsterAIController::UpdateReturnToSleep()
{
	const FVector Bed = Monster->GetBedTransform().GetLocation();
	const bool bAtBed = FVector::Dist2D(Monster->GetActorLocation(), Bed) < 150.f;
	if (bAtBed || bArrived)
	{
		LieDown();
		return;
	}
	if (bMoveFailed || !bMoving)
	{
		// Atascado volviendo a la cama: tras tres intentos, si nadie está cerca, aparece en ella
		// con un bostezo (GDD 24.7: nunca encima de un jugador).
		if (++BedFailures >= 3)
		{
			bool bPlayerNear = false;
			if (const AGameStateBase* GameState = GetWorld()->GetGameState())
			{
				for (const APlayerState* OtherPlayerState : GameState->PlayerArray)
				{
					const APawn* OtherPawn = OtherPlayerState ? OtherPlayerState->GetPawn() : nullptr;
					bPlayerNear |= OtherPawn && FVector::Dist(OtherPawn->GetActorLocation(), Bed) < 800.f;
				}
			}
			if (!bPlayerNear)
			{
				const float HalfHeight = Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				Monster->SetActorLocation(Bed + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				LieDown();
				return;
			}
			BedFailures = 0;
		}
		MoveToPoint(Bed, 80.f);
	}
}

void ANDMonsterAIController::LieDown()
{
	const UNDMonsterDefinition& Def = GetDef();
	HaltMovement();
	Monster->SetActorRotation(FRotator(0.f, Monster->GetBedTransform().Rotator().Yaw, 0.f));
	Suspicion = FMath::Min(Suspicion, Def.BackToBedSuspicion);
	Detection.Reset();
	NextNightWalkTime = Now() + Rng.FRandRange(Def.NightWalkIntervalMin, FMath::Max(Def.NightWalkIntervalMin, Def.NightWalkIntervalMax));
	EnterState(NDMonsterBrain::GetBedState(Def, ENDMonsterState::LightSleep, Suspicion));
}

void ANDMonsterAIController::UpdateNightWalk()
{
	const UNDMonsterDefinition& Def = GetDef();
	if (PauseUntil > 0.0)
	{
		if (Now() >= PauseUntil)
		{
			PauseUntil = 0.0;
			bLookingAround = false;
			if (WalkPoints.IsValidIndex(++WalkIndex))
			{
				MoveToPoint(WalkPoints[WalkIndex]);
			}
			else
			{
				StartReturnToSleep();
			}
		}
		return;
	}
	if (!WalkPoints.IsValidIndex(WalkIndex))
	{
		StartReturnToSleep();
		return;
	}
	if (bArrived || bMoveFailed || !bMoving)
	{
		HaltMovement();
		PauseUntil = Now() + Def.NightWalkPauseSeconds;
		bLookingAround = true;
		LookAroundStart = Now();
		LookBaseYaw = Monster->GetActorRotation().Yaw;
	}
}

void ANDMonsterAIController::UpdateDawnHunt()
{
	// Ronda todas las salas una y otra vez, despierto. Ruidos y vista le sacan de la ronda.
	if (WalkPoints.IsEmpty())
	{
		UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this);
		if (Monsters)
		{
			Monsters->GetPatrolRoute(WalkPoints);
		}
		if (WalkPoints.IsEmpty() && Monsters)
		{
			Monsters->GetSearchPoints(Monster->GetActorLocation(), 4, WalkPoints);
		}
		WalkIndex = DawnRouteIndex % FMath::Max(1, WalkPoints.Num());
		if (WalkPoints.IsValidIndex(WalkIndex))
		{
			MoveToPoint(WalkPoints[WalkIndex]);
		}
		return;
	}
	if (bArrived || bMoveFailed || !bMoving)
	{
		WalkIndex = (WalkIndex + 1) % WalkPoints.Num();
		DawnRouteIndex = WalkIndex;
		MoveToPoint(WalkPoints[WalkIndex]);
	}
}

void ANDMonsterAIController::EnterState(ENDMonsterState NewState)
{
	if (NewState == State)
	{
		return;
	}
	const ENDMonsterState Old = State;
	State = NewState;
	StateStartTime = Now();
	PauseUntil = 0.0;
	bLookingAround = false;
	if (NDMonsterBrain::IsInBed(NewState) || NewState == ENDMonsterState::WakingUp)
	{
		HaltMovement();
	}
	if (NewState == ENDMonsterState::DawnHunt)
	{
		WalkPoints.Reset();
	}
	if (NewState != ENDMonsterState::Chase)
	{
		bChaseFollowingActor = false;
	}
	Monster->SetMonsterState(NewState);
	ApplySpeed();
	UE_LOG(LogNDMonsterAI, Log, TEXT("%s: %s -> %s (sospecha %.0f)"), *Monster->GetName(), *NDMonsterBrain::GetStateLabel(Old), *NDMonsterBrain::GetStateLabel(NewState), Suspicion);
}

void ANDMonsterAIController::ApplySpeed()
{
	if (!Monster)
	{
		return;
	}
	const UNDMonsterDefinition& Def = GetDef();
	float Speed = Def.WalkSpeed;
	switch (State)
	{
	case ENDMonsterState::Investigate:
	case ENDMonsterState::Search:
	case ENDMonsterState::DawnHunt:
		Speed = Def.InvestigateSpeed;
		break;
	case ENDMonsterState::Chase:
		Speed = Def.ChaseSpeed;
		break;
	default:
		break;
	}
	Monster->GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void ANDMonsterAIController::UpdateSnore()
{
	FNDSnoreCycle Cycle = Monster->GetSnoreCycle();
	const double Time = Now();
	const bool bShouldSnore = NDMonsterBrain::IsSnoringState(State);
	const bool bSnoreStateChanged = bShouldSnore != Cycle.IsActive() || (bShouldSnore && (State == ENDMonsterState::DeepSleep) != (Cycle.Amplitude >= 1.f));
	if (Time >= Cycle.StartServerTime + Cycle.PeriodSeconds || (bSnoreStateChanged && !Cycle.IsInMaskWindow(Time)))
	{
		Monster->SetSnoreCycle(NDMonsterBrain::MakeSnoreCycle(GetDef(), State, Time, ++SnoreIndex, Rng));
	}
}

// ---------------------------------------------------------------------------
// Movimiento

void ANDMonsterAIController::MoveToPoint(const FVector& Goal, float Acceptance)
{
	bArrived = false;
	bMoveFailed = false;
	bChaseFollowingActor = false;
	MoveGoal = Goal;
	CurrentMoveId = FAIRequestID::InvalidRequest;
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, Acceptance, true, true, true, false, nullptr, true);
	CurrentMoveId = GetCurrentMoveRequestID();
	bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
	bArrived = Result == EPathFollowingRequestResult::AlreadyAtGoal;
	bMoveFailed = Result == EPathFollowingRequestResult::Failed;
	StuckCheckLocation = Monster ? Monster->GetActorLocation() : FVector::ZeroVector;
	StuckCheckTime = Now();
}

void ANDMonsterAIController::FollowActor(AActor* Goal)
{
	if (!Goal)
	{
		return;
	}
	bArrived = false;
	bMoveFailed = false;
	MoveGoal = Goal->GetActorLocation();
	CurrentMoveId = FAIRequestID::InvalidRequest;
	const EPathFollowingRequestResult::Type Result = MoveToActor(Goal, 60.f, true, true, false, nullptr, true);
	CurrentMoveId = GetCurrentMoveRequestID();
	bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
	bArrived = Result == EPathFollowingRequestResult::AlreadyAtGoal;
	bMoveFailed = Result == EPathFollowingRequestResult::Failed;
	bChaseFollowingActor = bMoving;
	StuckCheckLocation = Monster ? Monster->GetActorLocation() : FVector::ZeroVector;
	StuckCheckTime = Now();
}

void ANDMonsterAIController::HaltMovement()
{
	if (bMoving)
	{
		CurrentMoveId = FAIRequestID::InvalidRequest;
		StopMovement();
	}
	bMoving = false;
	bChaseFollowingActor = false;
}

void ANDMonsterAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	// Solo cuenta el movimiento actual (al pedir otro, el anterior termina como abortado).
	if (!CurrentMoveId.IsValid() || !(RequestID == CurrentMoveId))
	{
		return;
	}
	bMoving = false;
	bChaseFollowingActor = false;
	bArrived = Result.IsSuccess();
	bMoveFailed = !bArrived;
}

void ANDMonsterAIController::UpdateStuck()
{
	if (!bMoving)
	{
		StuckCheckLocation = Monster->GetActorLocation();
		StuckCheckTime = Now();
		return;
	}
	if (Now() - StuckCheckTime < NDMonsterAI::StuckSeconds)
	{
		return;
	}
	if (FVector::Dist2D(StuckCheckLocation, Monster->GetActorLocation()) < NDMonsterAI::StuckDistance && GrabStartTime < 0.0)
	{
		UE_LOG(LogNDMonsterAI, Log, TEXT("%s: atascado, cambia de plan."), *Monster->GetName());
		HaltMovement();
		bMoveFailed = true;
	}
	StuckCheckLocation = Monster->GetActorLocation();
	StuckCheckTime = Now();
}

void ANDMonsterAIController::FaceTowards(const FVector& Location, float DeltaSeconds, float DegreesPerSecond)
{
	if (!Monster || Monster->IsInBed())
	{
		return;
	}
	const FVector Direction = Location - Monster->GetActorLocation();
	if (Direction.SizeSquared2D() < 1.f)
	{
		return;
	}
	const FRotator Current = Monster->GetActorRotation();
	const FRotator Target(0.f, Direction.Rotation().Yaw, 0.f);
	Monster->SetActorRotation(FMath::RInterpConstantTo(Current, Target, DeltaSeconds, DegreesPerSecond));
}

// ---------------------------------------------------------------------------
// Depuración

void ANDMonsterAIController::DebugWake()
{
	Suspicion = 100.f;
	LastNoiseLocation = Monster ? Monster->GetActorLocation() + Monster->GetActorForwardVector() * 500.f : FVector::ZeroVector;
	LastEvidenceTime = Now();
	if (NDMonsterBrain::IsInBed(State))
	{
		InvestigateLocation = LastNoiseLocation;
		AfterWakeState = ENDMonsterState::Investigate;
		EnterState(ENDMonsterState::WakingUp);
	}
}

void ANDMonsterAIController::DebugSleep()
{
	if (!Monster)
	{
		return;
	}
	ChaseTarget.Reset();
	GrabStartTime = -1.0;
	Monster->SetGrabTelegraph(false);
	Suspicion = 0.f;
	const float HalfHeight = Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Monster->SetActorLocation(Monster->GetBedTransform().GetLocation() + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	LieDown();
	EnterState(ENDMonsterState::DeepSleep);
}

void ANDMonsterAIController::DebugSetSuspicion(float Value)
{
	Suspicion = FMath::Clamp(Value, 0.f, 100.f);
	if (Monster)
	{
		Monster->SetSuspicion(Suspicion);
	}
}

void ANDMonsterAIController::DebugNightWalk()
{
	const UNDMonsterDefinition& Def = GetDef();
	NextNightWalkTime = Now() + Rng.FRandRange(Def.NightWalkIntervalMin, FMath::Max(Def.NightWalkIntervalMin, Def.NightWalkIntervalMax));
	TArray<FVector> Route;
	if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this))
	{
		Monsters->GetPatrolRoute(Route);
	}
	// El recorrido sale del dormitorio: se toma un tramo seguido de 2-4 salas, sin repetir la cama.
	TArray<FVector> Points;
	const int32 Rooms = Rng.RandRange(Def.NightWalkRoomsMin, FMath::Max(Def.NightWalkRoomsMin, Def.NightWalkRoomsMax));
	if (Route.Num() > 2)
	{
		const int32 Start = Rng.RandRange(1, Route.Num() - 2);
		for (int32 Index = Start; Index < Route.Num() - 1 && Points.Num() < Rooms; ++Index)
		{
			if (FVector::Dist2D(Route[Index], Monster->GetBedTransform().GetLocation()) > 300.f)
			{
				Points.Add(Route[Index]);
			}
		}
	}
	else if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(this))
	{
		Monsters->GetSearchPoints(Monster->GetActorLocation(), Rooms, Points);
	}
	if (Points.IsEmpty())
	{
		return;
	}
	WalkPoints = MoveTemp(Points);
	WalkIndex = 0;
	PauseUntil = 0.0;
	AfterWakeState = ENDMonsterState::NightWalk;
	if (NDMonsterBrain::IsInBed(State))
	{
		EnterState(ENDMonsterState::WakingUp);
	}
	else
	{
		EnterState(ENDMonsterState::NightWalk);
	}
	UE_LOG(LogNDMonsterAI, Log, TEXT("%s: ronda nocturna por %d salas."), *Monster->GetName(), WalkPoints.Num());
}

FString ANDMonsterAIController::GetDebugStatus() const
{
	if (!Monster)
	{
		return TEXT("sin criatura");
	}
	const APawn* Target = ChaseTarget.Get();
	return FString::Printf(TEXT("%s | %s | sospecha %.0f | vista %.2f | ronquido %s | objetivo %s | moviendo %d | amanecer %d"),
		*Monster->GetName(), *NDMonsterBrain::GetStateLabel(State), Suspicion, Monster->GetSightAlert(),
		Monster->IsSnoreMaskActive() ? TEXT("SI") : TEXT("no"), Target ? *Target->GetName() : TEXT("-"), bMoving ? 1 : 0, bDawn ? 1 : 0);
}

void ANDMonsterAIController::DrawDebug() const
{
	const UWorld* World = GetWorld();
	const FVector Head = Monster->GetActorLocation() + FVector(0.f, 0.f, 330.f);
	DrawDebugString(World, Head, FString::Printf(TEXT("%s  %.0f"), *NDMonsterBrain::GetStateLabel(State), Suspicion), nullptr, FColor::White, 0.12f, true, 1.4f);
	for (const FNDHeardNoise& Noise : RecentNoises)
	{
		if (Now() - Noise.ServerTime < 3.0)
		{
			DrawDebugSphere(World, Noise.Location, 20.f + Noise.SuspicionGain * 8.f, 8, FColor::Cyan, false, 0.12f);
			DrawDebugLine(World, Noise.Location, Monster->GetActorLocation(), FColor(0, 150, 150), false, 0.12f);
		}
	}
	if (bMoving)
	{
		DrawDebugLine(World, Monster->GetActorLocation(), MoveGoal, FColor::Orange, false, 0.12f, 0, 3.f);
	}
	if (!Monster->IsInBed())
	{
		const UNDMonsterDefinition& Def = GetDef();
		DrawDebugCone(World, GetEyeLocation(), Monster->GetActorForwardVector(), Def.SightRange, FMath::DegreesToRadians(Def.SightHalfAngle), FMath::DegreesToRadians(15.f), 12, FColor::Yellow, false, 0.12f);
	}
	if (ChaseTarget.IsValid())
	{
		DrawDebugSphere(World, LastSeenLocation, 40.f, 8, FColor::Red, false, 0.12f);
	}
}
