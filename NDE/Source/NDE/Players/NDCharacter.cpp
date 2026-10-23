// No Despiertes Eso

#include "Players/NDCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Net/UnrealNetwork.h"
#include "Networking/NDLobbyGameState.h"
#include "Players/NDDesveladoBodyComponent.h"
#include "Players/NDHidingComponent.h"
#include "Players/NDInteractionComponent.h"
#include "Players/NDNoiseEmitterComponent.h"
#include "Players/NDScaleSettings.h"
#include "EngineUtils.h"
#include "UI/NDUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
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

	/** Nariz del placeholder respecto al centro de la cápsula, mirando al frente. */
	const FVector NoseRestLocation(22.f, 0.f, 15.f);

	TAutoConsoleVariable<int32> CVarFirstPerson(
		TEXT("nde.Camera.FirstPerson"),
		-1,
		TEXT("Cámara del Desvelado: 1 = primera persona (en los ojos), 0 = tercera persona, -1 = automático (primera persona salvo en La Madriguera)."));

	TAutoConsoleVariable<int32> CVarDebugText(
		TEXT("nde.Expedition.DebugText"),
		-1,
		TEXT("Texto de depuración de la expedición arriba a la izquierda: 1 = siempre, 0 = nunca, -1 = automático (solo si no hay HUD de expedición)."));

	TAutoConsoleVariable<int32> CVarSpecies(
		TEXT("nde.Character.Species"),
		0,
		TEXT("Especie del Desvelado al aparecer: 0 = una distinta por jugador, 1 = mapache, 2 = lémur, 3 = murciélago, 4 = zorro, 5 = hurón, 6 = búho."));

	/** nde.Character.SetSpecies N: cambia la especie del jugador local ya en partida (solo en el servidor o en solitario). */
	FAutoConsoleCommandWithWorldAndArgs CmdSetSpecies(
		TEXT("nde.Character.SetSpecies"),
		TEXT("Cambia la especie de tu Desvelado ahora: 1 = mapache, 2 = lémur, 3 = murciélago, 4 = zorro, 5 = hurón, 6 = búho. Sin número, pasa a la siguiente. Solo en el servidor o en solitario."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ANDCharacter> It(World); It; ++It)
			{
				if (!It->IsLocallyControlled() || !It->HasAuthority())
				{
					continue;
				}
				const int32 Count = static_cast<int32>(ENDDesveladoSpecies::Owl);
				int32 Value = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : static_cast<int32>(It->GetSpecies()) % Count + 1;
				Value = FMath::Clamp(Value, 1, Count);
				It->SetSpecies(static_cast<ENDDesveladoSpecies>(Value));
			}
		}));

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

	Carry = CreateDefaultSubobject<UNDCarryComponent>(TEXT("Carry"));
	Loadout = CreateDefaultSubobject<UNDLoadoutComponent>(TEXT("Loadout"));
	ExpeditionPlayer = CreateDefaultSubobject<UNDExpeditionPlayerComponent>(TEXT("ExpeditionPlayer"));
	Interaction = CreateDefaultSubobject<UNDInteractionComponent>(TEXT("Interaction"));
	Hiding = CreateDefaultSubobject<UNDHidingComponent>(TEXT("Hiding"));
	NoiseEmitter = CreateDefaultSubobject<UNDNoiseEmitterComponent>(TEXT("NoiseEmitter"));

	// Cuelga del Mesh (aunque esté vacío) para tener su suavizado de red y su ajuste al agacharse.
	Body = CreateDefaultSubobject<UNDDesveladoBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(GetMesh());

	TestToolA = NDCharacter::ToolAsset(TEXT("T03"));
	TestToolB = NDCharacter::ToolAsset(TEXT("T07"));
	TestPingTool = NDCharacter::ToolAsset(TEXT("T02"));
	TestConsumables.Add(NDCharacter::ToolAsset(TEXT("T17")));
	TestConsumables.Add(NDCharacter::ToolAsset(TEXT("T18")));

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

	Loadout->OnFlashlightChanged.AddDynamic(this, &ANDCharacter::HandleFlashlightChanged);
	UpdateMovementSpeed();
	// Un cuerpo desde el principio; la especie buena llega al poseerlo (servidor) o por réplica.
	ApplySpecies();
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
	// Escondido se mira siempre desde los ojos: en tercera la cámara chocaría con el mueble.
	if (Hiding && Hiding->IsHidden())
	{
		return true;
	}
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
	// El cuerpo no cambia; solo la cabeza (y la nariz), que quedaría delante de los ojos, se oculta para el dueño.
	PlaceholderNose->SetOwnerNoSee(bFirstPerson);
	Body->SetFirstPersonOwner(bFirstPerson);
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
		if (AutoWalkRemaining > 0.f && Controller)
		{
			AutoWalkRemaining -= DeltaSeconds;
			AddMovementInput(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector(), 1.f);
		}
	}
	UpdateHeadPitch();

	// El peso de la carga cambia sin avisar; servidor y dueño recalculan la velocidad cada frame.
	if (HasAuthority() || IsLocallyControlled())
	{
		UpdateMovementSpeed();
	}
	if (IsLocallyControlled() && bShowDebugHud && ShouldDrawDebugText())
	{
		NDExpeditionDebugHud::Draw(this, *Carry, *Loadout, *ExpeditionPlayer);
	}
}

