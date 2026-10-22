#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NDFrontEndGameMode.generated.h"

// GameMode del mapa de menú (L_MainMenu): sin pawn, solo UI.
UCLASS()
class NDE_API ANDFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANDFrontEndGameMode();
};
