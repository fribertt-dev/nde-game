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
#include "Loot/NDCarryComponent.h"
#include "Loot/NDExtractionVan.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Engine/LocalPlayer.h"
#include "UI/Data/NDUIDataProvider.h"
#include "UI/NDUIFunctionLibrary.h"
#include "UI/NDUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNDExpeditionDebug, Log, All);

namespace NDExpeditionDebug
{
	void MovePawn(APawn* Pawn, const FVector& Location, const FRotator& Rotation);

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

		// Dónde ve esta máquina lo que se lleva: debe ir con su portador (también en los clientes).
		for (TActorIterator<ANDLootActor> It(World); It; ++It)
		{
			const APawn* Leader = It->GetLeadCarrier();
			if (It->GetLootState() == ENDLootState::Carried && Leader)
			{
				UE_LOG(LogNDExpeditionDebug, Log, TEXT("  %s llevado por %d, a %.0f cm del primero (%s), fisica %s, pos %s"),
					It->GetDefinition() ? *It->GetDefinition()->LootId.ToString() : TEXT("?"), It->GetCarrierCount(),
					FVector::Dist(It->GetActorLocation(), Leader->GetActorLocation()),
					It->GetAttachParentActor() ? TEXT("acoplado") : TEXT("entre portadores"),
					It->Mesh->IsSimulatingPhysics() ? TEXT("si") : TEXT("no"),
					*It->GetActorLocation().ToCompactString());
				for (int32 Index = 0; Index < It->GetCarrierCount(); ++Index)
				{
					const APawn* Carrier = It->GetCarrierAt(Index);
					UE_LOG(LogNDExpeditionDebug, Log, TEXT("    portador %d: %s %s"), Index, Carrier ? *Carrier->GetName() : TEXT("-"),
						Carrier ? *Carrier->GetActorLocation().ToCompactString() : TEXT(""));
				}
			}
		}

