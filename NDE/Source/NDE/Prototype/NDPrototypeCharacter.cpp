#include "Prototype/NDPrototypeCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Contracts/NDContractDefinition.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Expedition/NDExpeditionPlayerComponent.h"
#include "Expedition/NDNightClockComponent.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Tools/NDLoadoutComponent.h"
#include "Tools/NDToolDefinition.h"

namespace NDPrototype
{
	const TCHAR* PhaseText(ENDExpeditionPhase Phase)
	{
		switch (Phase)
		{
		case ENDExpeditionPhase::Insertion:       return TEXT("Inserci\u00F3n");
		case ENDExpeditionPhase::Infiltration:    return TEXT("Infiltraci\u00F3n");
		case ENDExpeditionPhase::ExtractionReady: return TEXT("Extracci\u00F3n disponible");
		case ENDExpeditionPhase::DawnEscapeGrace: return TEXT("\u00A1AMANECI\u00D3! Huida final");
		case ENDExpeditionPhase::Settlement:      return TEXT("Liquidaci\u00F3n");
		default:                                  return TEXT("?");
		}
	}

	const TCHAR* NightText(ENDNightPhase Phase)
	{
		switch (Phase)
		{
		case ENDNightPhase::NotStarted: return TEXT("sin empezar");
		case ENDNightPhase::DeepNight:  return TEXT("noche profunda");
		case ENDNightPhase::Madrugada:  return TEXT("madrugada");
		case ENDNightPhase::PreDawn:    return TEXT("pre-amanecer");
		case ENDNightPhase::DawnGrace:  return TEXT("amanecer");
		case ENDNightPhase::DayEnded:   return TEXT("fin del d\u00EDa");
		default:                        return TEXT("?");
		}
	}

	const TCHAR* ConditionText(ENDLootCondition Condition)
	{
		switch (Condition)
		{
		case ENDLootCondition::Intact:  return TEXT("intacto");
		case ENDLootCondition::Damaged: return TEXT("da\u00F1ado");
		case ENDLootCondition::Broken:  return TEXT("roto");
		default:                        return TEXT("?");
		}
	}

	const TCHAR* OutcomeText(ENDExpeditionOutcome Outcome)
	{
		switch (Outcome)
		{
		case ENDExpeditionOutcome::Perfect:           return TEXT("\u00C9XITO PERFECTO");
		case ENDExpeditionOutcome::Success:           return TEXT("\u00C9XITO");
		case ENDExpeditionOutcome::PartialExtraction: return TEXT("EXTRACCI\u00D3N PARCIAL");
		case ENDExpeditionOutcome::Failure:           return TEXT("FRACASO");
		default:                                      return TEXT("-");
		}
	}

	const TCHAR* StatusText(ENDParticipantStatus Status)
	{
		switch (Status)
		{
		case ENDParticipantStatus::Active:       return TEXT("activo");
		case ENDParticipantStatus::Captured:     return TEXT("capturado");
		case ENDParticipantStatus::Extracted:    return TEXT("extra\u00EDdo");
		case ENDParticipantStatus::LeftBehind:   return TEXT("se qued\u00F3 atr\u00E1s");
		case ENDParticipantStatus::Disconnected: return TEXT("desconectado");
		default:                                 return TEXT("?");
		}
	}

