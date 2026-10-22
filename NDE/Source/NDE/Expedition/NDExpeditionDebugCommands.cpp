// Comandos de consola para probar el bucle de expedición sin jugarlo entero.
// Todos aceptan una espera final en segundos, para lanzarlos desde -ExecCmds.

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Contracts/NDContractDefinition.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Expedition/NDNightClockComponent.h"
#include "Loot/NDExtractionVan.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDExpeditionDebug, Log, All);

namespace NDExpeditionDebug
{
	float ArgFloat(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** Ejecuta Action ahora o tras Delay segundos (y cada Delay si bRepeat). */
	void RunLater(UWorld* World, float Delay, bool bRepeat, TFunction<bool(UWorld*)> Action)
	{
		if (!World)
		{
			return;
		}
		if (Delay <= 0.f)
		{
			Action(World);
			return;
		}
		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakWorld, Handle, Action, bRepeat]()
		{
			if (UWorld* W = WeakWorld.Get())
			{
				// Action devuelve false para dejar de repetirse.
				if (!Action(W) || !bRepeat)
				{
					W->GetTimerManager().ClearTimer(*Handle);
				}
			}
		}), Delay, bRepeat);
	}

	bool LogStatus(UWorld* World)
	{
		const ANDExpeditionDirector* Director = ANDExpeditionDirector::Get(World);
		if (!Director || !Director->Expedition)
		{
			UE_LOG(LogNDExpeditionDebug, Warning, TEXT("Sin director de expedicion"));
			return false;
		}
		const UNDExpeditionComponent* Expedition = Director->Expedition;
		const UNDNightClockComponent* Clock = Director->NightClock;
		int32 LooseLoot = 0;
		for (TActorIterator<ANDLootActor> It(World); It; ++It)
		{
			LooseLoot += It->GetLootState() != ENDLootState::Deposited ? 1 : 0;
		}
		const FNDRunSummary& Summary = Expedition->GetRunSummary();
		UE_LOG(LogNDExpeditionDebug, Log, TEXT("[%s] contrato %s | fase %s | noche %s, %.0f s al amanecer | botin %d / %d, objetivo %s | sin depositar %d | resultado %s"),
			World->GetNetMode() == NM_Client ? TEXT("cliente") : TEXT("servidor"),
			Expedition->Contract ? *Expedition->Contract->ContractId.ToString() : TEXT("-"),
			*UEnum::GetValueAsString(Expedition->GetPhase()),
			Clock ? *UEnum::GetValueAsString(Clock->GetPhase()) : TEXT("-"),
			Clock ? Clock->GetSecondsUntilDawn() : 0.f,
			Expedition->GetLootPoints(), Expedition->GetQuota(),
			Expedition->IsObjectiveMet() ? TEXT("cumplido") : TEXT("pendiente"),
			LooseLoot,
			*UEnum::GetValueAsString(Summary.Outcome));
		return Summary.Outcome == ENDExpeditionOutcome::None;
	}

	ANDExtractionVan* FindVan(UWorld* World)
	{
		TActorIterator<ANDExtractionVan> It(World);
		return It ? *It : nullptr;
	}

	APawn* FirstPawn(UWorld* World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
			{
				return Pawn;
			}
		}
		return nullptr;
	}

	/** Mundo de juego actual. Cambia cuando un cliente se conecta o se viaja de mapa. */
	UWorld* FindGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	void Status(const TArray<FString>& Args, UWorld* World)
	{
		const float Interval = ArgFloat(Args, 0, 0.f);
		if (Interval <= 0.f)
		{
			LogStatus(World);
			return;
		}
		// Con el ticker del motor sigue funcionando tras viajar (p. ej. un cliente que entra después).
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			UWorld* GameWorld = FindGameWorld();
			return !GameWorld || !ANDExpeditionDirector::Get(GameWorld) || LogStatus(GameWorld);
		}), Interval);
	}

	void DepositLoot(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		RunLater(World, ArgFloat(Args, 1, 0.f), false, [Count](UWorld* W)
		{
			ANDExtractionVan* Van = FindVan(W);
			if (!Van || !Van->HasAuthority())
			{
				UE_LOG(LogNDExpeditionDebug, Warning, TEXT("nde.Expedition.DepositLoot: hace falta la camioneta y ser el servidor"));
				return false;
			}
			int32 Deposited = 0;
			for (TActorIterator<ANDLootActor> It(W); It && (Count <= 0 || Deposited < Count); ++It)
			{
				if (It->GetLootState() == ENDLootState::WorldLoose && Van->TryDeposit(*It, FirstPawn(W)))
				{
					UE_LOG(LogNDExpeditionDebug, Log, TEXT("Depositado %s (%s)"), It->GetDefinition() ? *It->GetDefinition()->LootId.ToString() : TEXT("?"),
						*It->GetLootGuid().ToString());
					++Deposited;
				}
			}
			LogStatus(W);
			return false;
		});
	}

	void SkipTime(const TArray<FString>& Args, UWorld* World)
	{
		const float Seconds = ArgFloat(Args, 0, 60.f);
		RunLater(World, ArgFloat(Args, 1, 0.f), false, [Seconds](UWorld* W)
		{
			if (UNDNightClockComponent* Clock = ANDExpeditionDirector::GetNightClock(W))
			{
				Clock->DebugSkipSeconds(Seconds);
				UE_LOG(LogNDExpeditionDebug, Log, TEXT("Reloj adelantado %.0f s"), Seconds);
			}
			return false;
		});
	}

	void ToVan(const TArray<FString>& Args, UWorld* World)
	{
		RunLater(World, ArgFloat(Args, 0, 0.f), false, [](UWorld* W)
		{
			ANDExtractionVan* Van = FindVan(W);
			if (!Van || !Van->HasAuthority())
			{
				return false;
			}
			int32 Index = 0;
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
				{
					const FVector Offset(0.f, (Index++ - 1) * 80.f, 100.f);
					Pawn->TeleportTo(Van->GetActorLocation() + Van->GetActorRotation().RotateVector(Offset), Pawn->GetActorRotation());
				}
			}
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("%d jugadores llevados a la camioneta"), Index);
			return false;
		});
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("nde.Expedition.Status"),
		TEXT("Escribe en el log el estado de la expedicion. Uso: nde.Expedition.Status [cada cuantos segundos, 0 = una vez]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));

	FAutoConsoleCommandWithWorldAndArgs DepositCommand(TEXT("nde.Expedition.DepositLoot"),
		TEXT("Solo servidor: deposita en la camioneta el botin suelto. Uso: nde.Expedition.DepositLoot [cuantos, 0 = todo] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DepositLoot));

	FAutoConsoleCommandWithWorldAndArgs SkipCommand(TEXT("nde.Expedition.SkipTime"),
		TEXT("Solo servidor: adelanta el reloj de la noche. Uso: nde.Expedition.SkipTime <segundos> [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkipTime));

	FAutoConsoleCommandWithWorldAndArgs ToVanCommand(TEXT("nde.Expedition.ToVan"),
		TEXT("Solo servidor: lleva a todos los jugadores a la camioneta. Uso: nde.Expedition.ToVan [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToVan));
}
