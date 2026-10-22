// Definición de herramienta o consumible (GDD 18).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "NDToolDefinition.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ENDToolKind : uint8
{
	BasicFree,
	Permanent,
	Consumable
};

/** Efecto implementado en C++. Otros efectos pueden hacerse en Blueprint con OnToolUsed. */
UENUM(BlueprintType)
enum class ENDToolEffect : uint8
{
	None,
	/** T02: marca una posición para el equipo. */
	Ping,
	/** T03: enciende o apaga la linterna. */
	Flashlight,
	/** T07: pasivo, cuenta como un portador extra para bultos pesados. */
	Cart,
	/** T17, T18: coloca un actor en el punto apuntado. */
	SpawnDeployable
};

UCLASS(BlueprintType)
class NDE_API UNDToolDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool")
	ENDToolKind Kind = ENDToolKind::Permanent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool")
	ENDToolEffect Effect = ENDToolEffect::None;

	/** Precio en monedas mugrosas en la tienda de Don Mugre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = 0))
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = 0))
	int32 UnlockReputation = 0;

	/** Usos por expedición. -1 = ilimitado (herramientas permanentes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage")
	int32 Charges = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage", meta = (ClampMin = 0))
	float CooldownSeconds = 0.5f;

	/** Ruido al usarla (0 a 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage", meta = (ClampMin = 0, ClampMax = 1))
	float UseNoiseLoudness = 0.f;

	/** Distancia máxima para colocar un desplegable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage", meta = (ClampMin = 0))
	float PlaceRange = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage")
	TSubclassOf<AActor> DeployableClass;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("NDTool"), ItemId.IsNone() ? GetFName() : ItemId);
	}
};
