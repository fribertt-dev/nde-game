#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "NDUIExpeditionHUD.generated.h"

class APawn;
class UBorder;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * HUD de la expedición (UI.Screen.ExpeditionHUD, GDD 32.6), hecho en C++ hasta tener su WBP.
 * Arriba en el centro, el reloj de la noche con su fase; arriba a la izquierda, el contrato, el
 * objetivo y el botín asegurado; abajo a la derecha, manos y mochila; en medio, la retícula con
 * el texto de interacción del personaje, y encima los avisos. Abajo a la izquierda, las teclas.
 * Solo lee view models del proveedor de datos. Al cerrarse la noche abre UI.Screen.Results.
 */
UCLASS()
class NDE_API UNDUIExpeditionHUD : public UNDUIBaseScreen
{
	GENERATED_BODY()

public:
	UNDUIExpeditionHUD(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

	/** Nombre visible de una fase del reloj. */
	static FText GetPhaseText(ENDUINightPhase Phase);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeRefreshFromViewModel() override;

	/** El reloj se interpola en local (GDD 32.6): solo el texto del tiempo se repinta a menudo. */
	void UpdateClock();

	UFUNCTION()
	void HandleToastRequested(const FNDUIToast& Toast);

	void ShowNextToast();
	void HideToast();

	/** Texto de interacción del personaje ("E  Coger...") y si está escondido. */
	void UpdateInteraction();

	/** Roncómetro de la criatura más cercana y aviso de que te ve. */
	void UpdateMonster(const APawn* Pawn);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> NightBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContractText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LootText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HandsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PocketText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ToastPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HiddenText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MonsterPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MonsterStateText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> SuspicionBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SnoreText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SeenText;

	FNDNightClockVM Clock;
	TArray<FNDUIToast> PendingToasts;
	bool bShowingToast = false;
	bool bResultsShown = false;
	FTimerHandle ClockTimer;
	FTimerHandle ToastTimer;
	FTimerHandle InteractionTimer;
};