	FString FormatClock(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(FMath::Max(0.f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	FString ToolName(const UNDToolDefinition* Tool)
	{
		return Tool ? Tool->DisplayName.ToString() : TEXT("(vac\u00EDo)");
	}
}

ANDPrototypeCharacter::ANDPrototypeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Un Desvelado mide unos 45-60 cm (GDD 2.3).
	GetCapsuleComponent()->InitCapsuleSize(22.f, 34.f);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->JumpZVelocity = 560.f;
	Movement->NavAgentProps.bCanCrouch = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->SocketOffset = FVector(0.f, 35.f, 45.f);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(GetRootComponent());
	Flashlight->SetRelativeLocation(FVector(15.f, 0.f, 20.f));
	Flashlight->SetIntensity(5000.f);
	Flashlight->SetAttenuationRadius(1500.f);
	Flashlight->SetOuterConeAngle(25.f);
	Flashlight->SetVisibility(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.68f));

	NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
	NoseMesh->SetupAttachment(GetRootComponent());
	NoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NoseMesh->SetRelativeLocation(FVector(22.f, 0.f, 15.f));
	NoseMesh->SetRelativeScale3D(FVector(0.12f, 0.08f, 0.08f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (CubeMesh.Succeeded())
	{
		NoseMesh->SetStaticMesh(CubeMesh.Object);
	}

	Carry = CreateDefaultSubobject<UNDCarryComponent>(TEXT("Carry"));
	Loadout = CreateDefaultSubobject<UNDLoadoutComponent>(TEXT("Loadout"));
	ExpeditionPlayer = CreateDefaultSubobject<UNDExpeditionPlayerComponent>(TEXT("ExpeditionPlayer"));
}

void ANDPrototypeCharacter::BeginPlay()
{
	Super::BeginPlay();

	Loadout->OnFlashlightChanged.AddDynamic(this, &ANDPrototypeCharacter::HandleFlashlightChanged);
}

void ANDPrototypeCharacter::HandleFlashlightChanged(bool bOn)
{
	Flashlight->SetVisibility(bOn);
}

// ---------------------------------------------------------------------------
// Entrada

UInputAction* ANDPrototypeCharacter::MakeAction(FName Name, bool bAxis2D)
{
	UInputAction* Action = NewObject<UInputAction>(this, Name);
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	Actions.Add(Name, Action);
	return Action;
}

void ANDPrototypeCharacter::EnsureInputObjects()
{
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_NDPrototype"));

	UInputAction* Move = MakeAction(TEXT("Move"), true);
	UInputAction* Look = MakeAction(TEXT("Look"), true);

	auto MapWithModifiers = [this](UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};

	MapWithModifiers(Move, EKeys::W, true, false);
	MapWithModifiers(Move, EKeys::S, true, true);
	MapWithModifiers(Move, EKeys::A, false, true);
	MapWithModifiers(Move, EKeys::D, false, false);
	MappingContext->MapKey(Move, EKeys::Gamepad_Left2D);
	MappingContext->MapKey(Look, EKeys::Mouse2D);
	MappingContext->MapKey(Look, EKeys::Gamepad_Right2D);

	struct FButton { const TCHAR* Name; FKey Key; FKey PadKey; };
	const FButton Buttons[] = {
		{ TEXT("Jump"),       EKeys::SpaceBar,           EKeys::Gamepad_FaceButton_Bottom },
		{ TEXT("Sprint"),     EKeys::LeftShift,          EKeys::Gamepad_LeftThumbstick },
		{ TEXT("Crouch"),     EKeys::LeftControl,        EKeys::Gamepad_FaceButton_Right },
		{ TEXT("Interact"),   EKeys::E,                  EKeys::Gamepad_FaceButton_Left },
		{ TEXT("Drop"),       EKeys::G,                  EKeys::Gamepad_FaceButton_Top },
		{ TEXT("Throw"),      EKeys::LeftMouseButton,    EKeys::Gamepad_RightTrigger },
		{ TEXT("UseTool"),    EKeys::RightMouseButton,   EKeys::Gamepad_LeftTrigger },
		{ TEXT("Consumable"), EKeys::Q,                  EKeys::Gamepad_RightShoulder },
		{ TEXT("Ping"),       EKeys::MiddleMouseButton,  EKeys::Gamepad_DPad_Up },
		{ TEXT("Slot1"),      EKeys::One,                EKeys::Gamepad_DPad_Left },
		{ TEXT("Slot2"),      EKeys::Two,                EKeys::Gamepad_DPad_Right },
		{ TEXT("VoteNo"),     EKeys::N,                  EKeys::Gamepad_DPad_Down },
	};
	for (const FButton& Button : Buttons)
	{
		UInputAction* Action = MakeAction(Button.Name, false);
		MappingContext->MapKey(Action, Button.Key);
		MappingContext->MapKey(Action, Button.PadKey);
	}
}

void ANDPrototypeCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	EnsureInputObjects();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void ANDPrototypeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputObjects();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("NDPrototypeCharacter necesita Enhanced Input como clase de entrada por defecto."));
		return;
	}

