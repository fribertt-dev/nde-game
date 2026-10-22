// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "HouseGen/NDHouseLayout.h"

/**
 * Generador determinista de casas (GDD 15.1): misma semilla, misma versión y mismo lote
 * dan el mismo manifiesto. Solo lo usa el servidor; los clientes construyen desde el manifiesto.
 *
 * Cada intento usa una semilla derivada de Seed. Topología: Entrada en la fachada → distribuidor
 * → camino al dormitorio principal; la sala objetivo cuelga de ese camino; el resto de salas se
 * añaden donde quepan, más 0-2 bucles y quizá un acceso secundario al patio.
 */
struct NDE_API FNDHouseGenerator
{
	/**
	 * Genera y valida. Si ningún intento vale, devuelve la casa de respaldo (bUsedFallback).
	 * false solo si tampoco vale el respaldo; OutErrors recoge los fallos del último intento.
	 */
	static bool Generate(const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest, TArray<FString>* OutErrors = nullptr);

	/** Casa fija y sencilla para cuando la generación falla. */
	static bool BuildFallback(const FNDHouseGenParams& Params, FNDHouseGenerationManifest& OutManifest);

	/** Un único intento con una semilla concreta, sin validar. Para pruebas. */
	static bool TryGenerateAttempt(const FNDHouseGenParams& Params, int32 AttemptSeed, FNDHouseGenerationManifest& OutManifest);
};
