// No Despiertes Eso

#include "Progression/NDProfileSubsystem.h"

#include "Core/NDLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/NDProfileSaveGame.h"

void UNDProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString SlotName = GetSlotName();
	Profile = Cast<UNDProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Profile)
	{
		Profile = Cast<UNDProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UNDProfileSaveGame::StaticClass()));
		UE_LOG(LogNDNet, Log, TEXT("Perfil nuevo (%s)"), *SlotName);
	}
	else
	{
		// TODO: migrar cuando SchemaVersion < CurrentSchemaVersion.
		UE_LOG(LogNDNet, Log, TEXT("Perfil cargado (%s): %lld monedas, %d reputacion, %d expediciones"),
			*SlotName, Profile->Coins, Profile->Reputation, Profile->ExpeditionsCompleted);
	}
}

FString UNDProfileSubsystem::GetSlotName() const
{
	// Cada ventana de PIE tiene su propio perfil, para que dos jugadores de prueba no compartan archivo.
	const FWorldContext* Context = GetGameInstance() ? GetGameInstance()->GetWorldContext() : nullptr;
	return (Context && Context->PIEInstance != INDEX_NONE)
		? FString::Printf(TEXT("NDE_Perfil_PIE%d"), Context->PIEInstance)
		: TEXT("NDE_Perfil");
}

int64 UNDProfileSubsystem::GetCoins() const
{
	return Profile ? Profile->Coins : 0;
}

int32 UNDProfileSubsystem::GetReputation() const
{
	return Profile ? Profile->Reputation : 0;
}

int32 UNDProfileSubsystem::GetExpeditionsCompleted() const
{
	return Profile ? Profile->ExpeditionsCompleted : 0;
}

bool UNDProfileSubsystem::IsUnlocked(FName UnlockId) const
{
	return Profile && Profile->UnlockedIds.Contains(UnlockId);
}

bool UNDProfileSubsystem::ApplyRunReward(const FGuid& RunGuid, int32 Coins, int32 Reputation)
{
	if (!Profile || !RunGuid.IsValid() || Profile->SettledRunGuids.Contains(RunGuid))
	{
		return false;
	}

	Profile->Coins = FMath::Max<int64>(0, Profile->Coins + Coins);
	Profile->Reputation = FMath::Max(0, Profile->Reputation + Reputation);
	++Profile->ExpeditionsCompleted;
	Profile->SettledRunGuids.Add(RunGuid);
	if (Profile->SettledRunGuids.Num() > MaxRememberedRuns)
	{
		Profile->SettledRunGuids.RemoveAt(0, Profile->SettledRunGuids.Num() - MaxRememberedRuns);
	}

	UE_LOG(LogNDNet, Log, TEXT("Expedicion %s cobrada: +%d monedas, +%d reputacion (total %lld)"),
		*RunGuid.ToString(), Coins, Reputation, Profile->Coins);
	Save();
	return true;
}

bool UNDProfileSubsystem::TrySpendCoins(int32 Amount)
{
	if (!Profile || Amount < 0 || Profile->Coins < Amount)
	{
		return false;
	}
	Profile->Coins -= Amount;
	Save();
	return true;
}

void UNDProfileSubsystem::Unlock(FName UnlockId)
{
	if (Profile && !UnlockId.IsNone() && !Profile->UnlockedIds.Contains(UnlockId))
	{
		Profile->UnlockedIds.Add(UnlockId);
		Save();
	}
}

void UNDProfileSubsystem::Save()
{
	if (Profile && !UGameplayStatics::SaveGameToSlot(Profile, GetSlotName(), 0))
	{
		UE_LOG(LogNDNet, Error, TEXT("No se pudo guardar el perfil (%s)"), *GetSlotName());
	}
	OnProfileChanged.Broadcast();
}
