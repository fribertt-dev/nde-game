// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "NDSessionSubsystem.generated.h"

/** Resumen de una sala encontrada, apto para UI. */
USTRUCT(BlueprintType)
struct FNDSessionInfo
{
	GENERATED_BODY()

	/** Índice dentro de la última búsqueda; se pasa a JoinSessionByIndex. */
	UPROPERTY(BlueprintReadOnly)
	int32 SearchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FString HostName;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly)
	FString CompatibilityVersion;

	UPROPERTY(BlueprintReadOnly)
	bool bIsCompatible = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNDOnSessionOperationComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNDOnFindSessionsComplete, const TArray<FNDSessionInfo>&, Results, bool, bWasSuccessful);

/**
 * Salas de juego: crear, buscar, unirse, invitar y salir (GDD 22.2).
 * Habla con la interfaz genérica de sesiones de Unreal, no con Steam directamente,
 * para poder sumar otras plataformas más adelante. Sin Steam (PIE) usa el
 * subsistema NULL con salas LAN.
 */
UCLASS()
class NDE_API UNDSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/**
	 * Engancha los avisos de la plataforma (invitaciones aceptadas). Lo llama
	 * UNDGameInstance::OnStart, cuando ya hay mundo y subsistema online.
	 */
	void BindPlatformDelegates();

	/** Crea la sala y, al completarse, abre La Madriguera como listen server. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Session")
	void HostSession(bool bFriendsOnly);

	UFUNCTION(BlueprintCallable, Category = "NDE|Session")
	void FindSessions(int32 MaxResults = 50);

	UFUNCTION(BlueprintCallable, Category = "NDE|Session")
	void JoinSessionByIndex(int32 SearchIndex);

	/** Abre la ventana de invitaciones de la plataforma (overlay de Steam). */
	UFUNCTION(BlueprintCallable, Category = "NDE|Session")
	bool ShowInviteUI();

	/** Destruye la sala local. No viaja; para volver al menú usar UNDGameInstance::ReturnToMainMenuWithReason. */
	UFUNCTION(BlueprintCallable, Category = "NDE|Session")
	void DestroySession();

	/** Cierra o abre la sala a nuevas entradas (p. ej. durante una expedición). */
	void SetSessionJoinable(bool bJoinable);

	UFUNCTION(BlueprintPure, Category = "NDE|Session")
	bool HasActiveSession() const;

	/** true si el subsistema activo es NULL (sin Steam): las salas son LAN. */
	UFUNCTION(BlueprintPure, Category = "NDE|Session")
	bool IsUsingLAN() const;

	/** Dirección del último host al que nos unimos, para volver a una expedición. */
	const FString& GetLastConnectString() const { return LastConnectString; }

	UPROPERTY(BlueprintAssignable, Category = "NDE|Session")
	FNDOnSessionOperationComplete OnHostSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Session")
	FNDOnFindSessionsComplete OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Session")
	FNDOnSessionOperationComplete OnJoinSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "NDE|Session")
	FNDOnSessionOperationComplete OnDestroySessionComplete;

	/** Claves propias en los ajustes de sala. */
	static const FName SettingGameKey;
	static const FName SettingVersionKey;
	static const FString GameKeyValue;

private:
	IOnlineSessionPtr GetSessionInterface() const;
	FUniqueNetIdRepl GetLocalUserId() const;
	void JoinSearchResult(const FOnlineSessionSearchResult& Result);
	void TravelToLobbyAsHost();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	TSharedPtr<FOnlineSessionSearch> LastSearch;
	FString LastConnectString;

	/** Si al terminar de destruir una sala hay que crear otra (re-host). */
	TOptional<bool> PendingHostFriendsOnly;
	/** Si al terminar de destruir una sala hay que unirse a otra (invitación aceptada estando en sala). */
	TOptional<FOnlineSessionSearchResult> PendingJoin;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle InviteAcceptedHandle;
};
