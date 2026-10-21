// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NDNetworkSettings.generated.h"

class UWorld;

/**
 * Ajustes de red editables en Project Settings > Game > NDE Red.
 * Se guardan en DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NDE Red"))
class NDE_API UNDNetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNDNetworkSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UNDNetworkSettings* Get() { return GetDefault<UNDNetworkSettings>(); }

	/** Mapa al que se vuelve al salir de una sala o perder la conexión. */
	UPROPERTY(Config, EditAnywhere, Category = "Mapas")
	TSoftObjectPtr<UWorld> MainMenuMap;

	/** La Madriguera: sala de espera donde se vota y se prepara la expedición. */
	UPROPERTY(Config, EditAnywhere, Category = "Mapas")
	TSoftObjectPtr<UWorld> LobbyMap;

	/** Casa de pruebas hasta que exista el generador procedural. */
	UPROPERTY(Config, EditAnywhere, Category = "Mapas")
	TSoftObjectPtr<UWorld> ExpeditionTestMap;

	UPROPERTY(Config, EditAnywhere, Category = "Sesion", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MaxPlayers = 6;

	/** Jugadores mínimos listos para salir de expedición. 1 permite probar en solitario. */
	UPROPERTY(Config, EditAnywhere, Category = "Sesion", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MinPlayersToStart = 1;

	/** Segundos entre que todos están listos y el viaje, para poder cancelar. */
	UPROPERTY(Config, EditAnywhere, Category = "Sesion", meta = (ClampMin = "0"))
	float ReadyCountdownSeconds = 3.f;

	/**
	 * Versión de compatibilidad de red y contenido (GDD 23.5). Clientes con otra
	 * versión no ven la sala como compatible y el servidor los rechaza al conectar.
	 * Subirla cuando cambien catálogo, manifiestos o replicación.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sesion")
	FString CompatibilityVersion = TEXT("0.1.0");

	/** Tiempo máximo de Insertion esperando a que carguen todos los jugadores. */
	UPROPERTY(Config, EditAnywhere, Category = "Expedicion", meta = (ClampMin = "5"))
	float InsertionLoadTimeoutSeconds = 60.f;
};
