// GameMode PROVISIONAL para abrir el nivel de prueba sin pasar por La Madriguera.
// En el flujo completo manda ANDExpeditionGameMode (frente de red).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDPrototypeGameMode.generated.h"

UCLASS()
class NDE_API ANDPrototypeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDPrototypeGameMode();
};