	Input->BindAction(Actions[TEXT("Move")], ETriggerEvent::Triggered, this, &ANDPrototypeCharacter::OnMove);
	Input->BindAction(Actions[TEXT("Look")], ETriggerEvent::Triggered, this, &ANDPrototypeCharacter::OnLook);
	Input->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(Actions[TEXT("Sprint")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnSprintStarted);
	Input->BindAction(Actions[TEXT("Sprint")], ETriggerEvent::Completed, this, &ANDPrototypeCharacter::OnSprintEnded);
	Input->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnCrouchStarted);
	Input->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Completed, this, &ANDPrototypeCharacter::OnCrouchEnded);
	Input->BindAction(Actions[TEXT("Interact")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnInteract);
	Input->BindAction(Actions[TEXT("Drop")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnDrop);
	Input->BindAction(Actions[TEXT("Throw")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnThrowStarted);
	Input->BindAction(Actions[TEXT("Throw")], ETriggerEvent::Completed, this, &ANDPrototypeCharacter::OnThrowReleased);
	Input->BindAction(Actions[TEXT("UseTool")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnUseTool);
	Input->BindAction(Actions[TEXT("Consumable")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnUseConsumable);
	Input->BindAction(Actions[TEXT("Ping")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnPing);
	Input->BindAction(Actions[TEXT("Slot1")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnSelectSlot1);
	Input->BindAction(Actions[TEXT("Slot2")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnSelectSlot2);
	Input->BindAction(Actions[TEXT("VoteNo")], ETriggerEvent::Started, this, &ANDPrototypeCharacter::OnVoteNo);
}

void ANDPrototypeCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ANDPrototypeCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ANDPrototypeCharacter::OnSprintStarted()
{
	bWantsSprint = true;
	ServerSetSprinting(true);
}

void ANDPrototypeCharacter::OnSprintEnded()
{
	bWantsSprint = false;
	ServerSetSprinting(false);
}

void ANDPrototypeCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bWantsSprint = bNewSprinting;
}

void ANDPrototypeCharacter::OnCrouchStarted()
{
	Crouch();
}

void ANDPrototypeCharacter::OnCrouchEnded()
{
	UnCrouch();
}

void ANDPrototypeCharacter::OnInteract()
{
	if (ExpeditionPlayer->IsInsideVan())
	{
		ExpeditionPlayer->RequestVanInteraction();
		return;
	}
	if (ANDLootActor* Target = Carry->FindBestLootTarget())
	{
		Carry->RequestGrab(Target);
	}
}

void ANDPrototypeCharacter::OnDrop()
{
	Carry->RequestDrop();
}

void ANDPrototypeCharacter::OnThrowStarted()
{
	ThrowStartTime = GetWorld()->GetTimeSeconds();
}

void ANDPrototypeCharacter::OnThrowReleased()
{
	if (!Carry->GetHeldLoot() || ThrowStartTime < 0.0)
	{
		return;
	}
	const float Charge = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ThrowStartTime), 0.f, 1.f);
	ThrowStartTime = -1.0;

	FRotator Aim = GetControlRotation();
	Aim.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch) + 10.f, -30.f, 60.f);
	Carry->RequestThrow(Aim.Vector(), Charge);
}

void ANDPrototypeCharacter::OnUseTool()
{
	Loadout->RequestUseActiveTool(GetAimPoint());
}

void ANDPrototypeCharacter::OnUseConsumable()
{
	Loadout->RequestUseConsumable(GetAimPoint());
}

void ANDPrototypeCharacter::OnPing()
{
	Loadout->RequestPing(GetAimPoint());
}

void ANDPrototypeCharacter::OnSelectSlot1()
{
	Loadout->RequestSelectSlot(0);
}

void ANDPrototypeCharacter::OnSelectSlot2()
{
	Loadout->RequestSelectSlot(1);
}

void ANDPrototypeCharacter::OnVoteNo()
{
	ExpeditionPlayer->RequestEscapeVote(false);
}

FVector ANDPrototypeCharacter::GetAimPoint() const
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 1500.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDPrototypeAim), false, this);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

// ---------------------------------------------------------------------------
// Tick

void ANDPrototypeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() || IsLocallyControlled())
	{
		UpdateMovementSpeed();
	}
	if (IsLocallyControlled() && bShowDebugHud)
	{
		DrawDebugHud();
	}
}

void ANDPrototypeCharacter::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Multiplier = Carry->GetSpeedMultiplier();
	const bool bSprinting = bWantsSprint && Carry->CanSprint() && !bIsCrouched;
	Movement->MaxWalkSpeed = (bSprinting ? SprintSpeed : WalkSpeed) * Multiplier;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;
}

