// No Despiertes Eso

#include "Players/NDInteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Expedition/NDExpeditionPlayerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Players/NDHidingComponent.h"
#include "Players/NDInteractable.h"
#include "UI/NDUIManagerSubsystem.h"

namespace NDInteraction
{
	TAutoConsoleVariable<int32> CVarDrawPrompt(
		TEXT("nde.Interaction.DrawPrompt"),
		-1,
		TEXT("Texto de la tecla E en pantalla sin la UI: 1 = siempre, 0 = nunca, -1 = automático (solo si no hay raíz de UI)."));

	FText WithKey(const FText& Action)
	{
		return FText::Format(NSLOCTEXT("NDInteraction", "WithKey", "E  {0}"), Action);
	}

	FText LootName(const ANDLootActor* Loot)
	{
		const UNDLootDefinition* Definition = Loot ? Loot->GetDefinition() : nullptr;
		if (Definition && !Definition->DisplayName.IsEmpty())
		{
			return Definition->DisplayName;
		}
		return NSLOCTEXT("NDInteraction", "LootFallback", "botín");
	}
}

UNDInteractionComponent::UNDInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UNDInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	UpdateFocus();
	DrawFallbackPrompt();
}

void UNDInteractionComponent::UpdateFocus()
{
	const AActor* Owner = GetOwner();
	const UNDHidingComponent* Hiding = Owner->FindComponentByClass<UNDHidingComponent>();
	const UNDCarryComponent* Carry = Owner->FindComponentByClass<UNDCarryComponent>();
	const UNDExpeditionPlayerComponent* Expedition = Owner->FindComponentByClass<UNDExpeditionPlayerComponent>();

	ENDInteractionKind NewKind = ENDInteractionKind::None;
	FText NewText;
	FocusedLoot.Reset();
	FocusedActor.Reset();

	if (Hiding && Hiding->IsHidden())
	{
		NewKind = ENDInteractionKind::ExitHiding;
		NewText = NDInteraction::WithKey(NSLOCTEXT("NDInteraction", "ExitHiding", "Salir del escondite"));
	}
	else if (Expedition && Expedition->IsInsideVan())
	{
		NewKind = ENDInteractionKind::Van;
		NewText = NDInteraction::WithKey(NSLOCTEXT("NDInteraction", "Van", "Camioneta: votar la huida"));
	}
	else if (ANDLootActor* Loot = Carry ? Carry->FindBestLootTarget() : nullptr)
	{
		NewKind = ENDInteractionKind::Loot;
		FocusedLoot = Loot;
		const UNDLootDefinition* Definition = Loot->GetDefinition();
		const bool bOthersCarry = Loot->GetLootState() == ENDLootState::Carried;
		if (bOthersCarry)
		{
			NewText = NDInteraction::WithKey(FText::Format(NSLOCTEXT("NDInteraction", "HelpCarry", "Ayudar a cargar {0}"), NDInteraction::LootName(Loot)));
		}
		else if (Definition && Definition->IsPocketSized())
		{
			NewText = NDInteraction::WithKey(FText::Format(NSLOCTEXT("NDInteraction", "Pocket", "Guardar {0}"), NDInteraction::LootName(Loot)));
		}
		else if (Carry->GetHeldLoot())
		{
			NewKind = ENDInteractionKind::None;
			NewText = NSLOCTEXT("NDInteraction", "HandsFull", "Tienes las manos ocupadas (G para soltar)");
		}
		else
		{
			NewText = NDInteraction::WithKey(FText::Format(NSLOCTEXT("NDInteraction", "Grab", "Coger {0}"), NDInteraction::LootName(Loot)));
		}
	}
	else if (const FNDHidingSpotInfo Spot = Hiding ? Hiding->FindBestSpot() : FNDHidingSpotInfo(); Spot.IsValid())
	{
		if (Spot.IsFull())
		{
			NewText = NSLOCTEXT("NDInteraction", "HideFull", "Escondite lleno");
		}
		else if (Hiding->HandsTooFull())
		{
			NewText = NSLOCTEXT("NDInteraction", "HideHands", "Suelta lo que llevas para esconderte (G)");
		}
		else
		{
			NewKind = ENDInteractionKind::HidingSpot;
			FText Action;
			switch (Spot.Kind)
			{
			case ENDHidingSpotKind::UnderFurniture: Action = NSLOCTEXT("NDInteraction", "HideUnder", "Esconderse debajo"); break;
			case ENDHidingSpotKind::InsideCabinet:  Action = NSLOCTEXT("NDInteraction", "HideInside", "Esconderse dentro"); break;
			default:                                Action = NSLOCTEXT("NDInteraction", "HideBehind", "Esconderse detrás"); break;
			}
			if (Spot.Capacity > 1)
			{
				Action = FText::Format(NSLOCTEXT("NDInteraction", "HideRoom", "{0} (caben {1})"), Action, FText::AsNumber(Spot.Capacity - Spot.Occupants));
			}
			NewText = NDInteraction::WithKey(Action);
		}
	}
	else
	{
		FText ActorText;
		if (AActor* Target = TraceInteractable(ActorText))
		{
			NewKind = ENDInteractionKind::Interactable;
			FocusedActor = Target;
			NewText = NDInteraction::WithKey(ActorText);
		}
	}

	if (NewKind != Kind || !NewText.EqualTo(PromptText))
	{
		Kind = NewKind;
		PromptText = NewText;
		OnFocusChanged.Broadcast(Kind, PromptText);
	}
}

