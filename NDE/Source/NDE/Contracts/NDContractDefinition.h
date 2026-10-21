// Definición de contrato basada en datos (GDD 6.1, 7.8 y 12).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDContractDefinition.generated.h"

UCLASS(BlueprintType)
class NDE_API UNDContractDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato")
	FName ContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato", meta = (ClampMin = 1, ClampMax = 4))
	int32 DifficultyTier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato", meta = (ClampMin = 1, ClampMax = 6))
	int32 RecommendedPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objetivo")
	ENDObjectiveType ObjectiveType = ENDObjectiveType::Quota;

	/** Puntos de botín válidos que hay que depositar. 0 = sin cuota. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objetivo", meta = (ClampMin = 0))
	int32 RequiredQuotaValue = 0;

	/** LootId que deben llegar a la camioneta sin romperse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objetivo")
	TArray<FName> RequiredLootIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objetivo")
	ENDSecondaryObjective SecondaryObjective = ENDSecondaryObjective::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objetivo")
	FText SecondaryObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recompensa", meta = (ClampMin = 0))
	int32 BasePayoutPerPlayer = 150;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recompensa", meta = (ClampMin = 0))
	int32 ReputationReward = 45;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recompensa", meta = (ClampMin = 0))
	float ContractValueMultiplier = 1.f;

	/** Duración de la noche (GDD 7.8: 600 s por defecto, 480 corto, 720-840 avanzado, 900 operación). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noche", meta = (ClampMin = 30))
	float NightDurationSeconds = 600.f;

	/** Ventana final tras el amanecer. No ampliable con herramientas normales. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noche", meta = (ClampMin = 0))
	float DawnGraceSeconds = 45.f;

	/** Solo se desactiva si la tarjeta del contrato lo anuncia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noche")
	bool bDawnEnabled = true;

	/** Mapa de la casa. En el corte gris todos usan el mismo nivel de prueba. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contrato")
	TSoftObjectPtr<UWorld> HouseMap;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("NDContract"), ContractId.IsNone() ? GetFName() : ContractId);
	}
};
