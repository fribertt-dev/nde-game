// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Core/NDDesveladoTypes.h"
#include "GameFramework/Character.h"
#include "NDCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UNDCarryComponent;
class UNDLoadoutComponent;
class UNDExpeditionPlayerComponent;
class UNDToolDefinition;
class UNDInteractionComponent;
class UNDHidingComponent;
class UNDNoiseEmitterComponent;
class UNDDesveladoBodyComponent;
struct FInputActionValue;

/**
 * Personaje base de un Desvelado: movimiento replicado y cámara en primera persona (en los ojos) o en
 * tercera (nde.Camera.FirstPerson). La escala sale de UNDScaleSettings.
 * Lleva la carga de botín, las herramientas y las acciones de la expedición.
 * BP_Desvelado hereda de aquí. Hasta tener skeletal mesh se ve el cuerpo provisional de su especie
 * (UNDDesveladoBodyComponent); la cápsula con nariz solo queda si ese cuerpo no se puede crear.
 * Crea sus acciones de Enhanced Input en tiempo de ejecución para no depender de assets.
 */
UCLASS()
class NDE_API ANDCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANDCharacter();

	/** Blueprint que usan La Madriguera y la Expedición; si no existe se usa esta clase. */
	static TSubclassOf<APawn> GetDesveladoClass();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UCameraComponent> Camera;

	/** Linterna; la enciende la herramienta T03. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<USpotLightComponent> Flashlight;

	/** Placeholder: se oculta cuando el Blueprint tenga su skeletal mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	/** Placeholder: marca hacia dónde mira el personaje. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDCarryComponent> Carry;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDLoadoutComponent> Loadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDExpeditionPlayerComponent> ExpeditionPlayer;

	/** Acción contextual de E y su texto para la UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDInteractionComponent> Interaction;

	/** Esconderse en los muebles de la casa. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDHidingComponent> Hiding;

	/** Pasos y aterrizajes que oye la criatura. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDNoiseEmitterComponent> NoiseEmitter;

	/** Cuerpo provisional por especie (formas básicas) hasta tener skeletal mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	TObjectPtr<UNDDesveladoBodyComponent> Body;

	/** Especie del Desvelado (solo estética, GDD 3.1). La elige el servidor al poseerlo. */
	UFUNCTION(BlueprintPure, Category = "NDE|Personaje")
	ENDDesveladoSpecies GetSpecies() const { return Species; }

	/** Solo servidor: cambia la especie (se replica y rehace el cuerpo en todas las máquinas). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Personaje")
	void SetSpecies(ENDDesveladoSpecies NewSpecies);

	/** Usar siempre ForcedSpecies en vez de una especie distinta por jugador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	bool bForceSpecies = false;

	/** Especie fija para probar (si Force Species está marcado). La consola nde.Character.Species manda sobre esto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Personaje", meta = (EditCondition = "bForceSpecies"))
	ENDDesveladoSpecies ForcedSpecies = ENDDesveladoSpecies::Raccoon;

	/** Velocidad al andar (GDD 3.3). Viene de Project Settings > Game > NDE Scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float WalkSpeed = 320.f;

	/** Velocidad al correr (GDD 3.3). Viene de NDE Scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float SprintSpeed = 470.f;

	/** Velocidad agachado (GDD 3.3). Viene de NDE Scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float CrouchSpeed = 185.f;

	/** Equipo de prueba hasta que La Madriguera tenga su loadout (assets de Scripts/nde_setup_bucle.py). */
	UPROPERTY(EditAnywhere, Category = "NDE|Character|Test")
	TSoftObjectPtr<UNDToolDefinition> TestToolA;

	UPROPERTY(EditAnywhere, Category = "NDE|Character|Test")
	TSoftObjectPtr<UNDToolDefinition> TestToolB;

	UPROPERTY(EditAnywhere, Category = "NDE|Character|Test")
	TSoftObjectPtr<UNDToolDefinition> TestPingTool;

	/** Se reparten por turnos para probar los dos consumibles. */
	UPROPERTY(EditAnywhere, Category = "NDE|Character|Test")
	TArray<TSoftObjectPtr<UNDToolDefinition>> TestConsumables;

	/** Muestra el estado de la expedición en pantalla (texto de depuración). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Character|Test")
	bool bShowDebugHud = true;

	UFUNCTION(BlueprintPure, Category = "NDE|Character")
	bool IsSprinting() const { return bIsSprinting; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	/** true si la cámara del dueño está en los ojos (primera persona). */
	UFUNCTION(BlueprintPure, Category = "NDE|Personaje")
	bool IsFirstPerson() const { return bFirstPersonActive; }

protected:
	/**
	 * Primera persona: nde.Camera.FirstPerson 1/0 manda; con -1 (por defecto) es primera persona salvo en
	 * La Madriguera, que sigue en tercera.
	 */
	bool WantsFirstPerson() const;

	/** Cámara en los ojos (primera persona) o en el brazo (tercera). Solo cambia la cámara, no el cuerpo. */
	void ApplyCameraMode(bool bFirstPerson);

	/** En primera persona el cuerpo gira con la vista; en tercera, hacia donde anda. Igual en servidor y dueño. */
	void ApplyRotationMode(bool bFirstPerson);

	UFUNCTION(Server, Reliable)
	void ServerSetFirstPerson(bool bFirstPerson);

	/** Texto de depuración del bucle: según nde.Expedition.DebugText (por defecto, solo sin HUD). */
	bool ShouldDrawDebugText() const;

	void UpdateHeadBob(float DeltaSeconds);

	/** Inclina la nariz con el cabeceo de la vista (replicado): los demás ven hacia dónde mira. */
	void UpdateHeadPitch();

	/** Cápsula, escalón, salto y velocidades desde UNDScaleSettings (única fuente de la escala). */
	void ApplyScaleSettings();

	void EnsureInputObjects();
	UInputAction* MakeAction(FName Name, bool bAxis2D);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintEnded();
	void OnCrouchStarted();
	void OnCrouchEnded();
	void OnInteract();
	void OnDrop();
	void OnThrowStarted();
	void OnThrowReleased();
	void OnUseTool();
	void OnUseConsumable();
	void OnPing();
	void OnSelectSlot1();
	void OnSelectSlot2();
	void OnVoteNo();

	void SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void HandleFlashlightChanged(bool bOn);

	FVector GetAimPoint() const;

	/** Aplica correr, agacharse y el peso de lo que se carga. */
	void UpdateMovementSpeed();

	UFUNCTION()
	void OnRep_Species();

	/** Rehace el cuerpo de la especie y oculta el placeholder si el cuerpo existe. */
	void ApplySpecies();

	/** Especie para un jugador recién poseído: forzada, la de la consola o una por puesto en la partida. */
	ENDDesveladoSpecies PickSpecies(const AController* NewController) const;

	UPROPERTY(ReplicatedUsing = OnRep_Species)
	ENDDesveladoSpecies Species = ENDDesveladoSpecies::None;

	/** El dueño lo predice al pulsar; el servidor lo confirma y lo reparte a los demás. */
	UPROPERTY(Replicated)
	bool bIsSprinting = false;

	bool bFirstPersonActive = false;
	bool bCameraModeApplied = false;
	/** Distancia andada para el balanceo de cabeza. */
	float HeadBobDistance = 0.f;
	float HeadBobOffset = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> Actions;

	double ThrowStartTime = -1.0;
};
