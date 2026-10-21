#include "Core/NDFrontEndGameMode.h"

#include "UI/NDHUD.h"

ANDFrontEndGameMode::ANDFrontEndGameMode()
{
	HUDClass = ANDFrontEndHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
