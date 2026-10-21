// No Despiertes Eso

#include "Networking/NDSessionSubsystem.h"

#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Networking/NDNetworkSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

const FName UNDSessionSubsystem::SettingGameKey(TEXT("NDE_GAME"));
const FName UNDSessionSubsystem::SettingVersionKey(TEXT("NDE_VERSION"));
const FString UNDSessionSubsystem::GameKeyValue(TEXT("NoDespiertesEso"));

void UNDSessionSubsystem::Deinitialize()
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}
	Super::Deinitialize();
}

void UNDSessionSubsystem::BindPlatformDelegates()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || InviteAcceptedHandle.IsValid())
	{
		return;
	}
	InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
		FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSessionUserInviteAccepted));
}

IOnlineSessionPtr UNDSessionSubsystem::GetSessionInterface() const
{
	return Online::GetSessionInterface(GetWorld());
}

FUniqueNetIdRepl UNDSessionSubsystem::GetLocalUserId() const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
}

bool UNDSessionSubsystem::IsUsingLAN() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return !OSS || OSS->GetSubsystemName() == NULL_SUBSYSTEM;
}

bool UNDSessionSubsystem::HasActiveSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void UNDSessionSubsystem::HostSession(bool bFriendsOnly)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		UE_LOG(LogNDNet, Warning, TEXT("HostSession: no hay subsistema online o usuario local."));
		OnHostSessionComplete.Broadcast(false);
		return;
	}

	// Una sala anterior (p. ej. tras volver al menú) se destruye antes de crear la nueva.
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		PendingHostFriendsOnly = bFriendsOnly;
		DestroySession();
		return;
	}

	const UNDNetworkSettings* NetSettings = UNDNetworkSettings::Get();

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = NetSettings->MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = IsUsingLAN();
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = bFriendsOnly;
	Settings.bAllowInvites = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUsesStats = false;
	Settings.Set(SettingGameKey, GameKeyValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SettingVersionKey, NetSettings->CompatibilityVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));

	if (!Sessions->CreateSession(*UserId, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		OnHostSessionComplete.Broadcast(false);
	}
}

void UNDSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	UE_LOG(LogNDNet, Log, TEXT("Sala %s creada: %d (LAN=%d)"), *SessionName.ToString(), bWasSuccessful, IsUsingLAN());
	OnHostSessionComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful)
	{
		TravelToLobbyAsHost();
	}
}

void UNDSessionSubsystem::TravelToLobbyAsHost()
{
	const FString LobbyPackage = UNDNetworkSettings::Get()->LobbyMap.ToSoftObjectPath().GetLongPackageName();
	if (LobbyPackage.IsEmpty())
	{
		UE_LOG(LogNDNet, Error, TEXT("LobbyMap no configurado en Project Settings > NDE Red."));
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*LobbyPackage), true, TEXT("listen?game=Lobby"));
}

void UNDSessionSubsystem::FindSessions(int32 MaxResults)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		OnFindSessionsComplete.Broadcast({}, false);
		return;
	}

	LastSearch = MakeShared<FOnlineSessionSearch>();
	LastSearch->MaxSearchResults = MaxResults;
	LastSearch->bIsLanQuery = IsUsingLAN();
	LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	// El AppID 480 lo comparten muchos juegos de prueba: filtramos por nuestra clave.
	LastSearch->QuerySettings.Set(SettingGameKey, GameKeyValue, EOnlineComparisonOp::Equals);

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(*UserId, LastSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		OnFindSessionsComplete.Broadcast({}, false);
	}
}

void UNDSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	TArray<FNDSessionInfo> Results;
	if (bWasSuccessful && LastSearch.IsValid())
	{
		const FString& LocalVersion = UNDNetworkSettings::Get()->CompatibilityVersion;
		for (int32 Index = 0; Index < LastSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = LastSearch->SearchResults[Index];

			// El filtro del servidor de búsqueda no siempre se aplica (p. ej. en LAN).
			FString GameValue;
			if (!Result.Session.SessionSettings.Get(SettingGameKey, GameValue) || GameValue != GameKeyValue)
			{
				continue;
			}

			FNDSessionInfo& Info = Results.AddDefaulted_GetRef();
			Info.SearchIndex = Index;
			Info.HostName = Result.Session.OwningUserName;
			Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Info.PingMs = Result.PingInMs;
			Result.Session.SessionSettings.Get(SettingVersionKey, Info.CompatibilityVersion);
			Info.bIsCompatible = Info.CompatibilityVersion == LocalVersion;
		}
	}

	UE_LOG(LogNDNet, Log, TEXT("Busqueda de salas: %d resultados validos"), Results.Num());
	OnFindSessionsComplete.Broadcast(Results, bWasSuccessful);
}

