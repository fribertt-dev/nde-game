// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/NDDesveladoTypes.h"
#include "NDDesveladoBodyComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Cuerpo provisional del Desvelado hecho con formas básicas del motor, uno por especie (GDD 3.1 y 3.2):
 * mapache, lémur, murciélago, zorro pequeño, hurón y búho, con su silueta y paleta de referencia y una
 * mochila de ladrón. Se mueve solo con código (pasos, cola, cabeza que mira, agacharse) hasta que haya
 * skeletal mesh, rig y Animation Blueprint; entonces se apaga solo si el Mesh del personaje tiene malla.
 * Va colgado del Mesh del personaje para usar su suavizado de red y su ajuste al agacharse.
 */
UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDDesveladoBodyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UNDDesveladoBodyComponent();

	/** Crea (o rehace) las piezas de una especie. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Body")
	void BuildSpecies(ENDDesveladoSpecies NewSpecies);

	UFUNCTION(BlueprintPure, Category = "NDE|Body")
	ENDDesveladoSpecies GetSpecies() const { return Species; }

	/** true si ya hay piezas (y por tanto sobra el placeholder de cápsula). */
	UFUNCTION(BlueprintPure, Category = "NDE|Body")
	bool IsBuilt() const { return Parts.Num() > 0; }

	/** Primera persona del dueño: oculta la cabeza para él (sigue proyectando sombra). */
	void SetFirstPersonOwner(bool bFirstPerson);

	/** Color principal de una especie (también para la UI). */
	static FLinearColor GetSpeciesColor(ENDDesveladoSpecies InSpecies);

	/** Escala de las piezas; 1 = Desvelado de 68 cm. Se ajusta sola a la cápsula de NDE Scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Body", meta = (ClampMin = "0.2"))
	float BodyScale = 1.f;

	/** Balanceo al andar, giro de cola y cabeza que sigue la mirada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Body")
	bool bProceduralMotion = true;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	enum class EShape : uint8 { Sphere, Cone, Cylinder, Cube };
	enum class EGroup : uint8 { Body, Head, Tail, FootL, FootR, HandL, HandR, Count };

	/** Pieza: forma, grupo que la mueve, posición (cm, desde el pivote del grupo), giro, tamaño (cm) y color. */
	struct FPart
	{
		EShape Shape;
		EGroup Group;
		FVector Location;
		FRotator Rotation;
		FVector Size;
		FLinearColor Color;
	};

	/** Recetas por especie. */
	static void MakeRecipe(ENDDesveladoSpecies InSpecies, TArray<FPart>& OutParts);

	void ClearParts();
	USceneComponent* EnsurePivot(EGroup Group);
	UStaticMesh* GetShapeMesh(EShape Shape) const;

	/** Coloca el cuerpo con los pies en el suelo de la cápsula, compensando el Mesh. */
	void PlaceOnCapsule();

	void UpdateMotion(float DeltaTime);

	ENDDesveladoSpecies Species = ENDDesveladoSpecies::None;
	bool bFirstPersonOwner = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Partes de la cabeza, para ocultarlas en primera persona. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> HeadParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Pivots;

	/** Posición de reposo de cada pivote. */
	TArray<FVector> PivotRest;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	float WalkPhase = 0.f;
	float IdleTime = 0.f;
	float MoveAlpha = 0.f;
};
