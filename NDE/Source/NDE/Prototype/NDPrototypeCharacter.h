// Personaje PROVISIONAL para probar el bucle en caja gris.
// No es la base del Desvelado final: ese personaje lo hará el frente de animación.
// Crea sus acciones de Enhanced Input en tiempo de ejecución para no depender de assets.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NDPrototypeCharacter.generated.h"

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

UCLASS()
class NDE_API ANDPrototypeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANDPrototypeCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<USpotLightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UStaticMeshComponent> NoseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UNDCarryComponent> Carry;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UNDLoadoutComponent> Loadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	TObjectPtr<UNDExpeditionPlayerComponent> ExpeditionPlayer;

	/** Velocidades del GDD 3.3 (CONFIGURABLE). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	float WalkSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	float SprintSpeed = 470.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	float CrouchSpeed = 185.f;

	/** Equipo de prueba (los assets los crea Scripts/nde_setup_bucle.py). */
	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> TestToolA;

	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> TestToolB;

	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TSoftObjectPtr<UNDToolDefinition> TestPingTool;

	/** Se reparten por turnos para probar los dos consumibles. */
	UPROPERTY(EditAnywhere, Category = "NDE|Prototype")
	TArray<TSoftObjectPtr<UNDToolDefinition>> TestConsumables;

	/** Muestra el estado de la expedición en pantalla (texto de depuración). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Prototype")
	bool bShowDebugHud = true;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

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

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void HandleFlashlightChanged(bool bOn);

	FVector GetAimPoint() const;
	void UpdateMovementSpeed();
	void DrawDebugHud() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> Actions;

	bool bWantsSprint = false;
	double ThrowStartTime = -1.0;
};
