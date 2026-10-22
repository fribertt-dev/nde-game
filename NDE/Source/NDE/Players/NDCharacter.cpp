// No Despiertes Eso

#include "Players/NDCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/NDLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "Networking/NDLobbyGameState.h"
#include "Players/NDScaleSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace NDCharacter
{
	const TCHAR* DesveladoClassPath = TEXT("/Game/_NDE_/Characters/BP_Desvelado.BP_Desvelado_C");

	/** Nariz del placeholder respecto al centro de la cápsula, mirando al frente. */
	const FVector NoseRestLocation(22.f, 0.f, 15.f);

	TAutoConsoleVariable<int32> CVarFirstPerson(
		TEXT("nde.Camera.FirstPerson"),
		-1,
		TEXT("Cámara del Desvelado: 1 = primera persona (en los ojos), 0 = tercera persona, -1 = automático (primera persona salvo en La Madriguera)."));

	TAutoConsoleVariable<int32> CVarHeadBob(
		TEXT("nde.Camera.HeadBob"),
		1,
		TEXT("Balanceo de cabeza al andar en primera persona: 1 = sí (si está activado en NDE Scale), 0 = no."));
}

ANDCharacter::ANDCharacter()
{
	// Un Desvelado mide unos 45-60 cm (GDD 2.3); las medidas salen de UNDScaleSettings.
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->NavAgentProps.bCanCrouch = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = UNDScaleSettings::Get()->ThirdPersonArmLength;
	CameraBoom->SocketOffset = UNDScaleSettings::Get()->ThirdPersonOffset;
	CameraBoom->bUsePawnControlRotation = true;
	// Cámara y nariz se actualizan cada frame (modo de cámara, balanceo, cabeceo).
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetRootComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Tamaño en ApplyScaleSettings, igual que la cápsula.

	PlaceholderNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderNose"));
	PlaceholderNose->SetupAttachment(GetRootComponent());
	PlaceholderNose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderNose->SetRelativeLocation(NDCharacter::NoseRestLocation);
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
	ApplyScaleSettings();
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

void ANDCharacter::ApplyScaleSettings()
{
	const UNDScaleSettings* Scale = UNDScaleSettings::Get();
	GetCapsuleComponent()->InitCapsuleSize(Scale->CapsuleRadius, Scale->CapsuleHalfHeight);
	if (PlaceholderBody)
	{
		// El cilindro del motor mide 100 x 100 cm: ocupa justo la cápsula.
		PlaceholderBody->SetRelativeScale3D(FVector(Scale->CapsuleRadius * 2.f, Scale->CapsuleRadius * 2.f, Scale->CapsuleHalfHeight * 2.f) / 100.f);
	}
	WalkSpeed = Scale->WalkSpeed;
	SprintSpeed = Scale->SprintSpeed;
	CrouchSpeed = Scale->CrouchSpeed;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxStepHeight = Scale->MaxStepHeight;
	Movement->JumpZVelocity = Scale->JumpZVelocity;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->SetCrouchedHalfHeight(Scale->CrouchedHalfHeight);
	UpdateMovementSpeed();
}

void ANDCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// También en partida: así manda lo que haya en NDE Scale aunque el Blueprint guarde valores antiguos.
	ApplyScaleSettings();
}

void ANDCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateMovementSpeed();
	// El servidor arranca con el modo por defecto; el dueño le dice luego el suyo (ServerSetFirstPerson).
	if (HasAuthority() && !IsLocallyControlled())
	{
		ApplyRotationMode(WantsFirstPerson());
	}
}

// ---------------------------------------------------------------------------
// Cámara

bool ANDCharacter::WantsFirstPerson() const
{
	const int32 Forced = NDCharacter::CVarFirstPerson.GetValueOnGameThread();
	if (Forced >= 0)
	{
		return Forced != 0;
	}
	// Automático: en La Madriguera se sigue viendo al personaje; en la expedición y la casa, primera persona.
	const UWorld* World = GetWorld();
	return !(World && World->GetGameState() && World->GetGameState()->IsA<ANDLobbyGameState>());
}

void ANDCharacter::ApplyRotationMode(bool bFirstPerson)
{
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;
}

