#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "NDUIPauseScreen.generated.h"

class UButton;
class UTextBlock;

/**
 * Menú de pausa (UI.Screen.Pause, GDD 32.6), hecho en C++ hasta tener su WBP.
 * No detiene la partida: los demás siguen jugando. Seguir, volver a La Madriguera (solo el
 * host, en la casa), salir al menú o cerrar el juego, y la lista de teclas.
 * Se abre y se cierra con P, Esc o Start (lo pide ANDHUD).
 */
UCLASS()
class NDE_API UNDUIPauseScreen : public UNDUIBaseScreen
{
	GENERATED_BODY()

public:
	UNDUIPauseScreen(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

	/** Teclas que abren y cierran la pausa. */
	static bool IsPauseKey(const FKey& Key);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnToHideoutClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UButton* AddButton(class UVerticalBox& Box, const FText& Label);

	/** true si este jugador es el host de una expedición en marcha. */
	bool CanReturnToHideout() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Buttons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> Labels;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnToHideoutButton;

	TWeakObjectPtr<UButton> Highlighted;
	bool bFocusPending = false;
};
