// No Despiertes Eso

#include "Networking/NDNetworkSettings.h"

UNDNetworkSettings::UNDNetworkSettings()
{
	MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_MenuPrincipal.L_MenuPrincipal")));
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_Madriguera.L_Madriguera")));
	ExpeditionTestMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/_NDE_/Levels/L_Expedicion_Test.L_Expedicion_Test")));
}