void UNDSessionSubsystem::JoinSessionByIndex(int32 SearchIndex)
{
	if (!LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	const FOnlineSessionSearchResult& Result = LastSearch->SearchResults[SearchIndex];
	FString RemoteVersion;
	Result.Session.SessionSettings.Get(SettingVersionKey, RemoteVersion);
	if (RemoteVersion != UNDNetworkSettings::Get()->CompatibilityVersion)
	{
		if (UNDGameInstance* GI = Cast<UNDGameInstance>(GetGameInstance()))
		{
			GI->SetLastDisconnectReason(ENDDisconnectReason::VersionMismatch);
		}
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSearchResult(Result);
}

void UNDSessionSubsystem::JoinSearchResult(const FOnlineSessionSearchResult& Result)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdRepl UserId = GetLocalUserId();
	if (!Sessions.IsValid() || !UserId.IsValid())
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		PendingJoin = Result;
		DestroySession();
		return;
	}

	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));

	if (!Sessions->JoinSession(*UserId, NAME_GameSession, Result))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UNDSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	FString ConnectString;
	const bool bJoined = Result == EOnJoinSessionCompleteResult::Success
		&& Sessions.IsValid()
		&& Sessions->GetResolvedConnectString(SessionName, ConnectString);

	if (!bJoined)
	{
		UE_LOG(LogNDNet, Warning, TEXT("No se pudo entrar a la sala (resultado %d)"), static_cast<int32>(Result));
		if (UNDGameInstance* GI = Cast<UNDGameInstance>(GetGameInstance()))
		{
			GI->SetLastDisconnectReason(Result == EOnJoinSessionCompleteResult::SessionIsFull
				? ENDDisconnectReason::SessionFull
				: ENDDisconnectReason::JoinFailed);
		}
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	OnJoinSessionComplete.Broadcast(true);

	// La versión viaja en la URL para que el servidor rechace builds incompatibles (PreLogin).
	const FString URL = FString::Printf(TEXT("%s?NDEVersion=%s"), *ConnectString, *UNDNetworkSettings::Get()->CompatibilityVersion);
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		UE_LOG(LogNDNet, Log, TEXT("Viajando a %s"), *URL);
		PC->ClientTravel(URL, TRAVEL_Absolute);
	}
}

bool UNDSessionSubsystem::ShowInviteUI()
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineExternalUIPtr ExternalUI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	return ExternalUI.IsValid() && HasActiveSession() && ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

void UNDSessionSubsystem::SetSessionJoinable(bool bJoinable)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	FOnlineSessionSettings* Settings = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (!Settings)
	{
		return;
	}
	Settings->bAllowJoinInProgress = bJoinable;
	Settings->bAllowJoinViaPresence = bJoinable;
	Settings->bShouldAdvertise = bJoinable;
	Sessions->UpdateSession(NAME_GameSession, *Settings, true);
}

void UNDSessionSubsystem::DestroySession()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		HandleDestroySessionComplete(NAME_GameSession, true);
		return;
	}

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));

	if (!Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		HandleDestroySessionComplete(NAME_GameSession, false);
	}
}

void UNDSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	OnDestroySessionComplete.Broadcast(bWasSuccessful);

	if (PendingHostFriendsOnly.IsSet())
	{
		const bool bFriendsOnly = PendingHostFriendsOnly.GetValue();
		PendingHostFriendsOnly.Reset();
		HostSession(bFriendsOnly);
	}
	else if (PendingJoin.IsSet())
	{
		const FOnlineSessionSearchResult Result = PendingJoin.GetValue();
		PendingJoin.Reset();
		JoinSearchResult(Result);
	}
}

void UNDSessionSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		return;
	}

	FString RemoteVersion;
	InviteResult.Session.SessionSettings.Get(SettingVersionKey, RemoteVersion);
	if (RemoteVersion != UNDNetworkSettings::Get()->CompatibilityVersion)
	{
		UE_LOG(LogNDNet, Warning, TEXT("Invitacion rechazada: version %s, local %s"), *RemoteVersion, *UNDNetworkSettings::Get()->CompatibilityVersion);
		if (UNDGameInstance* GI = Cast<UNDGameInstance>(GetGameInstance()))
		{
			GI->SetLastDisconnectReason(ENDDisconnectReason::VersionMismatch);
		}
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSearchResult(InviteResult);
}
