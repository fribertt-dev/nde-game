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

	/** Texto para el jugador. Las DisplayName del enum no existen fuera del editor. */
	UFUNCTION(BlueprintPure, Category = "NDE|Red")
	static FText GetDisconnectReasonText(ENDDisconnectReason Reason);

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

	UFUNCTION(Exec)
	void NDRejoin();

	/** true si salimos de una expedición en marcha y aún estamos a tiempo de volver. */
	UFUNCTION(BlueprintPure, Category = "NDE|Red")
	bool CanRejoinExpedition() const;

	/** Segundos que quedan para volver; 0 si no se puede. */
	UFUNCTION(BlueprintPure, Category = "NDE|Red")
	float GetRejoinSecondsLeft() const;

	/** Vuelve a conectar con el host de la expedición que dejamos. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Red")
	bool RejoinExpedition();

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

	/** Dirección del host y hora real límite para volver a la expedición (solo cliente). */
	FString RejoinAddress;
	double RejoinDeadline = 0.0;

	void RememberExpeditionForRejoin(ENDDisconnectReason Reason);
	bool bReturningToMenu = false;
};
