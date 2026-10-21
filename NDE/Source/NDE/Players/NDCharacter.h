// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
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
struct FInputActionValue;

/**
 * Personaje base de un Desvelado: movimiento replicado, cámara en tercera persona,
 * carga de botín, herramientas y acciones de la expedición.
 * BP_Desvelado hereda de aquí; la cápsula con nariz es provisional hasta tener el skeletal mesh.
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

	/** Velocidades del GDD 3.3 (CONFIGURABLE). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	float WalkSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Character")
	float SprintSpeed = 470.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Character")
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
	virtual void Tick(float DeltaSeconds) override;

protected:
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

	/** El dueño lo predice al pulsar; el servidor lo confirma y lo reparte a los demás. */
	UPROPERTY(Replicated)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> Actions;

	double ThrowStartTime = -1.0;
};
