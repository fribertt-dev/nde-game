// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/NDTypes.h"
#include "NDNetworkSettings.generated.h"

class UNDLobbyStatusWidget;
class USoundAttenuation;
class UNDMainMenuWidget;
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

	/** Widget del menú principal. Cambiar por un WBP con estilo propio. */
	UPROPERTY(Config, EditAnywhere, Category = "Interfaz")
	TSoftClassPtr<UNDMainMenuWidget> MainMenuWidgetClass;

	/** Widget de estado de La Madriguera (listos y cuenta atrás). */
	UPROPERTY(Config, EditAnywhere, Category = "Interfaz")
	TSoftClassPtr<UNDLobbyStatusWidget> LobbyStatusWidgetClass;

	/** Contratos que se votan en La Madriguera. Provisional hasta que existan los Data Assets de contrato. */
	UPROPERTY(Config, EditAnywhere, Category = "Sesion")
	TArray<FNDContractOption> ContractOptions;

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

	/** Segundos que tiene un jugador caído para volver a la misma expedición (GDD 21.6). */
	UPROPERTY(Config, EditAnywhere, Category = "Expedicion", meta = (ClampMin = "0"))
	float RejoinWindowSeconds = 120.f;

	/** Segundos que el personaje de un jugador caído queda quieto en la casa antes de desaparecer. */
	UPROPERTY(Config, EditAnywhere, Category = "Expedicion", meta = (ClampMin = "0"))
	float DisconnectedPawnLingerSeconds = 10.f;

	/** Atenuación de la voz de los compañeros en la casa. Vacío: se usan los radios de abajo. */
	UPROPERTY(Config, EditAnywhere, Category = "Voz")
	TSoftObjectPtr<USoundAttenuation> VoiceAttenuation;

	/** Hasta esta distancia la voz se oye entera (GDD 31.2). */
	UPROPERTY(Config, EditAnywhere, Category = "Voz", meta = (ClampMin = "0"))
	float VoiceFullVolumeRadius = 300.f;

	/** A partir de aquí no se oye a un compañero, y el monstruo tampoco oye la voz. */
	UPROPERTY(Config, EditAnywhere, Category = "Voz", meta = (ClampMin = "1"))
	float VoiceNoiseMaxRange = 1400.f;

	/** Volumen de ruido (0-1) de un comentario breve. */
	UPROPERTY(Config, EditAnywhere, Category = "Voz", meta = (ClampMin = "0", ClampMax = "1"))
	float VoiceBriefLoudness = 0.35f;

	/** Volumen de ruido (0-1) de una conversación sostenida. */
	UPROPERTY(Config, EditAnywhere, Category = "Voz", meta = (ClampMin = "0", ClampMax = "1"))
	float VoiceSustainedLoudness = 0.7f;

	/** Segundos hablando seguidos a partir de los cuales cuenta como conversación sostenida. */
	UPROPERTY(Config, EditAnywhere, Category = "Voz", meta = (ClampMin = "0"))
	float VoiceSustainedSeconds = 3.f;
};
