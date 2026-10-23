// No Despiertes Eso

#include "Monsters/NDMonsterUtils.h"

#include "Components/SpotLightComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HouseGen/NDHouseLot.h"
#include "Monsters/NDMonsterDefinition.h"
#include "Players/NDCharacter.h"
#include "UI/NDUIManagerSubsystem.h"
#include "UI/NDUITypes.h"

namespace NDMonsterUtils
{
	namespace
	{
		/** Llama a una función sin parámetros de un componente del pawn por su nombre y lee lo que devuelve. */
		bool CallComponentGetter(const APawn* Pawn, FName FunctionName, double& OutValue)
		{
			if (!Pawn)
			{
				return false;
			}
			TInlineComponentArray<UActorComponent*> Components(Pawn);
			for (UActorComponent* Component : Components)
			{
				// Solo el componente de escondite del personaje (UNDHidingComponent), no otros con el mismo nombre de función.
				if (!Component || !Component->GetClass()->GetName().Contains(TEXT("Hiding")))
				{
					continue;
				}
				UFunction* Function = Component->FindFunction(FunctionName);
				if (!Function || Function->NumParms != 1 || !Function->GetReturnProperty())
				{
					continue;
				}
				uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(Function->ParmsSize, 16)));
				FMemory::Memzero(Buffer, FMath::Max<int32>(Function->ParmsSize, 16));
				Component->ProcessEvent(Function, Buffer);
				const FProperty* Return = Function->GetReturnProperty();
				if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(Return))
				{
					OutValue = AsFloat->GetPropertyValue_InContainer(Buffer);
					return true;
				}
				if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Return))
				{
					OutValue = AsDouble->GetPropertyValue_InContainer(Buffer);
					return true;
				}
				if (const FBoolProperty* AsBool = CastField<FBoolProperty>(Return))
				{
					OutValue = AsBool->GetPropertyValue_InContainer(Buffer) ? 1.0 : 0.0;
					return true;
				}
			}
			return false;
		}
	}

	float GetSoundTransmission(const UObject* WorldContext, const FVector& A, const FVector& B)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const ANDHouseLot* Lot = World ? ANDHouseLot::FindLot(World) : nullptr;
		if (!Lot || !Lot->IsHouseBuilt())
		{
			return 1.f;
		}
		return Lot->GetSoundTransmission(A, B);
	}

	bool IsPawnHidden(const APawn* Pawn)
	{
		double Value = 0.0;
		return CallComponentGetter(Pawn, TEXT("IsHidden"), Value) && Value > 0.5;
	}

	float GetPawnVisibility(const APawn* Pawn, const UNDMonsterDefinition& Def)
	{
		if (!Pawn)
		{
			return 0.f;
		}
		double FromCharacter = 1.0;
		float Visibility = 1.f;
		if (CallComponentGetter(Pawn, TEXT("GetVisibilityFactor"), FromCharacter))
		{
			Visibility = static_cast<float>(FromCharacter);
		}
		else if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (Character->bIsCrouched)
			{
				Visibility = Def.CrouchedVisibility;
			}
		}
		if (Visibility <= 0.f)
		{
			return 0.f;
		}
		if (const ANDCharacter* Desvelado = Cast<ANDCharacter>(Pawn))
		{
			if (Desvelado->IsSprinting())
			{
				Visibility *= 1.25f;
			}
			// La linterna encendida delata (GDD 9.5).
			if (Desvelado->Flashlight && Desvelado->Flashlight->IsVisible())
			{
				Visibility *= 1.5f;
			}
		}
		return FMath::Clamp(Visibility, 0.f, 1.5f);
	}

	bool IsTargetablePlayer(const APawn* Pawn)
	{
		const APlayerState* PlayerState = IsValid(Pawn) ? Pawn->GetPlayerState() : nullptr;
		if (!PlayerState)
		{
			return false;
		}
		const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(Pawn);
		if (!Expedition)
		{
			return true;
		}
		// Sin registrar todavía (antes de entrar): se le puede ver igual.
		const bool bKnown = Expedition->GetParticipants().ContainsByPredicate([PlayerState](const FNDParticipant& P) { return P.PlayerState == PlayerState; });
		return !bKnown || Expedition->GetParticipantStatus(PlayerState) == ENDParticipantStatus::Active;
	}

	void ShowLocalToast(APlayerController* PC, const FText& Message, float Seconds, int32 Priority)
	{
		ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		UNDUIManagerSubsystem* UI = LocalPlayer ? LocalPlayer->GetSubsystem<UNDUIManagerSubsystem>() : nullptr;
		if (!UI)
		{
			return;
		}
		FNDUIToast Toast;
		Toast.Message = Message;
		Toast.Priority = Priority;
		Toast.DurationSeconds = Seconds;
		UI->ShowToast(Toast);
	}
}
