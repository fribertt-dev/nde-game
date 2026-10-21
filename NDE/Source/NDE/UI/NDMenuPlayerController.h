// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NDMenuPlayerController.generated.h"

class UNDMainMenuWidget;

/**
 * Muestra el menú principal configurado en Project Settings > Game > NDE Red.
 * Si no hay clase configurada, no muestra nada y lo avisa en el log.
 */
UCLASS()
class NDE_API ANDMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UNDMainMenuWidget> MenuWidget;
};
