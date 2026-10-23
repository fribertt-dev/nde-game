// Comandos de consola para probar la criatura sin jugar una partida entera.
// Actúan en el servidor (en PIE, aunque se escriban en la ventana de un cliente).
// Como los de la expedición, aceptan una espera final en segundos para lanzarlos desde -ExecCmds.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HouseGen/NDHouseLot.h"
#include "Monsters/NDMonsterAIController.h"
#include "Monsters/NDMonsterBrain.h"
#include "Monsters/NDMonsterCage.h"
#include "Monsters/NDMonsterCharacter.h"
#include "Monsters/NDMonsterSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDMonsterDebug, Log, All);

namespace NDMonsterDebug
{
	/** El mundo del servidor (en PIE, el de la ventana que hace de servidor). */
	UWorld* GetServerWorld(UWorld* World)
	{
		if (World && World->GetNetMode() != NM_Client)
		{
			return World;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Candidate->GetNetMode() != NM_Client && (Candidate->WorldType == EWorldType::PIE || Candidate->WorldType == EWorldType::Game))
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	/** Ejecuta Action en el servidor ahora o tras Delay segundos. */
	void Run(UWorld* World, const TArray<FString>& Args, int32 DelayArg, TFunction<void(UWorld*)> Action)
	{
		UWorld* Server = GetServerWorld(World);
		if (!Server)
		{
			UE_LOG(LogNDMonsterDebug, Warning, TEXT("No hay mundo de servidor."));
			return;
		}
		const float Delay = Args.IsValidIndex(DelayArg) ? FCString::Atof(*Args[DelayArg]) : 0.f;
		if (Delay <= 0.f)
		{
			Action(Server);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> WeakServer(Server);
		Server->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakServer, Action]()
		{
			if (UWorld* W = WeakServer.Get())
			{
				Action(W);
			}
		}), Delay, false);
	}

	void ForEachBrain(UWorld* World, TFunctionRef<void(ANDMonsterAIController&)> Action)
	{
		UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(World);
		if (!Monsters)
		{
			return;
		}
		const TArray<ANDMonsterCharacter*> All = Monsters->GetMonsters();
		if (All.IsEmpty())
		{
			UE_LOG(LogNDMonsterDebug, Warning, TEXT("No hay ninguna criatura en el mapa."));
		}
		for (ANDMonsterCharacter* Monster : All)
		{
			if (ANDMonsterAIController* Brain = Cast<ANDMonsterAIController>(Monster->GetController()))
			{
				Action(*Brain);
			}
		}
	}

	APawn* GetPlayerPawn(UWorld* World, int32 Index)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || !GameState->PlayerArray.IsValidIndex(Index))
		{
			return nullptr;
		}
		return GameState->PlayerArray[Index] ? GameState->PlayerArray[Index]->GetPawn() : nullptr;
	}

	void Status(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server)
		{
			ForEachBrain(Server, [](ANDMonsterAIController& Brain) { UE_LOG(LogNDMonsterDebug, Display, TEXT("%s"), *Brain.GetDebugStatus()); });
			if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(Server))
			{
				TArray<FVector> Route;
				Monsters->GetPatrolRoute(Route);
				UE_LOG(LogNDMonsterDebug, Display, TEXT("Patrulla: %d puntos."), Route.Num());
			}
		});
	}

	void Wake(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server) { ForEachBrain(Server, [](ANDMonsterAIController& Brain) { Brain.DebugWake(); }); });
	}

	void Sleep(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server) { ForEachBrain(Server, [](ANDMonsterAIController& Brain) { Brain.DebugSleep(); }); });
	}

	void Suspicion(const TArray<FString>& Args, UWorld* World)
	{
		const float Value = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 50.f;
		Run(World, Args, 1, [Value](UWorld* Server) { ForEachBrain(Server, [Value](ANDMonsterAIController& Brain) { Brain.DebugSetSuspicion(Value); }); });
	}

	void NightWalk(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server) { ForEachBrain(Server, [](ANDMonsterAIController& Brain) { Brain.DebugNightWalk(); }); });
	}

	void Noise(const TArray<FString>& Args, UWorld* World)
	{
		const float Loudness = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 30.f;
		const int32 Player = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
		Run(World, Args, 2, [Loudness, Player](UWorld* Server)
		{
			APawn* Pawn = GetPlayerPawn(Server, Player);
			if (!Pawn)
			{
				UE_LOG(LogNDMonsterDebug, Warning, TEXT("No hay jugador %d."), Player);
				return;
			}
			ForEachBrain(Server, [Pawn, Loudness](ANDMonsterAIController& Brain)
			{
				Brain.HearNoise(Pawn->GetActorLocation(), Loudness, Pawn, TEXT("NDE.Noise.Debug"));
			});
			UE_LOG(LogNDMonsterDebug, Display, TEXT("Ruido %.0f en %s."), Loudness, *Pawn->GetActorLocation().ToString());
		});
	}

	void ToBed(const TArray<FString>& Args, UWorld* World)
	{
		// Pone al jugador junto a la cama de la criatura, mirándola.
		const int32 Player = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		Run(World, Args, 1, [Player](UWorld* Server)
		{
			APawn* Pawn = GetPlayerPawn(Server, Player);
			UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(Server);
			const TArray<ANDMonsterCharacter*> All = Monsters ? Monsters->GetMonsters() : TArray<ANDMonsterCharacter*>();
			if (!Pawn || All.IsEmpty())
			{
				return;
			}
			const FTransform Bed = All[0]->GetBedTransform();
			const FVector Spot = Bed.GetLocation() + Bed.GetRotation().GetForwardVector() * 450.f + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + 10.f);
			Pawn->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation((Bed.GetLocation() - Spot).Rotation());
			}
		});
	}

	void Respawn(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server)
		{
			UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(Server);
			ANDHouseLot* Lot = ANDHouseLot::FindLot(Server);
			if (Monsters && Lot && Lot->IsHouseBuilt())
			{
				Monsters->SpawnForHouse(Lot);
			}
			else
			{
				UE_LOG(LogNDMonsterDebug, Warning, TEXT("Sin casa generada: coloca la criatura a mano (ANDMonsterCharacter)."));
			}
		});
	}

	void Release(const TArray<FString>& Args, UWorld* World)
	{
		Run(World, Args, 0, [](UWorld* Server)
		{
			if (UNDMonsterSubsystem* Monsters = UNDMonsterSubsystem::Get(Server))
			{
				if (ANDMonsterCage* Cage = Monsters->GetOrSpawnCage(FVector::ZeroVector))
				{
					Cage->ReleaseAll(nullptr);
				}
			}
		});
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("nde.Monster.Status"),
		TEXT("[espera] Escribe en el log el estado de cada criatura: estado, sospecha, vista, ronquido y objetivo."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));
	FAutoConsoleCommandWithWorldAndArgs WakeCommand(TEXT("nde.Monster.Wake"),
		TEXT("[espera] Servidor: despierta a la criatura (se levanta e investiga delante de ella)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Wake));
	FAutoConsoleCommandWithWorldAndArgs SleepCommand(TEXT("nde.Monster.Sleep"),
		TEXT("[espera] Servidor: la manda a su cama en sueño profundo."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Sleep));
	FAutoConsoleCommandWithWorldAndArgs SuspicionCommand(TEXT("nde.Monster.Suspicion"),
		TEXT("<0-100> [espera] Servidor: fija la sospecha (25 ligero, 40 se revuelve, 60 medio despierto, 80 se levanta)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Suspicion));
	FAutoConsoleCommandWithWorldAndArgs NightWalkCommand(TEXT("nde.Monster.NightWalk"),
		TEXT("[espera] Servidor: se levanta ya a hacer su ronda por la casa."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NightWalk));
	FAutoConsoleCommandWithWorldAndArgs NoiseCommand(TEXT("nde.Monster.Noise"),
		TEXT("<0-100> [jugador] [espera] Servidor: ruido en la posición del jugador (andar 10, correr 22, vidrio 58)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Noise));
	FAutoConsoleCommandWithWorldAndArgs ToBedCommand(TEXT("nde.Monster.ToBed"),
		TEXT("[jugador] [espera] Servidor: pone al jugador a 4,5 m de la cama de la criatura, mirándola."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToBed));
	FAutoConsoleCommandWithWorldAndArgs RespawnCommand(TEXT("nde.Monster.Respawn"),
		TEXT("[espera] Servidor: vuelve a crear la criatura y la jaula en la casa generada."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Respawn));
	FAutoConsoleCommandWithWorldAndArgs ReleaseCommand(TEXT("nde.Monster.Release"),
		TEXT("[espera] Servidor: abre la jaula y libera a todos."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Release));
}
