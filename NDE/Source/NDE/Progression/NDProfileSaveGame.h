// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NDProfileSaveGame.generated.h"

/**
 * Progreso del jugador guardado en su propio PC (GDD 21.8, opción C).
 * No es fiable frente a trampas: no sirve para rankings.
 */
UCLASS()
class NDE_API UNDProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;

	/** Para migraciones al cambiar el formato (GDD 23.5). */
	UPROPERTY()
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int64 Coins = 0;

	UPROPERTY()
	int32 Reputation = 0;

	UPROPERTY()
	int32 ExpeditionsCompleted = 0;

	UPROPERTY()
	TArray<FName> UnlockedIds;

	/** Últimas expediciones ya cobradas, para no cobrar dos veces la misma (idempotencia). */
	UPROPERTY()
	TArray<FGuid> SettledRunGuids;
};
