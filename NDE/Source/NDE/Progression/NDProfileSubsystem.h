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

	/** Monedas del perfil local. */
	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int64 GetCoins() const;

	/** Reputación del perfil local. */
	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int32 GetReputation() const;

	/** Expediciones terminadas con este perfil. */
	UFUNCTION(BlueprintPure, Category = "NDE|Profile")
	int32 GetExpeditionsCompleted() const;

	/** true si el perfil tiene este desbloqueo. */
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

	/** Añade un desbloqueo al perfil y lo guarda. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Profile")
	void Unlock(FName UnlockId);

	/** Se dispara cuando cambian monedas, reputación o desbloqueos. */
	UPROPERTY(BlueprintAssignable, Category = "NDE|Profile")
	FNDOnProfileChanged OnProfileChanged;

private:
	void Save();
	FString GetSlotName() const;

	static constexpr int32 MaxRememberedRuns = 50;

	UPROPERTY(Transient)
	TObjectPtr<UNDProfileSaveGame> Profile;
};
