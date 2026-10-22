// No Despiertes Eso

#include "Players/NDCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/NDLog.h"
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
#include "Players/NDScaleSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace NDCharacter
{
	const TCHAR* DesveladoClassPath = TEXT("/Game/_NDE_/Characters/BP_Desvelado.BP_Desvelado_C");
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
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->SocketOffset = FVector(0.f, 35.f, 45.f);
	CameraBoom->bUsePawnControlRotation = true;

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
