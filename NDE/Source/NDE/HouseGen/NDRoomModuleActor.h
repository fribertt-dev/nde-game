// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "NDRoomModuleActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;

/**
 * Sala greybox: suelo, paredes con huecos de puerta y techo opcional, hechos con el cubo
 * del motor. No se replica: cada máquina la construye desde el manifiesto.
 * Más adelante este actor cargará el Level Instance de arte (ArtLevel) en lugar del greybox.
 */
UCLASS()
class NDE_API ANDRoomModuleActor : public AActor
{
	GENERATED_BODY()

public:
	ANDRoomModuleActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * Construye el módulo. OpeningWidths tiene un valor por hueco de la definición:
	 * ancho del hueco abierto en cm, o 0 si se tapa con pared. CeilingPerCell (en el orden de
	 * GetLocalCells) quita el techo donde hay otra sala encima; sin él, techo en todas las celdas.
	 */
	void BuildGreybox(const FNDRoomModuleDefinition& Definition, const TArray<float>& OpeningWidths, bool bBuildCeiling, bool bShowMarkers,
		bool bAddRoomLight = false, const TArray<bool>* CeilingPerCell = nullptr, const TArray<float>* OpeningOffsets = nullptr,
		const TArray<FNDRoomDoorSocket>* ExtraOpenings = nullptr);

	/** Módulo del catálogo que muestra este actor al colocarlo a mano en el editor. */
	UPROPERTY(EditAnywhere, Category = "NDE|HouseGen")
	FName PreviewModuleId;

	/** Al previsualizar en el editor, abre todos los huecos. */
	UPROPERTY(EditAnywhere, Category = "NDE|HouseGen")
	bool bPreviewOpenSockets = true;

	/** Sala del manifiesto que representa (INDEX_NONE si es una previsualización). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "NDE|HouseGen")
	int32 RoomInstanceId = INDEX_NONE;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UInstancedStaticMeshComponent> FloorMeshes;

	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UInstancedStaticMeshComponent> WallMeshes;

	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<UInstancedStaticMeshComponent> CeilingMeshes;

	/** Marcas de suelo por tipo de punto (botín, escondite...); sin colisión. Se crean al construir. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> MarkerMeshes;

	/** Luz interior tenue del greybox (se crea al construir si el lote la pide). */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> RoomLight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	void ClearGreybox();
	void AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	void SetColor(UInstancedStaticMeshComponent* Target, const FLinearColor& Color);
	void BuildEdge(const FVector& EdgeCenter, ENDCellDirection Direction, float OpeningWidth, float OpeningOffset = 0.f);
	UInstancedStaticMeshComponent* GetMarkerMeshes(ENDRoomMarkerType Type);
};