void ANDPrototypeCharacter::DrawDebugHud() const
{
	if (!GEngine)
	{
		return;
	}
	using namespace NDPrototype;

	const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	const UNDNightClockComponent* Clock = ANDExpeditionDirector::GetNightClock(this);
	if (!Expedition || !Clock)
	{
		GEngine->AddOnScreenDebugMessage(9100, 0.5f, FColor::Red, TEXT("No hay ANDExpeditionDirector en el nivel."));
		return;
	}

	TArray<FString> Lines;
	const UNDContractDefinition* Contract = Expedition->Contract;
	Lines.Add(FString::Printf(TEXT("Contrato: %s (tier %d)"),
		Contract ? *Contract->DisplayName.ToString() : TEXT("ninguno"), Contract ? Contract->DifficultyTier : 0));

	if (Expedition->GetPhase() == ENDExpeditionPhase::Settlement)
	{
		const FNDRunSummary& Summary = Expedition->GetRunSummary();
		Lines.Add(FString::Printf(TEXT("RESULTADO: %s"), OutcomeText(Summary.Outcome)));
		Lines.Add(FString::Printf(TEXT("Bot\u00EDn: %d puntos (valor vendido %d) | cuota %d | objetivo %s | secundario %s"),
			Summary.LootPointsDeposited, Summary.LootSoldValue, Summary.Quota,
			Summary.bObjectiveMet ? TEXT("s\u00ED") : TEXT("no"), Summary.bSecondaryMet ? TEXT("s\u00ED") : TEXT("no")));
		for (const FNDPlayerSettlement& Player : Summary.Players)
		{
			Lines.Add(FString::Printf(TEXT("  %s: %s, +%d monedas, +%d reputaci\u00F3n"),
				*Player.PlayerName, StatusText(Player.FinalStatus), Player.Reward.Coins, Player.Reward.Reputation));
		}
	}
	else
	{
		FString TimeLine;
		if (Expedition->GetPhase() == ENDExpeditionPhase::Insertion)
		{
			TimeLine = FString::Printf(TEXT("Entra en la casa: la noche empieza en %s"), *FormatClock(Expedition->GetInsertionSecondsRemaining()));
		}
		else if (Clock->IsDawnTriggered())
		{
			TimeLine = FString::Printf(TEXT("\u00A1SE DESPERT\u00D3! Quedan %s para escapar"), *FormatClock(Clock->GetGraceSecondsRemaining()));
		}
		else
		{
			TimeLine = FString::Printf(TEXT("Amanecer en %s (%s)"), *FormatClock(Clock->GetSecondsUntilDawn()), NightText(Clock->GetPhase()));
		}
		Lines.Add(FString::Printf(TEXT("%s | %s"), PhaseText(Expedition->GetPhase()), *TimeLine));

		Lines.Add(FString::Printf(TEXT("Bot\u00EDn depositado: %d / %d puntos | Objetivo: %s"),
			Expedition->GetLootPoints(), Expedition->GetQuota(), Expedition->IsObjectiveMet() ? TEXT("CUMPLIDO") : TEXT("pendiente")));

		if (const ANDLootActor* Held = Carry->GetHeldLoot())
		{
			const UNDLootDefinition* Definition = Held->GetDefinition();
			Lines.Add(FString::Printf(TEXT("Llevas: %s (%s) | portadores %d%s"),
				Definition ? *Definition->DisplayName.ToString() : TEXT("?"), ConditionText(Held->GetCondition()),
				Held->GetCarrierCount(), Held->HasEnoughCarriers() ? TEXT("") : TEXT(" | necesitas ayuda o el carrito")));
		}
		Lines.Add(FString::Printf(TEXT("Mochila: %d / %d"), Carry->GetPocketLoot().Num(), Carry->PocketSlots));

		const TArray<FNDToolSlot>& Slots = Loadout->GetActiveSlots();
		const FNDToolSlot& Consumable = Loadout->GetConsumableSlot();
		Lines.Add(FString::Printf(TEXT("Herramientas: %s%s  %s%s | Consumible (Q): %s x%d"),
			Loadout->GetSelectedSlot() == 0 ? TEXT("> ") : TEXT(""), Slots.IsValidIndex(0) ? *ToolName(Slots[0].Tool) : TEXT("-"),
			Loadout->GetSelectedSlot() == 1 ? TEXT("> ") : TEXT(""), Slots.IsValidIndex(1) ? *ToolName(Slots[1].Tool) : TEXT("-"),
			*ToolName(Consumable.Tool), FMath::Max(0, Consumable.Charges)));

		if (Expedition->IsEscapeVoteActive())
		{
			Lines.Add(FString::Printf(TEXT("VOTACI\u00D3N DE ESCAPE: %d s\u00ED / %d no (%s). E vota s\u00ED en la camioneta, N vota no."),
				Expedition->GetVotesYes(), Expedition->GetVotesNo(), *FormatClock(Expedition->GetEscapeVoteSecondsRemaining())));
		}
		if (Expedition->IsEvacuationActive())
		{
			Lines.Add(FString::Printf(TEXT("EVACUACI\u00D3N EN %s: \u00A1a la camioneta!"), *FormatClock(Expedition->GetEvacuationSecondsRemaining())));
		}

		if (ExpeditionPlayer->IsInsideVan())
		{
			Lines.Add(Expedition->GetPhase() == ENDExpeditionPhase::DawnEscapeGrace
				? TEXT("En la camioneta: E para huir ya")
				: TEXT("En la camioneta: E para proponer la retirada"));
		}
		else if (const ANDLootActor* Target = Carry->FindBestLootTarget())
		{
			const UNDLootDefinition* Definition = Target->GetDefinition();
			Lines.Add(FString::Printf(TEXT("Delante: %s (%d pts, %s). E para coger"),
				Definition ? *Definition->DisplayName.ToString() : TEXT("?"), Target->GetCurrentLootPoints(), ConditionText(Target->GetCondition())));
		}
	}

	GEngine->AddOnScreenDebugMessage(9100, 0.5f, FColor::White, FString::Join(Lines, TEXT("\n")));
}
