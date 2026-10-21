// No Despiertes Eso

#include "Players/NDCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/NDLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Expedition/NDExpeditionDebugHud.h"
#include "Expedition/NDExpeditionPlayerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Net/UnrealNetwork.h"
#include "Tools/NDLoadoutComponent.h"
#include "Tools/NDToolDefinition.h"
#include "UObject/ConstructorHelpers.h"

namespace NDCharacter
{
	const TCHAR* DesveladoClassPath = TEXT("/Game/_NDE_/Characters/BP_Desvelado.BP_Desvelado_C");

	TSoftObjectPtr<UNDToolDefinition> ToolAsset(const TCHAR* Id)
	{
		return TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(FString::Printf(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_%s.DA_Tool_%s"), Id, Id)));
	}
}

ANDCharacter::ANDCharacter()
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

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetRootComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.68f));

	PlaceholderNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderNose"));
	PlaceholderNose->SetupAttachment(GetRootComponent());
	PlaceholderNose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderNose->SetRelativeLocation(FVector(22.f, 0.f, 15.f));
	PlaceholderNose->SetRelativeScale3D(FVector(0.12f, 0.08f, 0.08f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CylinderMesh.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
	}
	if (CubeMesh.Succeeded())
	{
		PlaceholderNose->SetStaticMesh(CubeMesh.Object);
	}

	Carry = CreateDefaultSubobject<UNDCarryComponent>(TEXT("Carry"));
	Loadout = CreateDefaultSubobject<UNDLoadoutComponent>(TEXT("Loadout"));
	ExpeditionPlayer = CreateDefaultSubobject<UNDExpeditionPlayerComponent>(TEXT("ExpeditionPlayer"));

	TestToolA = NDCharacter::ToolAsset(TEXT("T03"));
	TestToolB = NDCharacter::ToolAsset(TEXT("T07"));
	TestPingTool = NDCharacter::ToolAsset(TEXT("T02"));
	TestConsumables.Add(NDCharacter::ToolAsset(TEXT("T17")));
	TestConsumables.Add(NDCharacter::ToolAsset(TEXT("T18")));
}

TSubclassOf<APawn> ANDCharacter::GetDesveladoClass()
{
	static TSoftClassPtr<APawn> DesveladoClass{FSoftObjectPath(NDCharacter::DesveladoClassPath)};
	if (UClass* Loaded = DesveladoClass.LoadSynchronous())
	{
		return Loaded;
	}
	UE_LOG(LogNDNet, Warning, TEXT("No se encontro BP_Desvelado; se usa ANDCharacter."));
	return ANDCharacter::StaticClass();
}

void ANDCharacter::BeginPlay()
{
	Super::BeginPlay();

	Loadout->OnFlashlightChanged.AddDynamic(this, &ANDCharacter::HandleFlashlightChanged);
	UpdateMovementSpeed();
}

void ANDCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Mientras no exista el loadout de La Madriguera, cada jugador sale con el equipo de prueba.
	static int32 NextConsumable = 0;
	UNDToolDefinition* Consumable = TestConsumables.Num() > 0
		? TestConsumables[NextConsumable++ % TestConsumables.Num()].LoadSynchronous()
		: nullptr;
	Loadout->PingTool = TestPingTool.LoadSynchronous();
	Loadout->SetLoadout(TestToolA.LoadSynchronous(), TestToolB.LoadSynchronous(), Consumable, 0);
}

void ANDCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANDCharacter, bIsSprinting, COND_SkipOwner);
}

void ANDCharacter::HandleFlashlightChanged(bool bOn)
{
	Flashlight->SetVisibility(bOn);
}

// ---------------------------------------------------------------------------
// Entrada

UInputAction* ANDCharacter::MakeAction(FName Name, bool bAxis2D)
{
	UInputAction* Action = NewObject<UInputAction>(this, Name);
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	Actions.Add(Name, Action);
	return Action;
}

void ANDCharacter::EnsureInputObjects()
{
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_NDCharacter"));

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
		{ TEXT("Jump"),       EKeys::SpaceBar,          EKeys::Gamepad_FaceButton_Bottom },
		{ TEXT("Sprint"),     EKeys::LeftShift,         EKeys::Gamepad_LeftThumbstick },
		{ TEXT("Crouch"),     EKeys::LeftControl,       EKeys::Gamepad_FaceButton_Right },
		{ TEXT("Interact"),   EKeys::E,                 EKeys::Gamepad_FaceButton_Left },
		{ TEXT("Drop"),       EKeys::G,                 EKeys::Gamepad_FaceButton_Top },
		{ TEXT("Throw"),      EKeys::LeftMouseButton,   EKeys::Gamepad_RightTrigger },
		{ TEXT("UseTool"),    EKeys::RightMouseButton,  EKeys::Gamepad_LeftTrigger },
		{ TEXT("Consumable"), EKeys::Q,                 EKeys::Gamepad_RightShoulder },
		{ TEXT("Ping"),       EKeys::MiddleMouseButton, EKeys::Gamepad_DPad_Up },
		{ TEXT("Slot1"),      EKeys::One,               EKeys::Gamepad_DPad_Left },
		{ TEXT("Slot2"),      EKeys::Two,               EKeys::Gamepad_DPad_Right },
		{ TEXT("VoteNo"),     EKeys::N,                 EKeys::Gamepad_DPad_Down },
	};
	for (const FButton& Button : Buttons)
	{
		UInputAction* Action = MakeAction(Button.Name, false);
		MappingContext->MapKey(Action, Button.Key);
		MappingContext->MapKey(Action, Button.PadKey);
	}
}

