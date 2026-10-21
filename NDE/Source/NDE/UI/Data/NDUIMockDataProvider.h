#pragma once

#include "CoreMinimal.h"
#include "UI/Data/NDUIDataProvider.h"
#include "Engine/TimerHandle.h"
#include "NDUIMockDataProvider.generated.h"

class UWorld;

/**
 * Datos simulados para navegar toda la UI sin backend (UIMockDataProvider, GDD 32.2).
 * Los valores por defecto salen del GDD y se pueden editar en un Blueprint hijo.
 * Los comandos cambian solo esta copia en memoria: nada se guarda y cada respuesta va marcada como simulada.
 */
UCLASS(Blueprintable)
class NDE_API UNDUIMockDataProvider : public UNDUIDataProvider
{
	GENERATED_BODY()

public:
	UNDUIMockDataProvider();

	virtual bool IsSimulated_Implementation() const override { return true; }
	virtual FNDPlayerCardVM GetLocalPlayer_Implementation() const override;
	virtual int32 GetLocalMoney_Implementation() const override { return Money; }
	virtual TArray<FNDPlayerCardVM> GetLobbyPlayers_Implementation() const override { return LobbyPlayers; }
	virtual TArray<FNDContractCardVM> GetAvailableContracts_Implementation() const override { return Contracts; }
	virtual TArray<FNDShopItemVM> GetShopItems_Implementation() const override { return ShopItems; }
	virtual TArray<FNDInventorySlotVM> GetLoadout_Implementation() const override { return Loadout; }
	virtual FNDNightClockVM GetNightClock_Implementation() const override { return NightClock; }
	virtual TArray<FNDPlayerVoiceVM> GetVoiceStates_Implementation() const override { return VoiceStates; }
	virtual FNDResultsVM GetLastResults_Implementation() const override { return Results; }

	virtual void RequestSelectSpecies_Implementation(ENDDesveladoSpecies Species) override;
	virtual void RequestSetReady_Implementation(bool bReady) override;
	virtual void RequestVoteContract_Implementation(FName ContractId) override;
	virtual void RequestPurchase_Implementation(FName ItemId) override;
	virtual void RequestEquip_Implementation(int32 SlotIndex, FName ItemId) override;

	// Arranca una noche simulada desde ahora, para probar el reloj, los avisos y la huida.
	UFUNCTION(BlueprintCallable, Category = "NDE|UI|Mock", meta = (WorldContext = "WorldContextObject"))
	void StartSimulatedNight(const UObject* WorldContextObject, float DurationSeconds = 600.f);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI|Mock")
	void SetSimulatedOutcome(ENDExpeditionOutcome Outcome);

protected:
	int32 FindLocalPlayerIndex() const;
	void HandleSimulatedDawn();
	void HandleSimulatedEscapeEnd();

	TWeakObjectPtr<UWorld> SimulatedNightWorld;
	FTimerHandle SimulatedNightTimer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	int32 Money = 400;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	TArray<FNDPlayerCardVM> LobbyPlayers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	TArray<FNDContractCardVM> Contracts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	TArray<FNDShopItemVM> ShopItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	TArray<FNDInventorySlotVM> Loadout;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	FNDNightClockVM NightClock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	TArray<FNDPlayerVoiceVM> VoiceStates;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mock")
	FNDResultsVM Results;

	UPROPERTY(Transient)
	FName VotedContractId;
};
