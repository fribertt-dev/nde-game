// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FNDHouseLayout;

namespace NDHouseGenDebug
{
	/**
	 * Dibuja con líneas persistentes el lote, cada sala (color por tipo), las puertas
	 * (bucles en naranja), la ruta del bulto entrada → objetivo (verde) y los puntos marcados.
	 * Con bDrawText añade tipo y profundidad (solo se ve en juego, necesita un HUD).
	 */
	NDE_API void DrawLayout(const UWorld* World, const FTransform& LotToWorld, const FNDHouseLayout& Layout, float CarryEnvelopeWidth, bool bDrawText);

	/** Etiqueta de una sala: "R06_MasterBedroom d3". */
	NDE_API FString GetRoomLabel(const FNDHouseLayout& Layout, int32 RoomIndex);
}
