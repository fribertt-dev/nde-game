// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"

class UNDCarryComponent;
class UNDLoadoutComponent;
class UNDExpeditionPlayerComponent;

/** Texto de depuración con el estado de la expedición, hasta que exista el HUD de UMG. */
namespace NDExpeditionDebugHud
{
	NDE_API void Draw(const UObject* Context, const UNDCarryComponent& Carry, const UNDLoadoutComponent& Loadout, const UNDExpeditionPlayerComponent& ExpeditionPlayer);
}