void ANDCharacter::PawnClientRestart()
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

void ANDCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputObjects();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogNDNet, Error, TEXT("ANDCharacter necesita Enhanced Input como clase de entrada por defecto."));
		return;
	}

	Input->BindAction(Actions[TEXT("Move")], ETriggerEvent::Triggered, this, &ANDCharacter::OnMove);
	Input->BindAction(Actions[TEXT("Look")], ETriggerEvent::Triggered, this, &ANDCharacter::OnLook);
	Input->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(Actions[TEXT("Sprint")], ETriggerEvent::Started, this, &ANDCharacter::OnSprintStarted);
	Input->BindAction(Actions[TEXT("Sprint")], ETriggerEvent::Completed, this, &ANDCharacter::OnSprintEnded);
	Input->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Started, this, &ANDCharacter::OnCrouchStarted);
	Input->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Completed, this, &ANDCharacter::OnCrouchEnded);
	Input->BindAction(Actions[TEXT("Interact")], ETriggerEvent::Started, this, &ANDCharacter::OnInteract);
	Input->BindAction(Actions[TEXT("Drop")], ETriggerEvent::Started, this, &ANDCharacter::OnDrop);
	Input->BindAction(Actions[TEXT("Throw")], ETriggerEvent::Started, this, &ANDCharacter::OnThrowStarted);
	Input->BindAction(Actions[TEXT("Throw")], ETriggerEvent::Completed, this, &ANDCharacter::OnThrowReleased);
	Input->BindAction(Actions[TEXT("UseTool")], ETriggerEvent::Started, this, &ANDCharacter::OnUseTool);
	Input->BindAction(Actions[TEXT("Consumable")], ETriggerEvent::Started, this, &ANDCharacter::OnUseConsumable);
	Input->BindAction(Actions[TEXT("Ping")], ETriggerEvent::Started, this, &ANDCharacter::OnPing);
	Input->BindAction(Actions[TEXT("Slot1")], ETriggerEvent::Started, this, &ANDCharacter::OnSelectSlot1);
	Input->BindAction(Actions[TEXT("Slot2")], ETriggerEvent::Started, this, &ANDCharacter::OnSelectSlot2);
	Input->BindAction(Actions[TEXT("VoteNo")], ETriggerEvent::Started, this, &ANDCharacter::OnVoteNo);
}

void ANDCharacter::OnMove(const FInputActionValue& Value)
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

void ANDCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ANDCharacter::OnSprintStarted()
{
	SetSprinting(true);
}

void ANDCharacter::OnSprintEnded()
{
	SetSprinting(false);
}

void ANDCharacter::OnCrouchStarted()
{
	Crouch();
}

void ANDCharacter::OnCrouchEnded()
{
	UnCrouch();
}

void ANDCharacter::OnInteract()
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

void ANDCharacter::OnDrop()
{
	Carry->RequestDrop();
}

void ANDCharacter::OnThrowStarted()
{
	ThrowStartTime = GetWorld()->GetTimeSeconds();
}

void ANDCharacter::OnThrowReleased()
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

void ANDCharacter::OnUseTool()
{
	Loadout->RequestUseActiveTool(GetAimPoint());
}

void ANDCharacter::OnUseConsumable()
{
	Loadout->RequestUseConsumable(GetAimPoint());
}

void ANDCharacter::OnPing()
{
	Loadout->RequestPing(GetAimPoint());
}

void ANDCharacter::OnSelectSlot1()
{
	Loadout->RequestSelectSlot(0);
}

void ANDCharacter::OnSelectSlot2()
{
	Loadout->RequestSelectSlot(1);
}

void ANDCharacter::OnVoteNo()
{
	ExpeditionPlayer->RequestEscapeVote(false);
}

FVector ANDCharacter::GetAimPoint() const
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 1500.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDCharacterAim), false, this);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

// ---------------------------------------------------------------------------
// Movimiento

void ANDCharacter::SetSprinting(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	UpdateMovementSpeed();
	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void ANDCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	UpdateMovementSpeed();
}

void ANDCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// El peso de la carga cambia sin avisar; servidor y dueño recalculan la velocidad cada frame.
	if (HasAuthority() || IsLocallyControlled())
	{
		UpdateMovementSpeed();
	}
	if (IsLocallyControlled() && bShowDebugHud)
	{
		NDExpeditionDebugHud::Draw(this, *Carry, *Loadout, *ExpeditionPlayer);
	}
}

void ANDCharacter::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Multiplier = Carry->GetSpeedMultiplier();
	const bool bSprinting = bIsSprinting && Carry->CanSprint() && !bIsCrouched;
	Movement->MaxWalkSpeed = (bSprinting ? SprintSpeed : WalkSpeed) * Multiplier;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;
}
