// No Despiertes Eso

#include "Expedition/NDExpeditionDebugHud.h"

#include "Contracts/NDContractDefinition.h"
#include "Engine/Engine.h"
#include "Expedition/NDExpeditionComponent.h"
#include "Expedition/NDExpeditionDirector.h"
#include "Expedition/NDExpeditionPlayerComponent.h"
#include "Expedition/NDNightClockComponent.h"
#include "Loot/NDCarryComponent.h"
#include "Loot/NDLootActor.h"
#include "Loot/NDLootDefinition.h"
#include "Tools/NDLoadoutComponent.h"
#include "Tools/NDToolDefinition.h"

namespace NDExpeditionDebugHudText
{
	const TCHAR* PhaseText(ENDExpeditionPhase Phase)
	{
		switch (Phase)
		{
		case ENDExpeditionPhase::Insertion:       return TEXT("Inserci\u00F3n");
		case ENDExpeditionPhase::Infiltration:    return TEXT("Infiltraci\u00F3n");
		case ENDExpeditionPhase::ExtractionReady: return TEXT("Extracci\u00F3n disponible");
		case ENDExpeditionPhase::DawnEscapeGrace: return TEXT("\u00A1AMANECI\u00D3! Huida final");
		case ENDExpeditionPhase::Settlement:      return TEXT("Liquidaci\u00F3n");
		default:                                  return TEXT("?");
		}
	}

	const TCHAR* NightText(ENDNightPhase Phase)
	{
		switch (Phase)
		{
		case ENDNightPhase::NotStarted: return TEXT("sin empezar");
		case ENDNightPhase::DeepNight:  return TEXT("noche profunda");
		case ENDNightPhase::Madrugada:  return TEXT("madrugada");
		case ENDNightPhase::PreDawn:    return TEXT("pre-amanecer");
		case ENDNightPhase::DawnGrace:  return TEXT("amanecer");
		case ENDNightPhase::DayEnded:   return TEXT("fin del d\u00EDa");
		default:                        return TEXT("?");
		}
	}

	const TCHAR* ConditionText(ENDLootCondition Condition)
	{
		switch (Condition)
		{
		case ENDLootCondition::Intact:  return TEXT("intacto");
		case ENDLootCondition::Damaged: return TEXT("da\u00F1ado");
		case ENDLootCondition::Broken:  return TEXT("roto");
		default:                        return TEXT("?");
		}
	}

	const TCHAR* OutcomeText(ENDExpeditionOutcome Outcome)
	{
		switch (Outcome)
		{
		case ENDExpeditionOutcome::Perfect:           return TEXT("\u00C9XITO PERFECTO");
		case ENDExpeditionOutcome::Success:           return TEXT("\u00C9XITO");
		case ENDExpeditionOutcome::PartialExtraction: return TEXT("EXTRACCI\u00D3N PARCIAL");
		case ENDExpeditionOutcome::Failure:           return TEXT("FRACASO");
		default:                                      return TEXT("-");
		}
	}

	const TCHAR* StatusText(ENDParticipantStatus Status)
	{
		switch (Status)
		{
		case ENDParticipantStatus::Active:       return TEXT("activo");
		case ENDParticipantStatus::Captured:     return TEXT("capturado");
		case ENDParticipantStatus::Extracted:    return TEXT("extra\u00EDdo");
		case ENDParticipantStatus::LeftBehind:   return TEXT("se qued\u00F3 atr\u00E1s");
		case ENDParticipantStatus::Disconnected: return TEXT("desconectado");
		default:                                 return TEXT("?");
		}
	}

