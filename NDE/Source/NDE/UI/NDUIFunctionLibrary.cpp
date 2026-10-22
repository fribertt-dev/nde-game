#include "UI/NDUIFunctionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UI/NDUIManagerSubsystem.h"

double UNDUIFunctionLibrary::GetServerTimeSeconds(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return 0.0;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

float UNDUIFunctionLibrary::GetNightSecondsRemaining(const UObject* WorldContextObject, const FNDNightClockVM& Clock)
{
	if (IsDawn(Clock))
	{
		return 0.f;
	}
	if (Clock.Phase == ENDUINightPhase::Insertion)
	{
		return Clock.NightDurationSeconds;
	}
	const double Elapsed = GetServerTimeSeconds(WorldContextObject) - Clock.NightStartServerTime;
	return FMath::Max(0.f, Clock.NightDurationSeconds - static_cast<float>(Elapsed));
}

float UNDUIFunctionLibrary::GetNightProgress(const UObject* WorldContextObject, const FNDNightClockVM& Clock)
{
	if (Clock.NightDurationSeconds <= 0.f)
	{
		return 1.f;
	}
	return 1.f - GetNightSecondsRemaining(WorldContextObject, Clock) / Clock.NightDurationSeconds;
}

float UNDUIFunctionLibrary::GetDawnEscapeSecondsRemaining(const UObject* WorldContextObject, const FNDNightClockVM& Clock)
{
	if (Clock.Phase == ENDUINightPhase::Settlement)
	{
		return 0.f;
	}
	if (!IsDawn(Clock))
	{
		return Clock.DawnEscapeSeconds;
	}
	const double Elapsed = GetServerTimeSeconds(WorldContextObject) - Clock.DawnTriggeredServerTime;
	return FMath::Max(0.f, Clock.DawnEscapeSeconds - static_cast<float>(Elapsed));
}

bool UNDUIFunctionLibrary::IsDawn(const FNDNightClockVM& Clock)
{
	return Clock.Phase >= ENDUINightPhase::DawnTriggered;
}

FText UNDUIFunctionLibrary::FormatClock(float Seconds)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), TotalSeconds / 60, TotalSeconds % 60));
}

// Textos localizables: los DisplayName de UMETA no existen en builds empaquetadas.
FText UNDUIFunctionLibrary::GetSpeciesDisplayName(ENDDesveladoSpecies Species)
{
	switch (Species)
	{
	case ENDDesveladoSpecies::Raccoon:		return NSLOCTEXT("NDUI", "SpeciesRaccoon", "Mapache");
	case ENDDesveladoSpecies::Lemur:		return NSLOCTEXT("NDUI", "SpeciesLemur", "Lémur");
	case ENDDesveladoSpecies::Bat:	return NSLOCTEXT("NDUI", "SpeciesBat", "Murciélago");
	case ENDDesveladoSpecies::Fox:		return NSLOCTEXT("NDUI", "SpeciesFox", "Zorro pequeño");
	case ENDDesveladoSpecies::Ferret:		return NSLOCTEXT("NDUI", "SpeciesFerret", "Hurón");
	case ENDDesveladoSpecies::Owl:			return NSLOCTEXT("NDUI", "SpeciesOwl", "Búho");
	default:								return NSLOCTEXT("NDUI", "SpeciesNone", "Sin especie");
	}
}

UNDUIManagerSubsystem* UNDUIFunctionLibrary::GetUIManager(const UObject* WorldContextObject)
{
	// Desde un widget se usa su jugador; si no, el primer jugador local del mundo.
	const ULocalPlayer* LocalPlayer = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		LocalPlayer = Widget->GetOwningLocalPlayer();
	}
	if (!LocalPlayer)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	}
	return LocalPlayer ? LocalPlayer->GetSubsystem<UNDUIManagerSubsystem>() : nullptr;
}

UNDUIDataProvider* UNDUIFunctionLibrary::GetUIDataProvider(const UObject* WorldContextObject)
{
	const UNDUIManagerSubsystem* Manager = GetUIManager(WorldContextObject);
	return Manager ? Manager->GetDataProvider() : nullptr;
}
