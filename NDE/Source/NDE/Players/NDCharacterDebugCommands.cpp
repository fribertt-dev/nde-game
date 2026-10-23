// No Despiertes Eso
// Comandos de consola para probar el personaje sin teclado (capturas y pruebas en el PC).

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Players/NDCharacter.h"
#include "Players/NDHidingComponent.h"
#include "Players/NDInteractionComponent.h"
#include "Players/NDNoiseEmitterComponent.h"

namespace NDCharacterDebug
{
	/** Desvelados del jugador local en este mundo. */
	TArray<ANDCharacter*> LocalCharacters(UWorld* World)
	{
		TArray<ANDCharacter*> Result;
		for (TActorIterator<ANDCharacter> It(World); It; ++It)
		{
			if (It->IsLocallyControlled())
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	void Say(const FString& Text)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, Text);
		}
	}

	FAutoConsoleCommandWithWorld CmdUse(
		TEXT("nde.Interaction.Use"),
		TEXT("Hace lo mismo que pulsar E con tu Desvelado (coger, esconderse, salir, camioneta)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (ANDCharacter* Character : LocalCharacters(World))
			{
				Say(FString::Printf(TEXT("nde.Interaction.Use: %s"), *Character->Interaction->GetPromptText().ToString()));
				Character->Interaction->Interact();
			}
		}));

	FAutoConsoleCommandWithWorld CmdGoToHide(
		TEXT("nde.Hide.GoToNearest"),
		TEXT("Lleva a tu Desvelado a la entrada del escondite libre más cercano (solo servidor o en solitario). Luego nde.Interaction.Use para entrar."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (ANDCharacter* Character : LocalCharacters(World))
			{
				if (!Character->HasAuthority())
				{
					Say(TEXT("nde.Hide.GoToNearest solo funciona en el servidor."));
					continue;
				}
				const TArray<FNDHidingSpotInfo> Spots = UNDHidingComponent::GetAllSpots(World);
				const FNDHidingSpotInfo* Best = nullptr;
				for (const FNDHidingSpotInfo& Spot : Spots)
				{
					if (!Spot.IsFull() && (!Best || FVector::Dist(Spot.EntryLocation, Character->GetActorLocation()) < FVector::Dist(Best->EntryLocation, Character->GetActorLocation())))
					{
						Best = &Spot;
					}
				}
				if (!Best)
				{
					Say(TEXT("No hay escondites libres (¿hay una casa generada?)."));
					continue;
				}
				// Mirando al mueble, con los pies a la altura de la entrada.
				const FVector Location = Best->EntryLocation + FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
				const FRotator Facing(0.f, (Best->FurnitureLocation - Best->EntryLocation).Rotation().Yaw, 0.f);
				Character->TeleportTo(Location, Facing);
				if (Character->GetController())
				{
					Character->GetController()->SetControlRotation(Facing);
				}
				Say(FString::Printf(TEXT("Junto al escondite %d (%s, %d/%d). Usa nde.Interaction.Use."), Best->SpotId,
					*UEnum::GetValueAsString(Best->Kind), Best->Occupants, Best->Capacity));
			}
		}));

	FAutoConsoleCommandWithWorld CmdHideState(
		TEXT("nde.Hide.State"),
		TEXT("Escribe si tu Desvelado está escondido, dónde y cuánto se le ve."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			for (ANDCharacter* Character : LocalCharacters(World))
			{
				const UNDHidingComponent* Hiding = Character->Hiding;
				Say(FString::Printf(TEXT("Escondido: %s, escondite %d, visibilidad %.2f, escondites en la casa: %d"),
					Hiding->IsHidden() ? TEXT("sí") : TEXT("no"), Hiding->GetHiddenSpotId(), Hiding->GetVisibilityFactor(),
					UNDHidingComponent::GetAllSpots(World).Num()));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutoWalk(
		TEXT("nde.Character.AutoWalk"),
		TEXT("Anda hacia donde miras durante N segundos (por defecto 3) con el movimiento real: genera pasos y ruido."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 3.f;
			for (ANDCharacter* Character : LocalCharacters(World))
			{
				Character->StartAutoWalk(Seconds);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNoise(
		TEXT("nde.Noise.Emit"),
		TEXT("Hace un ruido en los pies de tu Desvelado: fuerza de 0 a 1 (por defecto 0.1, un paso). Solo servidor o en solitario."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Loudness = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.1f;
			for (ANDCharacter* Character : LocalCharacters(World))
			{
				const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				Character->NoiseEmitter->EmitNoise(Feet, Loudness, UNDNoiseEmitterComponent::FootstepTag);
				Say(FString::Printf(TEXT("Ruido %.2f emitido."), Loudness));
			}
		}));
}
