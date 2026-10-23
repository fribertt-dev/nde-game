// No Despiertes Eso

#include "Networking/NDNetworkSettings.h"

#include "UI/NDLobbyStatusWidget.h"
#include "UI/NDMainMenuWidget.h"

UNDNetworkSettings::UNDNetworkSettings()
{
	MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_MenuPrincipal.L_MenuPrincipal")));
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_Madriguera.L_Madriguera")));
	ExpeditionTestMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_CasaPrueba.L_CasaPrueba")));
	// Sin WBP propio se usan las clases C++, que montan una disposición mínima.
	MainMenuWidgetClass = UNDMainMenuWidget::StaticClass();
	LobbyStatusWidgetClass = UNDLobbyStatusWidget::StaticClass();

	auto AddContract = [this](const TCHAR* Id, const FText& Name)
	{
		FNDContractOption& Option = ContractOptions.AddDefaulted_GetRef();
		Option.ContractId = FName(Id);
		Option.DisplayName = Name;
	};
	// El id es el del Data Asset (DA_Contract_C01...), el que busca ANDHouseGameMode con ?Contract=.
	AddContract(TEXT("C01"), NSLOCTEXT("NDE", "ContractC01", "La Primera Siesta"));
	AddContract(TEXT("C03"), NSLOCTEXT("NDE", "ContractC03", "El Reloj Perdido"));
	AddContract(TEXT("C07"), NSLOCTEXT("NDE", "ContractC07", "El Televisor Prestado"));
}