AActor* UNDInteractionComponent::TraceInteractable(FText& OutText) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return nullptr;
	}
	const FVector Start = Camera->GetComponentLocation();
	// Desde los ojos: el alcance cuenta desde el cuerpo, así que se suma lo que la cámara está por detrás.
	const float Behind = FMath::Max(0.f, FVector::Dist(Start, Pawn->GetActorLocation()));
	const FVector End = Start + Camera->GetForwardVector() * (InteractableRange + Behind);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NDInteractionTrace), false, Pawn);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.f), Params))
	{
		return nullptr;
	}
	AActor* Target = Hit.GetActor();
	if (!Target || !Target->Implements<UNDInteractable>())
	{
		return nullptr;
	}
	OutText = INDInteractable::Execute_GetInteractionText(Target, const_cast<APawn*>(Pawn));
	return OutText.IsEmpty() ? nullptr : Target;
}

void UNDInteractionComponent::Interact()
{
	AActor* Owner = GetOwner();
	UpdateFocus();
	switch (Kind)
	{
	case ENDInteractionKind::ExitHiding:
	case ENDInteractionKind::HidingSpot:
		if (UNDHidingComponent* Hiding = Owner->FindComponentByClass<UNDHidingComponent>())
		{
			Hiding->RequestToggleHide();
		}
		break;
	case ENDInteractionKind::Van:
		if (UNDExpeditionPlayerComponent* Expedition = Owner->FindComponentByClass<UNDExpeditionPlayerComponent>())
		{
			Expedition->RequestVanInteraction();
		}
		break;
	case ENDInteractionKind::Loot:
		if (UNDCarryComponent* Carry = Owner->FindComponentByClass<UNDCarryComponent>(); Carry && FocusedLoot.IsValid())
		{
			Carry->RequestGrab(FocusedLoot.Get());
		}
		break;
	case ENDInteractionKind::Interactable:
		if (FocusedActor.IsValid())
		{
			ServerInteractWith(FocusedActor.Get());
		}
		break;
	default:
		break;
	}
}

void UNDInteractionComponent::ServerInteractWith_Implementation(AActor* Target)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Target || !Target->Implements<UNDInteractable>())
	{
		return;
	}
	// El servidor comprueba la distancia con margen por la latencia.
	const FVector Closest = Target->GetActorLocation();
	const float Radius = Target->GetComponentsBoundingBox().GetExtent().Size2D();
	if (FVector::Dist(Pawn->GetActorLocation(), Closest) > InteractableRange + Radius + 80.f)
	{
		return;
	}
	if (INDInteractable::Execute_GetInteractionText(Target, Pawn).IsEmpty())
	{
		return;
	}
	INDInteractable::Execute_Interact(Target, Pawn);
}

void UNDInteractionComponent::DrawFallbackPrompt() const
{
	if (!GEngine || PromptText.IsEmpty())
	{
		return;
	}
	const int32 Mode = NDInteraction::CVarDrawPrompt.GetValueOnGameThread();
	if (Mode == 0)
	{
		return;
	}
	if (Mode < 0)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const UNDUIManagerSubsystem* UIManager = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UNDUIManagerSubsystem>() : nullptr;
		if (UIManager && UIManager->GetRootLayout())
		{
			return;
		}
	}
	GEngine->AddOnScreenDebugMessage(9200, 0.15f, Kind == ENDInteractionKind::None ? FColor(200, 200, 200) : FColor::White, PromptText.ToString(), false, FVector2D(1.4f, 1.4f));
}
