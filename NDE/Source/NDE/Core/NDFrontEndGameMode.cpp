#include "Core/NDFrontEndGameMode.h"

#include "Core/NDPlayerController.h"

ANDFrontEndGameMode::ANDFrontEndGameMode()
{
	PlayerControllerClass = ANDFrontEndPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
