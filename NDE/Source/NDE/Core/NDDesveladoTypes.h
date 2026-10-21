#pragma once

#include "CoreMinimal.h"
#include "NDDesveladoTypes.generated.h"

// Especies jugables de Los Desvelados (GDD 3.1, v1.2). Solo estéticas en la primera entrega
// y únicas por partida: dos jugadores no pueden elegir la misma.
UENUM(BlueprintType)
enum class ENDDesveladoSpecies : uint8
{
	None		UMETA(Hidden),
	Raccoon		UMETA(DisplayName = "Mapache"),
	Lemur		UMETA(DisplayName = "Lémur"),
	Bat			UMETA(DisplayName = "Murciélago"),
	Fox			UMETA(DisplayName = "Zorro pequeño"),
	Ferret		UMETA(DisplayName = "Hurón"),
	Owl			UMETA(DisplayName = "Búho"),
};
