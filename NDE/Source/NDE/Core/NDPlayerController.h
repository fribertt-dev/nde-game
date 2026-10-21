#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "NDPlayerController.generated.h"

UCLASS()
class NDE_API ANDPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// Pantalla que se abre al entrar en el mapa. Vacía = ninguna.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Screen"))
	FGameplayTag InitialScreen;
};

// Controlador del menú principal: abre UI.Screen.MainMenu.
UCLASS()
class NDE_API ANDFrontEndPlayerController : public ANDPlayerController
{
	GENERATED_BODY()

public:
	ANDFrontEndPlayerController();
};
