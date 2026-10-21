// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "NDTypes.generated.h"

/** Fase de la sala en La Madriguera (GDD 22.5). */
UENUM(BlueprintType)
enum class ENDLobbyPhase : uint8
{
	Waiting,
	Voting,
	ReadyCheck,
	Traveling
};

/**
 * Fase de red de una expedición: carga, juego, liquidación y vuelta.
 * Las fases de reglas (GDD 7.1) viven en Expedition/NDExpeditionTypes.h
 * (ENDExpeditionPhase) y las decide UNDExpeditionComponent.
 */
UENUM(BlueprintType)
enum class ENDExpeditionSessionPhase : uint8
{
	/** Esperando a que todos carguen el mapa. */
	Loading,
	InProgress,
	/** Resultados congelados; se guardan perfiles. */
	Settlement,
	ReturningToBase
};

/** Motivo por el que el jugador vuelve al menú, para mostrarlo en la UI. */
UENUM(BlueprintType)
enum class ENDDisconnectReason : uint8
{
	None UMETA(DisplayName = "Sin motivo"),
	LeftVoluntarily UMETA(DisplayName = "Saliste de la sala"),
	HostLeft UMETA(DisplayName = "El host cerro la partida"),
	ConnectionLost UMETA(DisplayName = "Se perdio la conexion con la partida"),
	TravelFailed UMETA(DisplayName = "No se pudo cargar el mapa"),
	VersionMismatch UMETA(DisplayName = "La sala usa otra version del juego"),
	SessionFull UMETA(DisplayName = "La sala esta llena"),
	JoinFailed UMETA(DisplayName = "No se pudo entrar en la sala"),
	ExpeditionInProgress UMETA(DisplayName = "La banda esta en plena expedicion"),
	RejoinExpired UMETA(DisplayName = "Paso el tiempo para volver a la partida")
};
