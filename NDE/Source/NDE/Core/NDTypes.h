// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "NDTypes.generated.h"

/** Fase de la sala en La Madriguera (GDD 22.5). */
UENUM(BlueprintType)
enum class ENDLobbyPhase : uint8
{
	Waiting UMETA(ToolTip = "Esperando jugadores en La Madriguera."),
	Voting UMETA(ToolTip = "Votando el contrato."),
	ReadyCheck UMETA(ToolTip = "Todos listos; cuenta atrás hacia la expedición."),
	Traveling UMETA(ToolTip = "Viajando a la expedición.")
};

/**
 * Fase de red de una expedición: carga, juego, liquidación y vuelta.
 * Las fases de reglas (GDD 7.1) viven en Expedition/NDExpeditionTypes.h
 * (ENDExpeditionPhase) y las decide UNDExpeditionComponent.
 */
UENUM(BlueprintType)
enum class ENDExpeditionSessionPhase : uint8
{
	Loading UMETA(ToolTip = "Esperando a que todos carguen el mapa."),
	InProgress UMETA(ToolTip = "Expedición en juego."),
	Settlement UMETA(ToolTip = "Resultados congelados; se guardan perfiles."),
	ReturningToBase UMETA(ToolTip = "Volviendo a La Madriguera.")
};

/** Motivo por el que el jugador vuelve al menú, para mostrarlo en la UI. */
UENUM(BlueprintType)
enum class ENDDisconnectReason : uint8
{
	None UMETA(ToolTip = "Sin motivo."),
	LeftVoluntarily UMETA(ToolTip = "Saliste de la sala."),
	HostLeft UMETA(ToolTip = "El host cerro la partida."),
	ConnectionLost UMETA(ToolTip = "Se perdio la conexion con la partida."),
	TravelFailed UMETA(ToolTip = "No se pudo cargar el mapa."),
	VersionMismatch UMETA(ToolTip = "La sala usa otra version del juego."),
	SessionFull UMETA(ToolTip = "La sala esta llena."),
	JoinFailed UMETA(ToolTip = "No se pudo entrar en la sala."),
	ExpeditionInProgress UMETA(ToolTip = "La banda esta en plena expedicion."),
	RejoinExpired UMETA(ToolTip = "Paso el tiempo para volver a la partida.")
};

/** Contrato que se puede votar en La Madriguera. */
USTRUCT(BlueprintType)
struct FNDContractOption
{
	GENERATED_BODY()

	/** Id estable del contrato (GDD 23.5); lo recibe la expedición en la URL de viaje. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ContractId;

	/** Nombre del contrato que ve el jugador al votar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
};
