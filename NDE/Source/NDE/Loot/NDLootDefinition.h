// Definición de un tipo de botín (GDD 8.1 a 8.5).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDLootDefinition.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class NDE_API UNDLootDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Id del catálogo, por ejemplo L16. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName LootId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	ENDLootCategory Category = ENDLootCategory::Light;

	/** Valor base en puntos de botín, no en monedas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 BaseValue = 100;

	/** Peso de diseño en unidades abstractas (GDD 8.2). Informativo; manda la categoría. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	float DesignWeight = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragility")
	ENDFragility Fragility = ENDFragility::Sturdy;

	/** Cambio de velocidad (uu/s) que lo daña. 0 = valor por defecto de su fragilidad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragility", meta = (ClampMin = 0))
	float DamageSpeedThreshold = 0.f;

	/** Cambio de velocidad (uu/s) que lo rompe. 0 = valor por defecto de su fragilidad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragility", meta = (ClampMin = 0))
	float BreakSpeedThreshold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragility", meta = (ClampMin = 0, ClampMax = 1))
	float DamagedValueFactor = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragility", meta = (ClampMin = 0, ClampMax = 1))
	float BrokenValueFactor = 0.2f;

	/** Jugadores necesarios para moverlo a velocidad normal de carga. 0 = según categoría. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transport", meta = (ClampMin = 0, ClampMax = 4))
	int32 RequiredCarriersOverride = 0;

	/** El carrito plegable (T07) cuenta como un portador extra. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transport")
	bool bCartAllowed = true;

	/** Sonoridad base de un golpe (0 a 1) para el sistema de ruido. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = 0, ClampMax = 1))
	float ImpactNoiseLoudness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Escala aplicada a la malla en el corte gris. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector MeshScale = FVector::OneVector;

	/** Malla de arte (p. ej. SM_Loot_*), a su tamaño real. Solo se ve: la física y la colisión siguen siendo de Mesh, que queda oculta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> ArtMesh;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	int32 GetRequiredCarriers() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	float GetDamageSpeedThreshold() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	float GetBreakSpeedThreshold() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	float GetConditionFactor(ENDLootCondition Condition) const;

	UFUNCTION(BlueprintPure, Category = "NDE|Loot")
	bool IsPocketSized() const { return Category == ENDLootCategory::Pocket; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("NDLoot"), LootId.IsNone() ? GetFName() : LootId);
	}
};
