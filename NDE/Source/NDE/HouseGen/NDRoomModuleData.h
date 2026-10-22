// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "NDRoomModuleData.generated.h"

/**
 * Módulo de sala como asset (DA_Room_...). Preparado para cuando cada módulo tenga
 * su Level Instance de arte; hoy el generador usa el catálogo C++ (FNDRoomCatalog).
 */
UCLASS(BlueprintType)
class NDE_API UNDRoomModuleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Datos del módulo: tamaño, huecos de puerta y puntos marcados. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FNDRoomModuleDefinition Definition;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
