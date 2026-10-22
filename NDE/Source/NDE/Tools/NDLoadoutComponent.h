// Equipamiento en misión: dos herramientas activas y un consumible rápido (GDD 5.5 y 18.2).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "NDLoadoutComponent.generated.h"

class AActor;
class APawn;
class UNDToolDefinition;

USTRUCT(BlueprintType)
struct NDE_API FNDToolSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TObjectPtr<UNDToolDefinition> Tool = nullptr;

	/** Usos restantes. -1 = ilimitado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	int32 Charges = -1;

	UPROPERTY(BlueprintReadOnly, Category = "NDE|Tools")
	double ReadyServerTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNDToolUsedSignature, UNDToolDefinition*, Tool, FVector, AimPoint, AActor*, SpawnedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDPingSignature, APawn*, Pinger, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDFlashlightChangedSignature, bool, bOn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNDLoadoutChangedSignature);

UCLASS(ClassGroup = (NDE), meta = (BlueprintSpawnableComponent))
class NDE_API UNDLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNDLoadoutComponent();

	/** Equipo por defecto del corte gris. Luego lo fijará el bloqueo de loadout de la Madriguera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TArray<TObjectPtr<UNDToolDefinition>> DefaultActiveTools;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TObjectPtr<UNDToolDefinition> DefaultConsumable;

	/** Stack del consumible por defecto. 0 = usar Charges de la definición. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools", meta = (ClampMin = 0))
	int32 DefaultConsumableCount = 0;

	/** T02, ping silencioso: básico y siempre disponible, no ocupa ranura. Fija el cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Tools")
	TObjectPtr<UNDToolDefinition> PingTool;

	/** Servidor: fija el equipo para la expedición (LoadoutLock). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NDE|Tools")
	void SetLoadout(UNDToolDefinition* ToolA, UNDToolDefinition* ToolB, UNDToolDefinition* Consumable, int32 ConsumableCount);

	UFUNCTION(BlueprintCallable, Category = "NDE|Tools")
	void RequestSelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "NDE|Tools")
	void RequestUseActiveTool(FVector AimPoint);

	UFUNCTION(BlueprintCallable, Category = "NDE|Tools")
	void RequestUseConsumable(FVector AimPoint);

	UFUNCTION(BlueprintCallable, Category = "NDE|Tools")
	void RequestPing(FVector AimPoint);

	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	const TArray<FNDToolSlot>& GetActiveSlots() const { return ActiveSlots; }

	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	const FNDToolSlot& GetConsumableSlot() const { return ConsumableSlot; }

	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	int32 GetSelectedSlot() const { return SelectedSlot; }

	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	UNDToolDefinition* GetSelectedTool() const;

	/** El carrito es pasivo: basta con llevarlo en cualquiera de las dos ranuras. */
	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	bool HasCart() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Tools")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	/** Para efectos hechos en Blueprint o para sonido y VFX. Se emite en el servidor. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Tools")
	FNDToolUsedSignature OnToolUsed;

	/** Se emite en todas las máquinas. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Tools")
	FNDPingSignature OnPing;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Tools")
	FNDFlashlightChangedSignature OnFlashlightChanged;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Tools")
	FNDLoadoutChangedSignature OnLoadoutChanged;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUseActiveTool(FVector_NetQuantize AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerUseConsumable(FVector_NetQuantize AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerPing(FVector_NetQuantize AimPoint);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPing(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_Flashlight();

	UFUNCTION()
	void OnRep_Loadout();

	/** Ejecuta el efecto en el servidor. Devuelve true si se gastó un uso. */
	bool UseSlot(FNDToolSlot& Slot, const FVector& AimPoint);
	double GetServerNow() const;

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	TArray<FNDToolSlot> ActiveSlots;

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	FNDToolSlot ConsumableSlot;

	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	int32 SelectedSlot = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Flashlight)
	bool bFlashlightOn = false;

	/** Ranura implícita del ping (solo servidor). */
	FNDToolSlot PingSlot;
};
