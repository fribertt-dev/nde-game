#include "UI/Data/NDUIDataProvider.h"

bool UNDUIDataProvider::IsSimulated_Implementation() const { return false; }
FNDPlayerCardVM UNDUIDataProvider::GetLocalPlayer_Implementation() const { return FNDPlayerCardVM(); }
int32 UNDUIDataProvider::GetLocalMoney_Implementation() const { return 0; }
TArray<FNDPlayerCardVM> UNDUIDataProvider::GetLobbyPlayers_Implementation() const { return {}; }
TArray<FNDContractCardVM> UNDUIDataProvider::GetAvailableContracts_Implementation() const { return {}; }
TArray<FNDShopItemVM> UNDUIDataProvider::GetShopItems_Implementation() const { return {}; }
TArray<FNDInventorySlotVM> UNDUIDataProvider::GetLoadout_Implementation() const { return {}; }
FNDNightClockVM UNDUIDataProvider::GetNightClock_Implementation() const { return FNDNightClockVM(); }
TArray<FNDPlayerVoiceVM> UNDUIDataProvider::GetVoiceStates_Implementation() const { return {}; }
FNDResultsVM UNDUIDataProvider::GetLastResults_Implementation() const { return FNDResultsVM(); }

void UNDUIDataProvider::RequestSelectSpecies_Implementation(ENDDesveladoSpecies Species)
{
	NotifyCommandResult(TEXT("SelectSpecies"), false, NSLOCTEXT("NDUI", "NotImplemented", "No disponible todavía"));
}

void UNDUIDataProvider::RequestSetReady_Implementation(bool bReady)
{
	NotifyCommandResult(TEXT("SetReady"), false, NSLOCTEXT("NDUI", "NotImplemented", "No disponible todavía"));
}

void UNDUIDataProvider::RequestVoteContract_Implementation(FName ContractId)
{
	NotifyCommandResult(TEXT("VoteContract"), false, NSLOCTEXT("NDUI", "NotImplemented", "No disponible todavía"));
}

void UNDUIDataProvider::RequestPurchase_Implementation(FName ItemId)
{
	NotifyCommandResult(TEXT("Purchase"), false, NSLOCTEXT("NDUI", "NotImplemented", "No disponible todavía"));
}

void UNDUIDataProvider::RequestEquip_Implementation(int32 SlotIndex, FName ItemId)
{
	NotifyCommandResult(TEXT("Equip"), false, NSLOCTEXT("NDUI", "NotImplemented", "No disponible todavía"));
}

void UNDUIDataProvider::NotifyDataChanged(FName Topic)
{
	OnDataChanged.Broadcast(Topic);
}

void UNDUIDataProvider::NotifyCommandResult(FName Command, bool bSuccess, const FText& Message)
{
	FNDUICommandResult Result;
	Result.Command = Command;
	Result.bSuccess = bSuccess;
	Result.bSimulated = IsSimulated();
	Result.Message = Message;
	OnCommandResult.Broadcast(Result);
}