		// Lo mismo visto desde la UI del jugador local (proveedor de datos del HUD).
		const APlayerController* LocalPC = World->GetFirstPlayerController();
		const UNDUIManagerSubsystem* UIManager = LocalPC && LocalPC->GetLocalPlayer() ? LocalPC->GetLocalPlayer()->GetSubsystem<UNDUIManagerSubsystem>() : nullptr;
		if (UNDUIDataProvider* Provider = UIManager ? UIManager->GetDataProvider() : nullptr)
		{
			const FNDNightClockVM UIClock = Provider->GetNightClock();
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("  UI: %s | fase %s | noche %s | huida %s | botin %d / %d"),
				Provider->IsSimulated() ? TEXT("simulada") : TEXT("datos reales"),
				*UEnum::GetValueAsString(UIClock.Phase),
				*UNDUIFunctionLibrary::FormatClock(UNDUIFunctionLibrary::GetNightSecondsRemaining(World, UIClock)).ToString(),
				*UNDUIFunctionLibrary::FormatClock(UNDUIFunctionLibrary::GetDawnEscapeSecondsRemaining(World, UIClock)).ToString(),
				UIClock.SecuredLootValue, UIClock.Quota);
		}
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
					const FVector Location = Van->GetActorLocation() + Van->GetActorRotation().RotateVector(Offset);
					// Mirando a la puerta de la casa (el director está en la entrada).
					const ANDExpeditionDirector* Director = ANDExpeditionDirector::Get(W);
					const FRotator Look = Director ? (Director->GetActorLocation() - Location).GetSafeNormal2D().Rotation() : Pawn->GetActorRotation();
					MovePawn(Pawn, Location, Look);
					It->Get()->ClientSetRotation(Look);
				}
			}
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("%d jugadores llevados a la camioneta"), Index);
			return false;
		});
	}

	/**
	 * Mueve un jugador desde el servidor. Al de un cliente también se le avisa: su máquina manda en su
	 * propio movimiento y, sin teclas pulsadas, no recibiría la corrección.
	 */
	void MovePawn(APawn* Pawn, const FVector& Location, const FRotator& Rotation)
	{
		Pawn->TeleportTo(Location, Rotation, false, true);
		APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
		if (PC && !PC->IsLocalController())
		{
			PC->ClientSetLocation(Pawn->GetActorLocation(), Rotation);
		}
	}

	/** Botín por LootId (L16) o por índice. */
	ANDLootActor* FindLoot(UWorld* W, const FString& Key)
	{
		TArray<ANDLootActor*> Loot;
		for (TActorIterator<ANDLootActor> It(W); It; ++It)
		{
			if (It->GetLootState() != ENDLootState::Deposited)
			{
				Loot.Add(*It);
			}
		}
		if (Loot.IsEmpty())
		{
			return nullptr;
		}
		if (!Key.IsNumeric())
		{
			ANDLootActor* const* Found = Loot.FindByPredicate([&Key](const ANDLootActor* L) { return L->GetDefinition() && L->GetDefinition()->LootId.ToString() == Key; });
			return Found ? *Found : nullptr;
		}
		return Loot[FMath::Abs(FCString::Atoi(*Key)) % Loot.Num()];
	}

	/** Jugador número Index (0 = el del servidor). */
	APawn* PawnAt(UWorld* W, int32 Index)
	{
		int32 Current = 0;
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
			{
				if (Current++ == Index)
				{
					return Pawn;
				}
			}
		}
		return nullptr;
	}

	/** Pone al jugador mirando el objeto desde el lado más despejado. */
	void PlaceNearLoot(UWorld* W, APawn* Pawn, const ANDLootActor* Target, float SideYaw = 0.f)
	{
		const FVector LootLocation = Target->GetActorLocation();
		// Más lejos cuanto más grande es el objeto.
		const float Distance = FMath::Max(170.f, Target->GetComponentsBoundingBox().GetExtent().Size2D() * 3.f);
		// El lado más despejado (la cámara va detrás del jugador y también necesita sitio).
		const FVector Start = LootLocation + FVector(0.f, 0.f, 60.f);
		FCollisionQueryParams Params(NAME_None, false, Target);
		Params.AddIgnoredActor(Pawn);
		FVector BestDir = FVector::BackwardVector;
		float BestFree = -1.f;
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const FVector Dir = FRotator(0.f, Step * 22.5f, 0.f).Vector();
			FHitResult Hit;
			const float Free = W->LineTraceSingleByChannel(Hit, Start, Start + Dir * 800.f, ECC_Visibility, Params) ? Hit.Distance : 800.f;
			if (Free > BestFree)
			{
				BestFree = Free;
				BestDir = Dir;
			}
		}
		// SideYaw separa a un segundo jugador para que no caiga encima del primero.
		BestDir = BestDir.RotateAngleAxis(SideYaw, FVector::UpVector);
		const FVector Spot = Start + BestDir * FMath::Clamp(BestFree - 220.f, 120.f, Distance);
		const FRotator Look = (LootLocation - Spot).Rotation();
		MovePawn(Pawn, Spot, FRotator(0.f, Look.Yaw, 0.f));
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(Look);
			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				PC->ClientSetRotation(Look);
			}
		}
	}

	void ToLoot(const TArray<FString>& Args, UWorld* World)
	{
		const FString Key = Args.IsValidIndex(0) ? Args[0] : TEXT("0");
		RunLater(World, ArgFloat(Args, 1, 0.f), false, [Key](UWorld* W)
		{
			APawn* Pawn = FirstPawn(W);
			const ANDLootActor* Target = FindLoot(W, Key);
			if (!Target || !Pawn || !Pawn->HasAuthority())
			{
				return false;
			}
			PlaceNearLoot(W, Pawn, Target);
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("Jugador junto a %s"), Target->GetDefinition() ? *Target->GetDefinition()->LootId.ToString() : TEXT("?"));
			return false;
		});
	}

	void Grab(const TArray<FString>& Args, UWorld* World)
	{
		const FString Key = Args.IsValidIndex(0) ? Args[0] : TEXT("0");
		const int32 PlayerIndex = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
		RunLater(World, ArgFloat(Args, 2, 0.f), false, [Key, PlayerIndex](UWorld* W)
		{
			APawn* Pawn = PawnAt(W, PlayerIndex);
			ANDLootActor* Target = FindLoot(W, Key);
			UNDCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UNDCarryComponent>() : nullptr;
			if (!Target || !Carry || !Pawn->HasAuthority())
			{
				UE_LOG(LogNDExpeditionDebug, Warning, TEXT("nde.Expedition.Grab: falta el objeto %s o el jugador %d (solo servidor)"), *Key, PlayerIndex);
				return false;
			}
			if (const APawn* Leader = Target->GetLeadCarrier())
			{
				// Ya lo lleva alguien: el segundo se pone al otro lado del bulto, mirándolo.
				const FVector Away = (Target->GetActorLocation() - Leader->GetActorLocation()).GetSafeNormal2D();
				const FVector Spot = FVector(Target->GetActorLocation().X, Target->GetActorLocation().Y, Leader->GetActorLocation().Z) + Away * 90.f;
				MovePawn(Pawn, Spot, (-Away).Rotation());
			}
			else
			{
				PlaceNearLoot(W, Pawn, Target);
			}
			// Mismo camino que la tecla E: petición al servidor.
			Carry->RequestGrab(Target);
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("Jugador %d coge %s: %s, %d portadores"), PlayerIndex,
				Target->GetDefinition() ? *Target->GetDefinition()->LootId.ToString() : TEXT("?"), *UEnum::GetValueAsString(Target->GetLootState()),
				Target->GetCarrierCount());
			return false;
		});
	}

	void Drop(const TArray<FString>& Args, UWorld* World)
	{
		const int32 PlayerIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const bool bThrow = Args.IsValidIndex(1) && FCString::Atoi(*Args[1]) != 0;
		RunLater(World, ArgFloat(Args, 2, 0.f), false, [PlayerIndex, bThrow](UWorld* W)
		{
			APawn* Pawn = PawnAt(W, PlayerIndex);
			UNDCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UNDCarryComponent>() : nullptr;
			if (!Carry || !Pawn->HasAuthority())
			{
				return false;
			}
			if (bThrow)
			{
				Carry->RequestThrow(Pawn->GetActorForwardVector() + FVector(0.f, 0.f, 0.4f), 1.f);
			}
			else
			{
				Carry->RequestDrop();
			}
			UE_LOG(LogNDExpeditionDebug, Log, TEXT("Jugador %d %s"), PlayerIndex, bThrow ? TEXT("lanza") : TEXT("suelta"));
			return false;
		});
	}

	void Walk(const TArray<FString>& Args, UWorld* World)
	{
		const int32 PlayerIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const float Forward = ArgFloat(Args, 1, 200.f);
		RunLater(World, ArgFloat(Args, 2, 0.f), false, [PlayerIndex, Forward](UWorld* W)
		{
			if (APawn* Pawn = PawnAt(W, PlayerIndex))
			{
				// Varios pasos cortos con barrido, para que choque con las paredes como andando.
				const FVector Step = Pawn->GetActorForwardVector() * (Forward / 10.f);
				for (int32 Index = 0; Index < 10; ++Index)
				{
					Pawn->SetActorLocation(Pawn->GetActorLocation() + Step, true);
				}
				MovePawn(Pawn, Pawn->GetActorLocation(), Pawn->GetActorRotation());
			}
			return false;
		});
	}

	void Meet(const TArray<FString>& Args, UWorld* World)
	{
		const float Distance = ArgFloat(Args, 0, 250.f);
		RunLater(World, ArgFloat(Args, 1, 0.f), false, [Distance](UWorld* W)
		{
			APawn* Host = PawnAt(W, 0);
			const APawn* Guest = PawnAt(W, 1);
			if (!Host || !Guest || !Host->HasAuthority())
			{
				return false;
			}
			// El 0 se pone delante del 1 y se miran: cada uno ve al otro y lo que lleva.
			const FVector Forward = Guest->GetActorForwardVector().GetSafeNormal2D();
			MovePawn(Host, Guest->GetActorLocation() + Forward * Distance, (-Forward).Rotation());
			if (AController* Controller = Host->GetController())
			{
				Controller->SetControlRotation((-Forward).Rotation());
			}
			return false;
		});
	}

	void Shot(const TArray<FString>& Args, UWorld* World)
	{
		// Ticker del motor: la espera sobrevive al viaje (un cliente que se conecta después).
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			// Por el jugador local, para que lo atienda su viewport.
			UWorld* W = FindGameWorld();
			if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
			{
				PC->ConsoleCommand(TEXT("HighResShot 1280x720"));
			}
			return false;
		}), FMath::Max(0.01f, ArgFloat(Args, 0, 0.f)));
	}

	FAutoConsoleCommandWithWorldAndArgs MeetCommand(TEXT("nde.Expedition.Meet"),
		TEXT("Solo servidor: pone al jugador 0 delante del 1, mirandose, para ver lo que lleva el otro. Uso: nde.Expedition.Meet [cm] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Meet));

	FAutoConsoleCommandWithWorldAndArgs ToLootCommand(TEXT("nde.Expedition.ToLoot"),
		TEXT("Solo servidor: pone al jugador mirando un objeto de botin. Uso: nde.Expedition.ToLoot [indice] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToLoot));

	FAutoConsoleCommandWithWorldAndArgs GrabCommand(TEXT("nde.Expedition.Grab"),
		TEXT("Solo servidor: el jugador n coge un objeto (por LootId, p. ej. L16, o por indice) como con la tecla E. Uso: nde.Expedition.Grab <objeto> [jugador] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Grab));

	FAutoConsoleCommandWithWorldAndArgs DropCommand(TEXT("nde.Expedition.Drop"),
		TEXT("Solo servidor: el jugador n suelta (0) o lanza (1) lo que lleva. Uso: nde.Expedition.Drop [jugador] [lanzar] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Drop));

	FAutoConsoleCommandWithWorldAndArgs WalkCommand(TEXT("nde.Expedition.Walk"),
		TEXT("Solo servidor: el jugador n avanza hacia delante (cm) chocando con las paredes. Uso: nde.Expedition.Walk [jugador] [cm] [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Walk));

	FAutoConsoleCommandWithWorldAndArgs ShotCommand(TEXT("nde.Expedition.Shot"),
		TEXT("Captura de pantalla 1280x720 tras una espera. Uso: nde.Expedition.Shot [segundos de espera]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Shot));

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
