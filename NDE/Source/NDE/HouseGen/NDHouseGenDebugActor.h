// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "NDHouseGenDebugActor.generated.h"

class UTextRenderComponent;

/**
 * Vista previa del generador sin construir la casa: colócalo en un mapa, cambia la
 * semilla y pulsa Generate. Dibuja el lote, las salas (color por tipo), las puertas,
 * los bucles (naranja), la ruta del bulto (verde) y los puntos marcados.
 * En juego solo genera quien tiene autoridad.
 */
UCLASS()
class NDE_API ANDHouseGenDebugActor : public AActor
{
	GENERATED_BODY()

public:
	ANDHouseGenDebugActor();

	virtual void BeginPlay() override;

	/** Genera con la semilla actual y dibuja el resultado. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NDE|HouseGen")
	void Generate();

	/** Borra el dibujo. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NDE|HouseGen")
	void Clear();

	/** Pasa a la semilla siguiente y genera. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NDE|HouseGen")
	void NextSeed();

	/** Semilla de la casa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	int64 Seed = 1;

	/** Arquetipo de casa. De momento solo H01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FName Archetype = TEXT("H01");

	/** Tamaño de casa: Small 5-8 salas, Medium 7-11, Large 10-17. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	ENDHouseSize HouseSize = ENDHouseSize::Small;

	/** Mínimo de salas. 0 = el del tamaño de casa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "17"))
	int32 MinRooms = 0;

	/** Máximo de salas. 0 = el del tamaño de casa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "17"))
	int32 MaxRooms = 0;

	/** Piso superior: Auto según el tamaño, o siempre / nunca. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	ENDFloorOption UpperFloor = ENDFloorOption::Auto;

	/** Sótano: Auto según el tamaño, o siempre / nunca. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	ENDFloorOption Basement = ENDFloorOption::Auto;

	/** Lote de prueba (tamaño y fachada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FNDHouseLotInfo LotInfo;

	/** Genera también al empezar la partida. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	bool bGenerateOnBeginPlay = false;

	/** Resultado de la última generación. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "NDE|HouseGen")
	FString LastResult;

	/** Manifiesto de la última generación. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "NDE|HouseGen")
	FNDHouseGenerationManifest LastManifest;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NDE|HouseGen")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Labels;
};
