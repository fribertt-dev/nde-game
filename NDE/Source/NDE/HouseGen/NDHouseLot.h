// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseGen/NDHouseLayout.h"
#include "NDHouseLot.generated.h"

class ANDRoomModuleActor;
class UArrowComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ANDHouseLot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnHouseBuilt, ANDHouseLot*, Lot);

/**
 * Parcela de la calle donde se levanta una casa generada. Se coloca en el mapa del vecindario:
 * su transform es el centro del lote y la flecha apunta a la calle (FrontDirection).
 *
 * Solo el servidor genera la casa. El manifiesto se replica y cada máquina (servidor incluido)
 * construye desde él las salas greybox, el césped libre del lote y la marca de extracción.
 */
UCLASS()
class NDE_API ANDHouseLot : public AActor
{
	GENERATED_BODY()

public:
	ANDHouseLot();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Genera una casa con esta semilla, la construye y la replica. Solo servidor. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|HouseGen")
	void GenerateHouse(int64 Seed);

	/** Semilla para la próxima generación automática (p. ej. ?Seed= de la URL). Solo servidor. */
	void SetSeedOverride(int64 Seed);

	UFUNCTION(BlueprintPure, Category = "NDE|HouseGen")
	bool IsHouseBuilt() const { return bHouseBuilt; }

	UFUNCTION(BlueprintPure, Category = "NDE|HouseGen")
	const FNDHouseGenerationManifest& GetManifest() const { return Manifest; }

	/** Casa resuelta (válida cuando IsHouseBuilt). */
	const FNDHouseLayout& GetLayout() const { return Layout; }

	/** Transform del mundo para el jugador número Index (da la vuelta si hay más jugadores que spawns). */
	UFUNCTION(BlueprintPure, Category = "NDE|HouseGen")
	FTransform GetPlayerSpawnTransform(int32 Index) const;

	/** Transforms del mundo de todos los puntos de un tipo. */
	UFUNCTION(BlueprintPure, Category = "NDE|HouseGen")
	TArray<FTransform> GetMarkerTransforms(ENDRoomMarkerType Type) const;

	/** Del centro del lote (cm) al mundo. */
	FTransform GetLotToWorld() const { return GetActorTransform(); }

	/** Reglas que se usarán con esta semilla. */
	FNDHouseGenParams MakeParams(int64 Seed) const;

	/** Primer lote del mundo, o nullptr. */
	static ANDHouseLot* FindLot(const UWorld* World);

	/** Se dispara en cada máquina cuando termina de construir (también tras regenerar). */
	UPROPERTY(BlueprintAssignable, Category = "NDE|HouseGen")
	FNDOnHouseBuilt OnHouseBuilt;

	/** Tamaño, fachada e id del lote. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FNDHouseLotInfo LotInfo;

	/** Arquetipo de casa. De momento solo H01. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName HouseArchetype = TEXT("H01");

	/** Distrito del vecindario: elige los módulos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName District = TEXT("D01");

	/** Usa FixedSeed en vez de una semilla al azar. ?Seed= en la URL manda sobre esto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bUseFixedSeed = false;

	/** Semilla fija para probar siempre la misma casa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (EditCondition = "bUseFixedSeed"))
	int64 FixedSeed = 1;

	/** Tamaño de casa: Small 5-8 salas, Medium 7-11, Large 10-17. El lote crece si no cabe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDHouseSize HouseSize = ENDHouseSize::Small;

	/** Mínimo de salas. 0 = el del tamaño de casa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "17"))
	int32 MinRooms = 0;

	/** Máximo de salas. 0 = el del tamaño de casa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "17"))
	int32 MaxRooms = 0;

	/** Genera la casa al empezar la partida (en el servidor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bGenerateOnBeginPlay = true;

	/** Pone techo a las salas. Desactívalo para ver la casa desde arriba mientras pruebas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bBuildCeiling = true;

	/** Pinta en el suelo botín (amarillo), escondites (azul), criatura (rojo) y extracción (verde). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bShowMarkers = true;

	/** Pone una luz tenue y cálida en cada sala, para que el interior no quede negro con techo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bLightRooms = true;

	/** Piso superior: Auto según el tamaño de casa (Small casi nunca, Large a menudo), o siempre / nunca. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDFloorOption UpperFloor = ENDFloorOption::Auto;

	/** Sótano bajo la planta baja: Auto según el tamaño de casa, o siempre / nunca. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDFloorOption Basement = ENDFloorOption::Auto;

	/** Depuración: oculta los pisos por encima de este (0 = ver la planta baja desde arriba, -1 = ver el sótano). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "-1", ClampMax = "1"))
	int32 MaxVisibleFloor = 1;

	/** Dibuja en el servidor el grafo de salas, puertas y la ruta del bulto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bDrawDebugGraph = false;

	/** true cuando la navegación encontró camino de la entrada al dormitorio (solo servidor). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "NDE|HouseGen")
	bool bNavPathVerified = false;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<USceneComponent> Root;

	/** Límite del lote (solo en el editor). */
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UBoxComponent> LotBounds;

	/** Apunta a la calle. */
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UArrowComponent> FrontArrow;

	/** Césped en las celdas del lote sin sala. */
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UInstancedStaticMeshComponent> GroundMeshes;

	/** Marca de extracción (sitio de la camioneta). */
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UInstancedStaticMeshComponent> ExtractionMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	UFUNCTION()
	void OnRep_Manifest();

	void GenerateOnStart();
	void BuildFromManifest();
	void ClearHouse();
	void UpdateEditorBounds();
	void CheckNavigationPath();

	UPROPERTY(ReplicatedUsing = OnRep_Manifest)
	FNDHouseGenerationManifest Manifest;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANDRoomModuleActor>> RoomActors;

	FNDHouseLayout Layout;
	bool bHouseBuilt = false;
	bool bHasSeedOverride = false;
	int64 SeedOverride = 0;
	int32 NavCheckTries = 0;
	FTimerHandle NavCheckTimer;
};