void ANDCharacter::ApplyCameraMode(bool bFirstPerson)
{
	const UNDScaleSettings* Scale = UNDScaleSettings::Get();
	Camera->SetFieldOfView(Scale->FieldOfView);
	CameraBoom->TargetArmLength = Scale->ThirdPersonArmLength;
	CameraBoom->SocketOffset = Scale->ThirdPersonOffset;
	if (bFirstPerson)
	{
		// En los ojos, dentro de la cápsula: así no atraviesa paredes ni techo.
		Camera->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Camera->SetRelativeLocationAndRotation(Scale->GetEyeOffsetFromCapsuleCenter(), FRotator::ZeroRotator);
		Camera->bUsePawnControlRotation = true;
	}
	else
	{
		Camera->AttachToComponent(CameraBoom, FAttachmentTransformRules::KeepRelativeTransform, USpringArmComponent::SocketName);
		Camera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Camera->bUsePawnControlRotation = false;
	}
	// El cuerpo no cambia; solo la nariz, que quedaría delante de los ojos, se oculta para el dueño.
	PlaceholderNose->SetOwnerNoSee(bFirstPerson);
	ApplyRotationMode(bFirstPerson);
	HeadBobOffset = 0.f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = Scale->MinViewPitch;
			PC->PlayerCameraManager->ViewPitchMax = Scale->MaxViewPitch;
		}
		// Plano cercano pequeño: el Desvelado tiene los ojos muy cerca de puertas y muebles.
		PC->ConsoleCommand(FString::Printf(TEXT("r.SetNearClipPlane %.1f"), bFirstPerson ? Scale->NearClipPlane : 10.f), true);
	}

	bFirstPersonActive = bFirstPerson;
	bCameraModeApplied = true;
}

void ANDCharacter::ServerSetFirstPerson_Implementation(bool bFirstPerson)
{
	ApplyRotationMode(bFirstPerson);
}

void ANDCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		const bool bWantsFirstPerson = WantsFirstPerson();
		if (!bCameraModeApplied || bWantsFirstPerson != bFirstPersonActive)
		{
			ApplyCameraMode(bWantsFirstPerson);
			if (!HasAuthority())
			{
				ServerSetFirstPerson(bWantsFirstPerson);
			}
		}
		UpdateHeadBob(DeltaSeconds);
	}
	UpdateHeadPitch();
}

void ANDCharacter::UpdateHeadBob(float DeltaSeconds)
{
	const UNDScaleSettings* Scale = UNDScaleSettings::Get();
	const bool bEnabled = bFirstPersonActive && Scale->bHeadBob && NDCharacter::CVarHeadBob.GetValueOnGameThread() != 0;
	const float Speed = GetVelocity().Size2D();
	if (bEnabled && GetCharacterMovement()->IsMovingOnGround() && Speed > 10.f)
	{
		HeadBobDistance += Speed * DeltaSeconds;
		HeadBobOffset = Scale->HeadBobAmplitude * FMath::Sin(2.f * PI * Scale->HeadBobPerMeter * HeadBobDistance / 100.f);
	}
	else
	{
		// Vuelve suave a la altura de los ojos.
		HeadBobOffset = FMath::FInterpTo(HeadBobOffset, 0.f, DeltaSeconds, 10.f);
	}
	if (bFirstPersonActive)
	{
		Camera->SetRelativeLocation(Scale->GetEyeOffsetFromCapsuleCenter() + FVector(0.f, 0.f, HeadBobOffset));
	}
}

void ANDCharacter::UpdateHeadPitch()
{
	// GetBaseAimRotation usa el cabeceo replicado (RemoteViewPitch) en los demás clientes.
	const float Pitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
	const FVector Pivot = UNDScaleSettings::Get()->GetEyeOffsetFromCapsuleCenter();
	const FRotator Tilt(Pitch, 0.f, 0.f);
	PlaceholderNose->SetRelativeLocationAndRotation(Pivot + Tilt.RotateVector(NDCharacter::NoseRestLocation - Pivot), Tilt);
}

void ANDCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANDCharacter, bIsSprinting, COND_SkipOwner);
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
		{ TEXT("Jump"),   EKeys::SpaceBar,    EKeys::Gamepad_FaceButton_Bottom },
		{ TEXT("Sprint"), EKeys::LeftShift,   EKeys::Gamepad_LeftThumbstick },
		{ TEXT("Crouch"), EKeys::LeftControl, EKeys::Gamepad_FaceButton_Right },
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

	// Nuevo dueño o nuevo controlador: vuelve a poner la cámara (y los límites de cabeceo) en el siguiente Tick.
	bCameraModeApplied = false;
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

// ---------------------------------------------------------------------------
// Correr

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

void ANDCharacter::OnRep_IsSprinting()
{
	UpdateMovementSpeed();
}

void ANDCharacter::UpdateMovementSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}
