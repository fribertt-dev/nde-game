// Transporte de botín por un jugador (GDD 3.3, 5.5, 8.2 y 8.4).
// El cliente pide; el servidor valida distancia y estado y cambia al portador.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "NDCarryComponent.generated.h"

class ANDLootActor;
class APawn;

UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDCarryComponent();

	/** Coger o unirse a un bulto. Los objetos de bolsillo van a la mochila. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Carry")
	void RequestGrab(ANDLootActor* Target);

	UFUNCTION(BlueprintCallable, Category = "NDE|Carry")
	void RequestDrop();

	/** Lanzar lo que lleva en las manos. Charge va de 0 a 1. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Carry")
	void RequestThrow(FVector Direction, float Charge);

	/** Mejor objetivo de botín delante de la cámara, calculado en local. */
	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	ANDLootActor* FindBestLootTarget() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	ANDLootActor* GetHeldLoot() const { return HeldLoot; }

	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	TArray<ANDLootActor*> GetPocketLoot() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	bool HasFreePocketSlot() const { return PocketLoot.Num() < PocketSlots; }

	/** Multiplicador de velocidad por la carga actual (GDD 8.2). */
	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	float GetSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Carry")
	bool CanSprint() const;

	/** Todo lo que lleva encima: manos y mochila. Para depositar en la camioneta. */
	TArray<ANDLootActor*> GetAllCarriedLoot() const;

	/** Servidor: suelta todo (captura, desconexión o golpe crítico). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Carry")
	void DropEverything();

	/** Lo llama ANDLootActor cuando este jugador deja de llevar el objeto. */
	void NotifyLootReleased(ANDLootActor* Loot);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Carry")
	float InteractionRange = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Carry")
	float InteractionAngleDegrees = 55.f;

	/** Inventario secundario no voluminoso (GDD 5.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Carry")
	int32 PocketSlots = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Carry")
	float ThrowSpeedMin = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Carry")
	float ThrowSpeedMax = 900.f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Si el jugador desaparece (se desconecta, muere) lo que llevaba cae donde estaba. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerGrab(ANDLootActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerDrop();

	UFUNCTION(Server, Reliable)
	void ServerThrow(FVector_NetQuantizeNormal Direction, float Charge);

	APawn* GetPawn() const;
	bool IsWithinReach(const ANDLootActor* Loot) const;

	UPROPERTY(Replicated)
	TObjectPtr<ANDLootActor> HeldLoot;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ANDLootActor>> PocketLoot;
};
