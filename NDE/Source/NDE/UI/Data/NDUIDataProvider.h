#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Data/NDUIViewModels.h"
#include "NDUIDataProvider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDUIOnDataChanged, FName, Topic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDUIOnCommandResult, const FNDUICommandResult&, Result);

// Temas que se pasan en OnDataChanged para que cada pantalla refresque solo lo suyo.
namespace NDUITopics
{
	inline const FLazyName Profile = TEXT("Profile");
	inline const FLazyName Lobby = TEXT("Lobby");
	inline const FLazyName Contracts = TEXT("Contracts");
	inline const FLazyName Shop = TEXT("Shop");
	inline const FLazyName Loadout = TEXT("Loadout");
	inline const FLazyName NightClock = TEXT("NightClock");
	inline const FLazyName Voice = TEXT("Voice");
	inline const FLazyName Results = TEXT("Results");
	inline const FLazyName ExpeditionHUD = TEXT("ExpeditionHUD");
}

/**
 * Fuente de datos de la UI (BPI_UIDataProvider en GDD 32.3).
 * UNDUIMockDataProvider sirve datos simulados desde el día 1; un proveedor real que lea
 * GameState, PlayerState y subsistemas lo sustituye sin tocar los WBP.
 * Los comandos solo piden; el resultado llega por OnCommandResult y los datos por OnDataChanged.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class NDE_API UNDUIDataProvider : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	bool IsSimulated() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	FNDPlayerCardVM GetLocalPlayer() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	int32 GetLocalMoney() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	TArray<FNDPlayerCardVM> GetLobbyPlayers() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	TArray<FNDContractCardVM> GetAvailableContracts() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	TArray<FNDShopItemVM> GetShopItems() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	TArray<FNDInventorySlotVM> GetLoadout() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	FNDNightClockVM GetNightClock() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	TArray<FNDPlayerVoiceVM> GetVoiceStates() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	FNDResultsVM GetLastResults() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NDE|UI|Data")
	FNDExpeditionHUDVM GetExpeditionHUD() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|UI|Commands")
	void RequestSelectSpecies(ENDDesveladoSpecies Species);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|UI|Commands")
	void RequestSetReady(bool bReady);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|UI|Commands")
	void RequestVoteContract(FName ContractId);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|UI|Commands")
	void RequestPurchase(FName ItemId);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NDE|UI|Commands")
	void RequestEquip(int32 SlotIndex, FName ItemId);

	UPROPERTY(BlueprintAssignable, Category = "NDE|UI|Data")
	FNDUIOnDataChanged OnDataChanged;

	UPROPERTY(BlueprintAssignable, Category = "NDE|UI|Commands")
	FNDUIOnCommandResult OnCommandResult;

protected:
	UFUNCTION(BlueprintCallable, Category = "NDE|UI|Data")
	void NotifyDataChanged(FName Topic);

	UFUNCTION(BlueprintCallable, Category = "NDE|UI|Commands")
	void NotifyCommandResult(FName Command, bool bSuccess, const FText& Message);
};
