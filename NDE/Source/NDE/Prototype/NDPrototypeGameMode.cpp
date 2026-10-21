#include "Prototype/NDPrototypeGameMode.h"

#include "Prototype/NDPrototypeCharacter.h"

ANDPrototypeGameMode::ANDPrototypeGameMode()
{
	DefaultPawnClass = ANDPrototypeCharacter::StaticClass();
}