	FString FormatClock(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(FMath::Max(0.f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
	}

	FString ToolName(const UNDToolDefinition* Tool)
	{
		return Tool ? Tool->DisplayName.ToString() : TEXT("(vac\u00EDo)");
	}
}

void NDExpeditionDebugHud::Draw(const UObject* Context, const UNDCarryComponent& Carry, const UNDLoadoutComponent& Loadout, const UNDExpeditionPlayerComponent& ExpeditionPlayer)
{
	if (!GEngine)
	{
		return;
	}
	using namespace NDExpeditionDebugHudText;
	const UNDExpeditionComponent* Expedition = ANDExpeditionDirector::GetExpedition(Context);
	const UNDNightClockComponent* Clock = ANDExpeditionDirector::GetNightClock(Context);
	if (!Expedition || !Clock)
	{
		GEngine->AddOnScreenDebugMessage(9100, 0.5f, FColor::Red, TEXT("No hay ANDExpeditionDirector en el nivel."));
		return;
	}

	TArray<FString> Lines;
	const UNDContractDefinition* Contract = Expedition->Contract;
	Lines.Add(FString::Printf(TEXT("Contrato: %s (tier %d)"),
		Contract ? *Contract->DisplayName.ToString() : TEXT("ninguno"), Contract ? Contract->DifficultyTier : 0));

	if (Expedition->GetPhase() == ENDExpeditionPhase::Settlement)
	{
		const FNDRunSummary& Summary = Expedition->GetRunSummary();
		Lines.Add(FString::Printf(TEXT("RESULTADO: %s"), OutcomeText(Summary.Outcome)));
		Lines.Add(FString::Printf(TEXT("Bot\u00EDn: %d puntos (valor vendido %d) | cuota %d | objetivo %s | secundario %s"),
			Summary.LootPointsDeposited, Summary.LootSoldValue, Summary.Quota,
			Summary.bObjectiveMet ? TEXT("s\u00ED") : TEXT("no"), Summary.bSecondaryMet ? TEXT("s\u00ED") : TEXT("no")));
		for (const FNDPlayerSettlement& Player : Summary.Players)
		{
			Lines.Add(FString::Printf(TEXT("  %s: %s, +%d monedas, +%d reputaci\u00F3n"),
				*Player.PlayerName, StatusText(Player.FinalStatus), Player.Reward.Coins, Player.Reward.Reputation));
		}
	}
	else
	{
		FString TimeLine;
		if (Expedition->GetPhase() == ENDExpeditionPhase::Insertion)
		{
			TimeLine = FString::Printf(TEXT("Entra en la casa: la noche empieza en %s"), *FormatClock(Expedition->GetInsertionSecondsRemaining()));
		}
		else if (Clock->IsDawnTriggered())
		{
			TimeLine = FString::Printf(TEXT("\u00A1SE DESPERT\u00D3! Quedan %s para escapar"), *FormatClock(Clock->GetGraceSecondsRemaining()));
		}
		else
		{
			TimeLine = FString::Printf(TEXT("Amanecer en %s (%s)"), *FormatClock(Clock->GetSecondsUntilDawn()), NightText(Clock->GetPhase()));
		}
		Lines.Add(FString::Printf(TEXT("%s | %s"), PhaseText(Expedition->GetPhase()), *TimeLine));

		Lines.Add(FString::Printf(TEXT("Bot\u00EDn depositado: %d / %d puntos | Objetivo: %s"),
			Expedition->GetLootPoints(), Expedition->GetQuota(), Expedition->IsObjectiveMet() ? TEXT("CUMPLIDO") : TEXT("pendiente")));

		if (const ANDLootActor* Held = Carry.GetHeldLoot())
		{
			const UNDLootDefinition* Definition = Held->GetDefinition();
			Lines.Add(FString::Printf(TEXT("Llevas: %s (%s) | portadores %d%s"),
				Definition ? *Definition->DisplayName.ToString() : TEXT("?"), ConditionText(Held->GetCondition()),
				Held->GetCarrierCount(), Held->HasEnoughCarriers() ? TEXT("") : TEXT(" | necesitas ayuda o el carrito")));
		}
		Lines.Add(FString::Printf(TEXT("Mochila: %d / %d"), Carry.GetPocketLoot().Num(), Carry.PocketSlots));

		const TArray<FNDToolSlot>& Slots = Loadout.GetActiveSlots();
		const FNDToolSlot& Consumable = Loadout.GetConsumableSlot();
		Lines.Add(FString::Printf(TEXT("Herramientas: %s%s  %s%s | Consumible (Q): %s x%d"),
			Loadout.GetSelectedSlot() == 0 ? TEXT("> ") : TEXT(""), Slots.IsValidIndex(0) ? *ToolName(Slots[0].Tool) : TEXT("-"),
			Loadout.GetSelectedSlot() == 1 ? TEXT("> ") : TEXT(""), Slots.IsValidIndex(1) ? *ToolName(Slots[1].Tool) : TEXT("-"),
			*ToolName(Consumable.Tool), FMath::Max(0, Consumable.Charges)));

		if (Expedition->IsEscapeVoteActive())
		{
			Lines.Add(FString::Printf(TEXT("VOTACI\u00D3N DE ESCAPE: %d s\u00ED / %d no (%s). E vota s\u00ED en la camioneta, N vota no."),
				Expedition->GetVotesYes(), Expedition->GetVotesNo(), *FormatClock(Expedition->GetEscapeVoteSecondsRemaining())));
		}
		if (Expedition->IsEvacuationActive())
		{
			Lines.Add(FString::Printf(TEXT("EVACUACI\u00D3N EN %s: \u00A1a la camioneta!"), *FormatClock(Expedition->GetEvacuationSecondsRemaining())));
		}

		if (ExpeditionPlayer.IsInsideVan())
		{
			Lines.Add(Expedition->GetPhase() == ENDExpeditionPhase::DawnEscapeGrace
				? TEXT("En la camioneta: E para huir ya")
				: TEXT("En la camioneta: E para proponer la retirada"));
		}
		else if (const ANDLootActor* Target = Carry.FindBestLootTarget())
		{
			const UNDLootDefinition* Definition = Target->GetDefinition();
			Lines.Add(FString::Printf(TEXT("Delante: %s (%d pts, %s). E para coger"),
				Definition ? *Definition->DisplayName.ToString() : TEXT("?"), Target->GetCurrentLootPoints(), ConditionText(Target->GetCondition())));
		}
	}

	GEngine->AddOnScreenDebugMessage(9100, 0.5f, FColor::White, FString::Join(Lines, TEXT("\n")));
}
