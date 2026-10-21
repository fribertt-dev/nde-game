// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NDCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Personaje base de un Desvelado: movimiento replicado y cámara en tercera persona.
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	TObjectPtr<UCameraComponent> Camera;

	/** Placeholder: se oculta cuando el Blueprint tenga su skeletal mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	/** Placeholder: marca hacia dónde mira el personaje. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	/** Velocidades del GDD 3.3 (CONFIGURABLE). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float WalkSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float SprintSpeed = 470.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Personaje")
	float CrouchSpeed = 185.f;

	UFUNCTION(BlueprintPure, Category = "NDE|Personaje")
	bool IsSprinting() const { return bIsSprinting; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void BeginPlay() override;

protected:
	void EnsureInputObjects();
	UInputAction* MakeAction(FName Name, bool bAxis2D);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted();
	void OnSprintEnded();
	void OnCrouchStarted();
	void OnCrouchEnded();

	void SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	void UpdateMovementSpeed();

	/** El dueño lo predice al pulsar; el servidor lo confirma y lo reparte a los demás. */
	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> Actions;
};
