// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NDTypes.h"
#include "Networking/NDSessionSubsystem.h"
#include "NDMainMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Lógica del menú principal: crear, buscar y unirse a salas, y salir.
 * La apariencia va en un Blueprint hijo (WBP_MenuPrincipal). Si el hijo no
 * tiene diseño propio, se monta una disposición mínima de botones y texto.
 */
UCLASS()
class NDE_API UNDMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Crea una sala y abre La Madriguera como host. bFriendsOnly: solo amigos de Steam pueden entrar. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void HostSession(bool bFriendsOnly = false);

	/** Busca salas abiertas; el resultado llega en OnSessionsFound. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void FindSessions();

	/** SearchIndex de FNDSessionInfo, de la última búsqueda. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void JoinSession(int32 SearchIndex);

	/** Se une a la primera sala compatible de la última búsqueda. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void JoinFirstCompatibleSession();

	/** Cierra el juego. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void QuitGame();

	/** true si hay una expedición a la que aún podemos volver. */
	UFUNCTION(BlueprintPure, Category = "NDE|Menu")
	bool CanRejoinExpedition() const;

	/** Vuelve a la expedición de la que te desconectaste, si aún hay tiempo. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Menu")
	void RejoinExpedition();

	/** Salas encontradas en la última búsqueda. */
	UFUNCTION(BlueprintPure, Category = "NDE|Menu")
	const TArray<FNDSessionInfo>& GetLastSessions() const { return LastSessions; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Terminó la búsqueda de salas; aquí se rellena la lista. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Menu")
	void OnSessionsFound(const TArray<FNDSessionInfo>& Sessions, bool bWasSuccessful);

	/** Motivo de la última salida al menú, o fallo al crear o unirse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Menu")
	void OnSessionError(ENDDisconnectReason Reason);

	/** Texto de estado opcional; el Blueprint puede ignorarlo. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleHostComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleFindComplete(const TArray<FNDSessionInfo>& Results, bool bWasSuccessful);

	UFUNCTION()
	void HandleJoinComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleHostClicked() { HostSession(false); }

	UFUNCTION()
	void HandleQuitClicked() { QuitGame(); }

	UFUNCTION()
	void HandleFindClicked() { FindSessions(); }

	UFUNCTION()
	void HandleJoinClicked() { JoinFirstCompatibleSession(); }

	UFUNCTION()
	void HandleRejoinClicked() { RejoinExpedition(); }

	void BuildFallbackLayout();
	UButton* AddFallbackButton(class UVerticalBox* Box, const FText& Label);
	void SetStatus(const FString& Message);
	UNDSessionSubsystem* GetSessions() const;

	TArray<FNDSessionInfo> LastSessions;
};
