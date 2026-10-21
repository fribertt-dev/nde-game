// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDMenuGameMode.generated.h"

/** Menú principal: sin pawn jugable, solo el widget del menú. */
UCLASS()
class NDE_API ANDMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDMenuGameMode();
};
