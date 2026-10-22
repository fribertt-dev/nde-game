#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "NDUIResultsScreen.generated.h"

class UTextBlock;

/**
 * Resultado de la noche (UI.Screen.Results, GDD 7.2 y 32.5), hecho en C++ hasta tener su WBP:
 * Perfecto, Éxito, Extracción parcial o Fracaso, botín frente a cuota y la recompensa del jugador.
 * No pide nada: la vuelta a La Madriguera la decide el servidor.
 */
UCLASS()
class NDE_API UNDUIResultsScreen : public UNDUIBaseScreen
{
	GENERATED_BODY()

public:
	UNDUIResultsScreen(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

protected:
	virtual void NativeRefreshFromViewModel() override;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContractText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LootText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RewardText;
};
