// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "NDHidingComponent.generated.h"

class ACharacter;
class ANDHouseLot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnHiddenChanged, bool, bHidden);

/** Escondite de la casa visto desde el mundo (sale del layout de ANDHouseLot). */
USTRUCT(BlueprintType)
struct FNDHidingSpotInfo
{
	GENERATED_BODY()

	/** StableId del escondite en el layout; INDEX_NONE = ninguno. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	int32 SpotId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	ENDHidingSpotKind Kind = ENDHidingSpotKind::BehindFurniture;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	int32 Capacity = 1;

	/** Cuántos Desvelados hay dentro ahora. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	int32 Occupants = 0;

	/** Centro del mueble, en el mundo. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	FVector FurnitureLocation = FVector::ZeroVector;

	/** Por donde se entra y se sale, en el mundo. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Hiding")
	FVector EntryLocation = FVector::ZeroVector;

	bool IsValid() const { return SpotId != INDEX_NONE; }
	bool IsFull() const { return Occupants >= Capacity; }
};

/**
 * Esconderse en los muebles de la casa generada (GDD 3.4 y 14.5): debajo de camas y mesas, dentro de
 * armarios o detrás de sillones. Los escondites son los del layout de la casa, así que no hay que
 * colocar nada en el mapa. El cliente pide, el servidor valida distancia, capacidad y manos libres.
 * Escondido no se anda ni se hace ruido; se sale con E (o si te capturan).
 */
UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDHidingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDHidingComponent();

	/** true si está dentro de un escondite. La IA y la UI lo leen. */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	bool IsHidden() const { return HiddenSpotId != INDEX_NONE; }

	/** Escondite donde está, o INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	int32 GetHiddenSpotId() const { return HiddenSpotId; }

	/** Tipo del escondite donde está (válido si IsHidden). */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	ENDHidingSpotKind GetHiddenSpotKind() const { return HiddenSpotKind; }

	/**
	 * Cuánto se le ve, de 0 a 1 (GDD 9.5: postura + movimiento + cobertura). 0 dentro o debajo de un
	 * mueble, 0.3 detrás de uno, 0.45 agachado quieto, 1 corriendo o con la linterna encendida.
	 */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	float GetVisibilityFactor() const;

	/** Escondite más cercano al que podría entrar ahora (en local, para el texto de E). */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	FNDHidingSpotInfo FindBestSpot() const;

	/** Entra en el escondite más cercano o sale del actual. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Hiding")
	void RequestToggleHide();

	/** true si lo que lleva en las manos le impide esconderse (todo menos lo ligero y lo de bolsillo). */
	UFUNCTION(BlueprintPure, Category = "NDE|Hiding")
	bool HandsTooFull() const;

	/** Solo servidor: saca al jugador del escondite (captura, fin de partida). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Hiding")
	void ForceUnhide();

	/** Todos los escondites de la casa del mundo, con su ocupación. */
	static TArray<FNDHidingSpotInfo> GetAllSpots(const UObject* WorldContextObject);

	/** Distancia máxima (cm, en horizontal) desde la entrada del escondite para poder entrar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Hiding", meta = (ClampMin = "50"))
	float EnterRange = 140.f;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Hiding")
	FNDOnHiddenChanged OnHiddenChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerToggleHide();

	UFUNCTION()
	void OnRep_HiddenSpotId();

	/** Servidor: mete al jugador en el escondite. */
	void EnterSpot(const FNDHidingSpotInfo& Spot);

	/** Servidor: lo saca por la entrada. */
	void ExitSpot();

	/** Cuerpo, colisión y movimiento según esté escondido o no. En todas las máquinas. */
	void ApplyHiddenState();

	ACharacter* GetCharacter() const;

	UPROPERTY(ReplicatedUsing = OnRep_HiddenSpotId)
	int32 HiddenSpotId = INDEX_NONE;

	UPROPERTY(Replicated)
	ENDHidingSpotKind HiddenSpotKind = ENDHidingSpotKind::BehindFurniture;

	/** Dónde reaparece al salir (servidor). */
	FVector ExitLocation = FVector::ZeroVector;

	bool bHiddenStateApplied = false;
};
