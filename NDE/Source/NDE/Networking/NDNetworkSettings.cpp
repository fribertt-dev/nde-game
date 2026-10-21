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
}
