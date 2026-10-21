// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "NDPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDOnReadyChanged, class ANDPlayerState*, PlayerState, bool, bIsReady);

/**
 * Estado público por jugador (GDD 22.3): listo, y más adelante equipamiento
 * visible y resultados. La moneda personal no va aquí: llega por el perfil.
 */
UCLASS()
class NDE_API ANDPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category = "NDE|Sala")
	bool IsReady() const { return bIsReady; }

	/** Solo servidor. Los clientes piden el cambio con ANDPlayerController::ServerSetReady. */
	void SetReady(bool bNewReady);

	UPROPERTY(BlueprintAssignable, Category = "NDE|Sala")
	FNDOnReadyChanged OnReadyChanged;

private:
	UFUNCTION()
	void OnRep_IsReady();

	UPROPERTY(ReplicatedUsing = OnRep_IsReady)
	bool bIsReady = false;
};
