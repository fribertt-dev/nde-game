// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Core/NDTypes.h"
#include "Networking/NDSessionSubsystem.h"
#include "NDGameInstance.generated.h"

class UNetDriver;

/**
 * Vive durante toda la ejecución, en cliente y en host (GDD 22.2).
 * Gestiona transiciones entre mapas y fallos de red. No se replica ni sustituye
 * al servicio de perfil.
 */
UCLASS()
class NDE_API UNDGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Sale de la sala y vuelve al menú principal mostrando el motivo. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Red")
	void ReturnToMainMenuWithReason(ENDDisconnectReason Reason);

	/** Motivo de la última salida, para que el menú lo muestre una vez. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Red")
	ENDDisconnectReason ConsumeLastDisconnectReason();

	/** Comandos de consola para probar salas sin UI. */
	UFUNCTION(Exec)
	void NDHost(bool bFriendsOnly = false);

	UFUNCTION(Exec)
	void NDFind();

	UFUNCTION(Exec)
	void NDJoin(int32 SearchIndex);

	UFUNCTION(Exec)
	void NDInvite();

	UFUNCTION(Exec)
	void NDLeave();

	void SetLastDisconnectReason(ENDDisconnectReason Reason) { LastDisconnectReason = Reason; }

	/** Jugadores que salieron de La Madriguera hacia la expedición (solo host). */
	int32 GetExpectedExpeditionPlayers() const { return ExpectedExpeditionPlayers; }
	void SetExpectedExpeditionPlayers(int32 Count) { ExpectedExpeditionPlayers = Count; }

protected:
	virtual void OnStart() override;

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UFUNCTION()
	void LogFoundSessions(const TArray<FNDSessionInfo>& Results, bool bWasSuccessful);

	ENDDisconnectReason LastDisconnectReason = ENDDisconnectReason::None;
	int32 ExpectedExpeditionPlayers = 0;
	bool bReturningToMenu = false;
};
