#include "Tools/NDLoadoutComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Tools/NDToolDefinition.h"

namespace
{
	FNDToolSlot MakeSlot(UNDToolDefinition* Tool, int32 ChargesOverride = 0)
	{
		FNDToolSlot Slot;
		Slot.Tool = Tool;
		Slot.Charges = (ChargesOverride > 0) ? ChargesOverride : (Tool ? Tool->Charges : 0);
		return Slot;
	}
}

UNDLoadoutComponent::UNDLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNDLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNDLoadoutComponent, ActiveSlots);
	DOREPLIFETIME(UNDLoadoutComponent, ConsumableSlot);
	DOREPLIFETIME(UNDLoadoutComponent, SelectedSlot);
	DOREPLIFETIME(UNDLoadoutComponent, bFlashlightOn);
}

void UNDLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority() && ActiveSlots.Num() == 0)
	{
		SetLoadout(
			DefaultActiveTools.IsValidIndex(0) ? DefaultActiveTools[0].Get() : nullptr,
			DefaultActiveTools.IsValidIndex(1) ? DefaultActiveTools[1].Get() : nullptr,
			DefaultConsumable,
			DefaultConsumableCount);
	}
}

void UNDLoadoutComponent::SetLoadout(UNDToolDefinition* ToolA, UNDToolDefinition* ToolB, UNDToolDefinition* Consumable, int32 ConsumableCount)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	ActiveSlots.Reset();
	ActiveSlots.Add(MakeSlot(ToolA));
	ActiveSlots.Add(MakeSlot(ToolB));
	ConsumableSlot = MakeSlot(Consumable, ConsumableCount);
	PingSlot = MakeSlot(PingTool);
	PingSlot.Charges = -1;
	SelectedSlot = 0;
	bFlashlightOn = false;
	OnLoadoutChanged.Broadcast();
}

double UNDLoadoutComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World ? World->GetTimeSeconds() : 0.0;
}

UNDToolDefinition* UNDLoadoutComponent::GetSelectedTool() const
{
	return ActiveSlots.IsValidIndex(SelectedSlot) ? ActiveSlots[SelectedSlot].Tool.Get() : nullptr;
}

bool UNDLoadoutComponent::HasCart() const
{
	for (const FNDToolSlot& Slot : ActiveSlots)
	{
		if (Slot.Tool && Slot.Tool->Effect == ENDToolEffect::Cart)
		{
			return true;
		}
	}
	return false;
}

void UNDLoadoutComponent::RequestSelectSlot(int32 SlotIndex)
{
	ServerSelectSlot(SlotIndex);
}

void UNDLoadoutComponent::RequestUseActiveTool(FVector AimPoint)
{
	ServerUseActiveTool(AimPoint);
}

void UNDLoadoutComponent::RequestUseConsumable(FVector AimPoint)
{
	ServerUseConsumable(AimPoint);
}

void UNDLoadoutComponent::RequestPing(FVector AimPoint)
{
	ServerPing(AimPoint);
}

void UNDLoadoutComponent::ServerPing_Implementation(FVector_NetQuantize AimPoint)
{
	const double Now = GetServerNow();
	if (Now < PingSlot.ReadyServerTime)
	{
		return;
	}
	PingSlot.ReadyServerTime = Now + (PingTool ? PingTool->CooldownSeconds : 4.f);
	MulticastPing(AimPoint);
}

void UNDLoadoutComponent::ServerSelectSlot_Implementation(int32 SlotIndex)
{
	if (ActiveSlots.IsValidIndex(SlotIndex) && SelectedSlot != SlotIndex)
	{
		SelectedSlot = SlotIndex;
		// Cambiar de herramienta apaga la linterna.
		if (bFlashlightOn)
		{
			bFlashlightOn = false;
			OnFlashlightChanged.Broadcast(false);
		}
		OnLoadoutChanged.Broadcast();
	}
}

void UNDLoadoutComponent::ServerUseActiveTool_Implementation(FVector_NetQuantize AimPoint)
{
	if (ActiveSlots.IsValidIndex(SelectedSlot))
	{
		UseSlot(ActiveSlots[SelectedSlot], AimPoint);
	}
}

void UNDLoadoutComponent::ServerUseConsumable_Implementation(FVector_NetQuantize AimPoint)
{
	UseSlot(ConsumableSlot, AimPoint);
}

bool UNDLoadoutComponent::UseSlot(FNDToolSlot& Slot, const FVector& AimPoint)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UNDToolDefinition* Tool = Slot.Tool;
	if (!Pawn || !Tool || Slot.Charges == 0)
	{
		return false;
	}
	const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(this);
	if (Expedition && !Expedition->CanPlayerAct(Pawn))
	{
		return false;
	}

	const double Now = GetServerNow();
	if (Now < Slot.ReadyServerTime)
	{
		return false;
	}

	// El servidor limita el punto apuntado al alcance de la herramienta.
	const FVector Origin = Pawn->GetActorLocation();
	FVector Target = AimPoint;
	const FVector Offset = Target - Origin;
	if (Offset.Size() > Tool->PlaceRange)
	{
		Target = Origin + Offset.GetSafeNormal() * Tool->PlaceRange;
	}

	AActor* Spawned = nullptr;
	bool bConsumesCharge = true;
	switch (Tool->Effect)
	{
	case ENDToolEffect::Ping:
		MulticastPing(Target);
		bConsumesCharge = false;
		break;

	case ENDToolEffect::Flashlight:
		bFlashlightOn = !bFlashlightOn;
		OnFlashlightChanged.Broadcast(bFlashlightOn);
		bConsumesCharge = false;
		break;

	case ENDToolEffect::Cart:
		// Pasivo: usarlo no hace nada.
		return false;

	case ENDToolEffect::SpawnDeployable:
	{
		if (!Tool->DeployableClass)
		{
			return false;
		}
		FActorSpawnParameters Params;
		Params.Owner = Pawn;
		Params.Instigator = Pawn;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Facing(0.f, Pawn->GetControlRotation().Yaw, 0.f);
		Spawned = GetWorld()->SpawnActor<AActor>(Tool->DeployableClass, Target, Facing, Params);
		if (!Spawned)
		{
			return false;
		}
		break;
	}

	default:
		break;
	}

	if (Tool->UseNoiseLoudness > 0.f)
	{
		Pawn->MakeNoise(Tool->UseNoiseLoudness, Pawn, Origin);
	}

	Slot.ReadyServerTime = Now + Tool->CooldownSeconds;
	if (bConsumesCharge && Slot.Charges > 0)
	{
		--Slot.Charges;
	}
	OnToolUsed.Broadcast(Tool, Target, Spawned);
	OnLoadoutChanged.Broadcast();
	return true;
}

void UNDLoadoutComponent::MulticastPing_Implementation(FVector_NetQuantize Location)
{
	OnPing.Broadcast(Cast<APawn>(GetOwner()), Location);
}

void UNDLoadoutComponent::OnRep_Flashlight()
{
	OnFlashlightChanged.Broadcast(bFlashlightOn);
}

void UNDLoadoutComponent::OnRep_Loadout()
{
	OnLoadoutChanged.Broadcast();
}
