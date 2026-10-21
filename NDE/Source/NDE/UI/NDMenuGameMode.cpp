// No Despiertes Eso

#include "UI/NDMenuGameMode.h"

#include "UI/NDMenuPlayerController.h"

ANDMenuGameMode::ANDMenuGameMode()
{
	PlayerControllerClass = ANDMenuPlayerController::StaticClass();
	bStartPlayersAsSpectators = true;
}
