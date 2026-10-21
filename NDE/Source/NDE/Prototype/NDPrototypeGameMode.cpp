#include "Prototype/NDPrototypeGameMode.h"

#include "GameFramework/Pawn.h"
#include "Prototype/NDPrototypeCharacter.h"
#include "Tools/NDLoadoutComponent.h"
#include "Tools/NDToolDefinition.h"

ANDPrototypeGameMode::ANDPrototypeGameMode()
{
	DefaultPawnClass = ANDPrototypeCharacter::StaticClass();

	ToolA = TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_T03.DA_Tool_T03")));
	ToolB = TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_T07.DA_Tool_T07")));
	PingTool = TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_T02.DA_Tool_T02")));
	Consumables.Add(TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_T17.DA_Tool_T17"))));
	Consumables.Add(TSoftObjectPtr<UNDToolDefinition>(FSoftObjectPath(TEXT("/Game/_NDE_/Data/Tools/DA_Tool_T18.DA_Tool_T18"))));
}

void ANDPrototypeGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	UNDLoadoutComponent* Loadout = PlayerPawn ? PlayerPawn->FindComponentByClass<UNDLoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return;
	}

	UNDToolDefinition* Consumable = nullptr;
	if (Consumables.Num() > 0)
	{
		Consumable = Consumables[NextConsumableIndex++ % Consumables.Num()].LoadSynchronous();
	}
	Loadout->PingTool = PingTool.LoadSynchronous();
	Loadout->SetLoadout(ToolA.LoadSynchronous(), ToolB.LoadSynchronous(), Consumable, 0);
}