bool ANDCharacter::ShouldDrawDebugText() const
{
	const int32 Mode = NDCharacter::CVarDebugText.GetValueOnGameThread();
	if (Mode >= 0)
	{
		return Mode == 1;
	}
	// Automático: si la UI tiene su raíz en pantalla, el HUD de expedición ya enseña esto.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UNDUIManagerSubsystem* UIManager = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UNDUIManagerSubsystem>() : nullptr;
	return !UIManager || !UIManager->GetRootLayout();
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

	SetSpecies(PickSpecies(NewController));
}

ENDDesveladoSpecies ANDCharacter::PickSpecies(const AController* NewController) const
{
	const int32 Count = static_cast<int32>(ENDDesveladoSpecies::Owl);
	const int32 FromConsole = NDCharacter::CVarSpecies.GetValueOnGameThread();
	if (FromConsole >= 1 && FromConsole <= Count)
	{
		return static_cast<ENDDesveladoSpecies>(FromConsole);
	}
	if (bForceSpecies && ForcedSpecies != ENDDesveladoSpecies::None)
	{
		return ForcedSpecies;
	}
	// Una por puesto en la partida: con 6 jugadores salen las 6 (GDD 3.2: especie única por partida).
	const APlayerState* PS = NewController ? NewController->PlayerState : nullptr;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const int32 Slot = PS && GameState ? FMath::Max(0, GameState->PlayerArray.IndexOfByKey(PS)) : 0;
	return static_cast<ENDDesveladoSpecies>(Slot % Count + 1);
}

void ANDCharacter::SetSpecies(ENDDesveladoSpecies NewSpecies)
{
	if (!HasAuthority())
	{
		return;
	}
	Species = NewSpecies;
	ApplySpecies();
}

void ANDCharacter::OnRep_Species()
{
	ApplySpecies();
}

void ANDCharacter::ApplySpecies()
{
	Body->BuildSpecies(Species);
	const bool bBodyBuilt = Body->IsBuilt();
	PlaceholderBody->SetVisibility(!bBodyBuilt);
	PlaceholderNose->SetVisibility(!bBodyBuilt);
}

void ANDCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANDCharacter, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME(ANDCharacter, Species);
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
	if (!Hiding->IsHidden())
	{
		Crouch();
	}
}

void ANDCharacter::OnCrouchEnded()
{
	UnCrouch();
}

void ANDCharacter::OnInteract()
{
	// Camioneta, botín, escondites y actores NDInteractable; decide UNDInteractionComponent.
	Interaction->Interact();
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

void ANDCharacter::UpdateMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// ApplyScaleSettings puede llegar antes de que exista Carry (constructor de Blueprints antiguos).
	const float Multiplier = Carry ? Carry->GetSpeedMultiplier() : 1.f;
	const bool bSprinting = bIsSprinting && (!Carry || Carry->CanSprint()) && !bIsCrouched;
	Movement->MaxWalkSpeed = (bSprinting ? SprintSpeed : WalkSpeed) * Multiplier;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;
}
