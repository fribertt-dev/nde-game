// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NDMonsterSubsystem.generated.h"

class ANDHouseLot;
class ANDMonsterCage;
class ANDMonsterCharacter;
class APawn;

/**
 * Pone la criatura en su cama cuando se construye la casa generada (también al regenerarla) y le da lo
 * que necesita saber de la casa: recorrido de patrulla, sitios de búsqueda y la jaula. Lleva la cuenta de
 * los manotazos (el primero aturde, el segundo encierra) y la captura en la expedición.
 * Todo lo que decide es del servidor. Sin casa generada (mapa de prueba) la criatura se coloca a mano.
 */
UCLASS()
class NDE_API UNDMonsterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNDMonsterSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Criaturas vivas en el mundo (servidor y clientes). */
	TArray<ANDMonsterCharacter*> GetMonsters() const;

	/** La criatura más cercana a un punto, o nullptr. */
	ANDMonsterCharacter* FindNearestMonster(const FVector& Location) const;

	void RegisterCage(ANDMonsterCage* Cage);

	// --- Casa (servidor y clientes: sale del layout, igual en todas las máquinas).

	/** Recorrido de patrulla de la casa en coordenadas de mundo (sale del dormitorio y vuelve). Vacío sin casa. */
	void GetPatrolRoute(TArray<FVector>& OutPoints) const;

	/** Hasta Count sitios para buscar cerca de un punto (nodos de patrulla de salas cercanas, del mismo piso). */
	void GetSearchPoints(const FVector& Around, int32 Count, TArray<FVector>& OutPoints) const;

	// --- Captura (solo servidor).

	/**
	 * Manotazo a un Desvelado. El primero (o tras StrikeMemorySeconds) lo aturde y lo empuja; el segundo lo
	 * encierra en la jaula y lo marca capturado en la expedición. Devuelve true si lo ha encerrado.
	 */
	bool HandleGrab(ANDMonsterCharacter* Monster, APawn* Victim);

	/** Jaula de la casa (la crea si falta). */
	ANDMonsterCage* GetOrSpawnCage(const FVector& Near);

	/** Crea la criatura en la cama de la casa (borra la anterior). Lo llama solo al construirse la casa. */
	ANDMonsterCharacter* SpawnForHouse(ANDHouseLot* Lot);

	/** Hora del último manotazo a cada pawn (servidor). */
	double GetLastStrikeTime(const APawn* Pawn) const;

protected:
	UFUNCTION()
	void HandleHouseBuilt(ANDHouseLot* Lot);

	void TryBindToLot();
	FVector FindCageLocation(ANDHouseLot* Lot) const;
	bool IsSpotFree(const FVector& FloorLocation) const;
	ANDMonsterCage* SpawnCage(const FVector& FloorLocation, float Yaw);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ANDMonsterCage>> Cages;

	/** Lo que este subsistema creó al construirse la casa (se borra al regenerar). */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedForHouse;

	TWeakObjectPtr<ANDHouseLot> BoundLot;
	TMap<TWeakObjectPtr<APawn>, double> LastStrike;
	FTimerHandle FindLotTimer;
	int32 FindLotTries = 0;
};
