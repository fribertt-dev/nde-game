#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "NDHUD.generated.h"

// Existe solo en el jugador local: inicializa la UI y abre la pantalla inicial del mapa.
// Los GameModes la eligen con HUDClass, así la UI no depende de ningún PlayerController concreto.
UCLASS()
class NDE_API ANDHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// Pantalla que se abre al entrar en el mapa. Vacía = ninguna.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Screen"))
	FGameplayTag InitialScreen;

	// Si P, Esc o Start abren el menú de pausa (UI.Screen.Pause) en este mapa.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	bool bAllowPauseMenu = true;

private:
	void HandlePausePressed();
};

// HUD de La Madriguera: solo inicializa la UI para la pausa; el panel de la sala lo pone ANDPlayerController.
UCLASS()
class NDE_API ANDLobbyHUD : public ANDHUD
{
	GENERATED_BODY()
};

// HUD del menú principal: abre UI.Screen.MainMenu.
UCLASS()
class NDE_API ANDFrontEndHUD : public ANDHUD
{
	GENERATED_BODY()

public:
	ANDFrontEndHUD();
};

// HUD de la expedición: abre UI.Screen.ExpeditionHUD (reloj, cuota, objetivo, carga y avisos).
UCLASS()
class NDE_API ANDExpeditionHUD : public ANDHUD
{
	GENERATED_BODY()

public:
	ANDExpeditionHUD();
};
