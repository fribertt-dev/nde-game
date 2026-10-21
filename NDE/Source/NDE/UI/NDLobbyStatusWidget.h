// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/NDTypes.h"
#include "NDLobbyStatusWidget.generated.h"

class ANDLobbyGameState;
class UTextBlock;

/**
 * Estado de La Madriguera: cuántos están listos, cuenta atrás y viaje.
 * Lee el GameState replicado y avisa al Blueprint hijo (WBP_EstadoSala) solo
 * cuando algo cambia. Sin diseño propio, muestra dos textos sencillos.
 */
UCLASS()
class NDE_API UNDLobbyStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Segundos que faltan para el viaje; 0 si no hay cuenta atrás. */
	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	float GetSecondsUntilTravel() const;

	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	int32 GetNumReadyPlayers() const { return LastReady; }

	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	int32 GetNumPlayers() const { return LastTotal; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Sala")
	void OnReadyCountChanged(int32 Ready, int32 Total);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Sala")
	void OnCountdownStarted(double TravelServerTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Sala")
	void OnCountdownCancelled();

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|Sala")
	void OnTraveling();

	/** "Listos 1/2" y la lista de jugadores. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Sala", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyText;

	/** "Saliendo en 3", "Viajando..." o la pista de la zona. */
	UPROPERTY(BlueprintReadOnly, Category = "NDE|Sala", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText;

private:
	const ANDLobbyGameState* GetLobbyState() const;
	void BuildFallbackLayout();
	void UpdateTexts(const ANDLobbyGameState& LobbyState);

	int32 LastReady = -1;
	int32 LastTotal = -1;
	ENDLobbyPhase LastPhase = ENDLobbyPhase::Waiting;
};
