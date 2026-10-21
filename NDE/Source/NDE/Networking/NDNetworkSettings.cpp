// No Despiertes Eso

#include "Networking/NDNetworkSettings.h"

#include "UI/NDLobbyStatusWidget.h"
#include "UI/NDMainMenuWidget.h"

UNDNetworkSettings::UNDNetworkSettings()
{
	MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_MenuPrincipal.L_MenuPrincipal")));
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_Madriguera.L_Madriguera")));
	ExpeditionTestMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_Expedicion.L_Expedicion")));
	// Sin WBP propio se usan las clases C++, que montan una disposición mínima.
	MainMenuWidgetClass = UNDMainMenuWidget::StaticClass();
	LobbyStatusWidgetClass = UNDLobbyStatusWidget::StaticClass();

	auto AddContract = [this](const TCHAR* Id, const FText& Name)
	{
		FNDContractOption& Option = ContractOptions.AddDefaulted_GetRef();
		Option.ContractId = FName(Id);
		Option.DisplayName = Name;
	};
	AddContract(TEXT("C01_Cookies"), NSLOCTEXT("NDE", "ContractC01", "Las galletas de la abuela"));
	AddContract(TEXT("C02_PocketWatch"), NSLOCTEXT("NDE", "ContractC02", "El reloj de bolsillo"));
	AddContract(TEXT("C03_ToyChest"), NSLOCTEXT("NDE", "ContractC03", "El baul de juguetes"));
}
