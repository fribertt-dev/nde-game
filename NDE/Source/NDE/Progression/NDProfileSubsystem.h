// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NDProfileSubsystem.generated.h"

class UNDProfileSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNDOnProfileChanged);

/**
 * Perfil del jugador local (GDD 22.2): monedas, reputación y desbloqueos.
 * Ahora guarda en disco local; la interfaz no cambia si más adelante pasa a un servicio.
 */
UCLASS()
class NDE_API UNDProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int64 GetCoins() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int32 GetReputation() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int32 GetExpeditionsCompleted() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	bool IsUnlocked(FName UnlockId) const;

	/**
	 * Cobra la recompensa de una expedición. Si ese RunGuid ya se cobró, no hace nada
	 * y devuelve false (reintentos o mensajes repetidos no duplican monedas).
	 */
	UFUNCTION(BlueprintCallable, Category = "NDE|Profile")
	bool ApplyRunReward(const FGuid& RunGuid, int32 Coins, int32 Reputation);

	/** Gasta monedas si alcanzan. Devuelve false si no hay saldo. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Profile")
	bool TrySpendCoins(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "NDE|Profile")
	void Unlock(FName UnlockId);

	UPROPERTY(BlueprintAssignable, Category = "NDE|Profile")
	FNDOnProfileChanged OnProfileChanged;

private:
	void Save();
	FString GetSlotName() const;

	static constexpr int32 MaxRememberedRuns = 50;

	UPROPERTY(Transient)
	TObjectPtr<UNDProfileSaveGame> Profile;
};
