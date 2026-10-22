// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NDScaleSettings.generated.h"

/**
 * Escala del Desvelado y de su cámara: única fuente de verdad.
 * Opción del GDD: un Desvelado pequeño (unos 60 cm) en una casa a escala de gigante. La casa no cambia
 * (celda 400 cm, puerta 160 x 220 cm, paredes 380 cm, 400 cm por piso); todo lo del personaje se
 * mide respecto a su altura de ojos. Se ajusta sin recompilar en Project Settings > Game > NDE Scale (DefaultGame.ini).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NDE Scale"))
class NDE_API UNDScaleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNDScaleSettings();

	static const UNDScaleSettings* Get() { return GetDefault<UNDScaleSettings>(); }

	/** Radio de la cápsula del Desvelado, en cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = "5"))
	float CapsuleRadius = 22.f;

	/** Media altura de la cápsula, en cm (altura total = el doble). */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = "10"))
	float CapsuleHalfHeight = 34.f;

	/** Media altura agachado, en cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = "10"))
	float CrouchedHalfHeight = 22.f;

	/** Altura de los ojos sobre el suelo, en cm: ahí va la cámara en primera persona. */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = "5"))
	float EyeHeight = 56.f;

	/** Cuánto delante del centro de la cápsula van los ojos, en cm (dentro de la cápsula para no atravesar paredes). */
	UPROPERTY(Config, EditAnywhere, Category = "Body", meta = (ClampMin = "0"))
	float EyeForwardOffset = 10.f;

	/** Escalón más alto que sube andando, en cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float MaxStepHeight = 25.f;

	/** Velocidad inicial del salto, en cm/s (con gravedad normal sube unos 100 cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float JumpZVelocity = 450.f;

	/** Velocidad al andar, en cm/s (GDD 3.3). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float WalkSpeed = 320.f;

	/** Velocidad al correr, en cm/s (GDD 3.3). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float SprintSpeed = 470.f;

	/** Velocidad agachado, en cm/s (GDD 3.3). */
	UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0"))
	float CrouchSpeed = 185.f;

	/** Campo de visión en grados. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "60", ClampMax = "120"))
	float FieldOfView = 90.f;

	/** Plano cercano de la cámara, en cm: pequeño para un personaje bajito, sin ver a través de paredes. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "20"))
	float NearClipPlane = 5.f;

	/** Cuánto se puede mirar hacia abajo, en grados. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "-89", ClampMax = "0"))
	float MinViewPitch = -85.f;

	/** Cuánto se puede mirar hacia arriba, en grados. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxViewPitch = 85.f;

	/** Balanceo de cabeza al andar (accesibilidad: se puede quitar). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	bool bHeadBob = true;

	/** Altura del balanceo de cabeza, en cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "5"))
	float HeadBobAmplitude = 1.f;

	/** Balanceos por cada 100 cm recorridos. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0"))
	float HeadBobPerMeter = 1.6f;

	/** Distancia de la cámara en tercera persona, en cm (proporcional al Desvelado). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "50"))
	float ThirdPersonArmLength = 180.f;

	/** Desplazamiento de la cámara en tercera persona (lado, altura), en cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FVector ThirdPersonOffset = FVector(0.f, 20.f, 30.f);

	/** Altura máxima del salto con la gravedad dada (cm/s²), en cm. */
	float GetJumpApex(float GravityZ) const;

	/** Posición de los ojos respecto al centro de la cápsula. */
	FVector GetEyeOffsetFromCapsuleCenter() const { return FVector(EyeForwardOffset, 0.f, EyeHeight - CapsuleHalfHeight); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
