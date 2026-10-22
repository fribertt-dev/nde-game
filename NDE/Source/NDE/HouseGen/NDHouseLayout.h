// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "NDHouseLayout.generated.h"

/** Reglas de generación de una casa (GDD 15.1). */
USTRUCT(BlueprintType)
struct NDE_API FNDHouseGenParams
{
	GENERATED_BODY()

	/** Semilla de la partida. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	int64 Seed = 1;

	/** Arquetipo de casa. De momento solo H01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FName HouseArchetype = TEXT("H01");

	/** Distrito: filtra qué módulos se pueden usar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FName District = TEXT("D01");

	/** Contrato de la partida; solo se copia al manifiesto. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FName ContractId;

	/** Lote donde se construye. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	FNDHouseLotInfo Lot;

	/** Tamaño de casa; fija los valores por defecto de salas, bucles y lote mínimo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	ENDHouseSize HouseSize = ENDHouseSize::Small;

	/** Salas mínimas y máximas (se elige al azar entre ambas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "5", ClampMax = "17"))
	int32 MinRooms = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "5", ClampMax = "17"))
	int32 MaxRooms = 8;

	/** Bucles (puertas extra entre salas vecinas) mínimos y máximos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	int32 MinLoops = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	int32 MaxLoops = 2;

	/** Profundidad del dormitorio principal (puertas desde la entrada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "2"))
	int32 MinBedroomDepth = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "2"))
	int32 MaxBedroomDepth = 5;

	/** Profundidad de la sala objetivo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "2"))
	int32 MinObjectiveDepth = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "2"))
	int32 MaxObjectiveDepth = 7;

	/** Huecos de botín mínimos en toda la casa. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	int32 MinLootSockets = 6;

	/** Ancho del bulto cargado, en cm. Debe ser mayor que la cápsula del Desvelado (44 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float CarryEnvelopeWidth = 120.f;

	/** Probabilidad (0-1) de que la casa tenga piso superior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "1"))
	float UpperFloorChance = 0.1f;

	/** Probabilidad (0-1) de que la casa tenga sótano. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "1"))
	float BasementChance = 0.15f;

	/** Con piso superior, probabilidad (0-1) de que el dormitorio principal esté arriba. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "1"))
	float UpstairsBedroomChance = 0.8f;

	/** Probabilidad (0-1) de abrir un acceso secundario al patio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0", ClampMax = "1"))
	float SecondaryExitChance = 0.5f;

	/** Intentos con semillas derivadas antes de usar la casa de respaldo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "1"))
	int32 MaxAttempts = 24;

	/**
	 * Reglas por defecto de un arquetipo y tamaño. Si no se conoce el arquetipo, avisa y usa H01.
	 * Agranda el lote si es menor que el mínimo del tamaño.
	 */
	static FNDHouseGenParams ForArchetype(FName Archetype, ENDHouseSize Size = ENDHouseSize::Small);

	/** Lote mínimo (en celdas) para que quepa una casa de ese tamaño. */
	static FIntPoint GetMinLotSize(ENDHouseSize Size);

	/** Agranda Lot.SizeCells hasta el mínimo del tamaño. true si tuvo que agrandarlo. */
	bool GrowLotToFit();

	/** Usa este lote, agrandándolo (con aviso en el log) si la casa no cabe. */
	void SetLot(const FNDHouseLotInfo& InLot);

	/** Cambia el rango de salas; 0 deja el del tamaño. Se limita a 5-17. */
	void OverrideRoomRange(int32 InMinRooms, int32 InMaxRooms);

	/** Fuerza o quita el piso superior y el sótano; Auto deja la probabilidad del tamaño. */
	void ApplyFloorOptions(ENDFloorOption UpperFloor, ENDFloorOption Basement);
};

/** Hueco de una sala ya colocada, en celdas del lote. */
struct FNDHouseLayoutSocket
{
	FName SocketId;
	ENDRoomSocketType Type = ENDRoomSocketType::Door_M;
	float Width = 0.f;
	/** Desplazamiento del hueco a lo largo de la pared (ver FNDRoomDoorSocket::Offset). */
	float Offset = 0.f;
	/** Paso bajo: la criatura no cabe. */
	bool bLowClearance = false;
	FIntPoint LotCell = FIntPoint::ZeroValue;
	ENDCellDirection LotDirection = ENDCellDirection::North;
	/** Índice en FNDHouseLayout::Connections, o INDEX_NONE. */
	int32 Connection = INDEX_NONE;
	/** true si se abre al exterior (entrada principal o acceso secundario). */
	bool bExit = false;
	/** Puerta "Auto": no está en el módulo; se abrió para unir una escalera. Celda y lado del módulo sin girar. */
	bool bAuto = false;
	FIntPoint LocalCell = FIntPoint::ZeroValue;
	ENDCellDirection LocalDirection = ENDCellDirection::North;

	bool IsOpen() const { return Connection != INDEX_NONE || bExit; }
	/** Celda vecina al otro lado del hueco. */
	FIntPoint GetOutsideCell() const { return LotCell + NDHouseGen::DirectionToOffset(LotDirection); }
};

struct FNDHouseLayoutRoom
{
	int32 InstanceId = INDEX_NONE;
	const FNDRoomModuleDefinition* Def = nullptr;
	FIntPoint Grid = FIntPoint::ZeroValue;
	/** -1 sótano, 0 planta baja, 1 piso superior. */
	int32 Floor = 0;
	FIntPoint Size = FIntPoint(1, 1);
	int32 YawSteps = 0;
	int32 Depth = 0;
	TArray<FNDHouseLayoutSocket> Sockets;
};

struct FNDHouseLayoutConnection
{
	int32 RoomA = INDEX_NONE;
	int32 SocketA = INDEX_NONE;
	int32 RoomB = INDEX_NONE;
	int32 SocketB = INDEX_NONE;
	float OpeningWidth = 0.f;
	bool bIsLoop = false;
};

struct FNDHouseLayoutMarker
{
	ENDRoomMarkerType Type = ENDRoomMarkerType::LootSocket;
	/** Sala a la que pertenece (la extracción, que está fuera, usa la entrada). */
	int32 Room = INDEX_NONE;
	/** Relativo al centro del lote. */
	FTransform LotTransform;
};

// --- Puntos de juego. Salen del layout (y de la semilla), así que son iguales en todas las máquinas.
// StableId es el índice en su lista: sirve de StableSpawnId para lo que se cree en ellos.

/** Escondite real: cuántos caben, por dónde se entra y si se ve desde fuera. */
struct FNDHouseHidingSpot
{
	int32 StableId = INDEX_NONE;
	int32 Room = INDEX_NONE;
	/** Relativo al centro del lote. */
	FTransform LotTransform;
	/** Punto de entrada y salida, en la sala, hacia su centro. */
	FVector EntryPoint = FVector::ZeroVector;
	int32 Capacity = 1;
	ENDHidingSpotKind Kind = ENDHidingSpotKind::BehindFurniture;
};

/** Peligro fijo de la casa, siempre fuera de la ruta del bulto y lejos de las puertas. */
struct FNDHouseHazard
{
	int32 StableId = INDEX_NONE;
	int32 Room = INDEX_NONE;
	ENDHouseHazardType Type = ENDHouseHazardType::CreakyFloor;
	FTransform LotTransform;
};

/** Nodo de patrulla de la criatura: uno por sala a la que llega. */
struct FNDHousePatrolNode
{
	int32 StableId = INDEX_NONE;
	int32 Room = INDEX_NONE;
	/** Relativo al centro del lote, en el suelo de su piso. */
	FVector Location = FVector::ZeroVector;
	/** Nodos vecinos por puertas por las que cabe la criatura. */
	TArray<int32> Neighbors;
};

/** Cuánto sonido pasa por una puerta entre dos salas (0 = nada, 1 = todo). */
struct FNDHouseAudioLink
{
	int32 RoomA = INDEX_NONE;
	int32 RoomB = INDEX_NONE;
	float Transmission = 1.f;
};

/**
 * Casa resuelta a partir de un manifiesto y del catálogo. Servidor y clientes la calculan
 * igual, así que todo lo que se construye o se valida sale de aquí.
 */
struct NDE_API FNDHouseLayout
{
	FNDHouseLotInfo Lot;
	TArray<FNDHouseLayoutRoom> Rooms;
	TArray<FNDHouseLayoutConnection> Connections;
	TArray<FNDHouseLayoutMarker> Markers;
	int32 EntryRoom = INDEX_NONE;
	int32 EntryExitSocket = INDEX_NONE;
	int32 BedroomRoom = INDEX_NONE;
	int32 ObjectiveRoom = INDEX_NONE;
	/** Semilla del manifiesto: reparte los peligros. */
	int64 RunSeed = 0;

	TArray<FNDHouseHidingSpot> HidingSpots;
	TArray<FNDHouseHazard> Hazards;
	TArray<FNDHousePatrolNode> PatrolNodes;
	/** Recorrido de patrulla desde el dormitorio: pasa por todos los nodos y vuelve (índices de PatrolNodes). */
	TArray<int32> PatrolRoute;
	TArray<FNDHouseAudioLink> AudioLinks;

	/** Paso del sonido por un hueco abierto, por una puerta cerrada y por una escalera. */
	static constexpr float DoorOpenTransmission = 0.6f;
	static constexpr float DoorClosedTransmission = 0.2f;
	static constexpr float StairsTransmission = 0.7f;
	/** Pasos anchos (salas abiertas, 300 cm o más) casi no frenan el sonido. */
	static constexpr float WideOpeningTransmission = 0.85f;
	/** Distancia mínima de un peligro a una puerta abierta, en cm. */
	static constexpr float HazardDoorClearance = 140.f;

	/** Rellena escondites, peligros, patrulla y grafo de sonido. Lo llama FromManifest. */
	void BuildGameplayPoints();

	/** Sala que contiene un punto (relativo al centro del lote), o INDEX_NONE. */
	int32 FindRoomAt(const FVector& LotPosition) const;

	/**
	 * Fracción del sonido (0-1) que llega de A a B por las puertas, sin contar la distancia:
	 * 1 en la misma sala; el mejor camino multiplica el paso de cada puerta. 0 si no hay camino o
	 * algún punto está fuera de la casa.
	 */
	float GetSoundTransmission(const FVector& LotA, const FVector& LotB) const;

	/** Salas por las que va el bulto (entrada → objetivo); ahí no puede haber peligros. */
	void GetCarryRouteRooms(float CarryEnvelopeWidth, TSet<int32>& OutRooms) const;

	/** false (con motivo) si el manifiesto pide módulos o huecos que no existen. */
	static bool FromManifest(const FNDHouseGenerationManifest& Manifest, FNDHouseLayout& OutLayout, FString& OutError);

	/** Centro de la sala en cm, relativo al centro del lote (Z = suelo de su piso). */
	FVector GetRoomCenter(int32 RoomIndex) const;

	/** Celdas ocupadas, como (X, Y, piso). */
	void GetOccupancy(TMap<FIntVector, int32>& OutOccupancy) const;

	/** Pisos que tiene la casa (de -1 a 1). */
	void GetFloorRange(int32& OutLowest, int32& OutHighest) const;
	/** Punto medio del hueco en el borde de la sala. */
	FVector GetSocketLocation(int32 RoomIndex, int32 SocketIndex) const;
	/** Celdas del lote que ocupa la sala. */
	void GetRoomCells(int32 RoomIndex, TArray<FIntPoint>& OutCells) const;

	/** Puertas desde la entrada por el camino más corto; INDEX_NONE si no se llega. */
	void ComputeDepths(TArray<int32>& OutDepths) const;

	/**
	 * Camino de salas From → To cruzando solo puertas y salas de al menos MinWidth cm.
	 * Con MinWidth = 0 es la conectividad normal.
	 */
	bool FindRoute(int32 FromRoom, int32 ToRoom, float MinWidth, TArray<int32>* OutRooms = nullptr, bool bCreature = false) const;

	/** true si la puerta es un paso bajo (no pasa la criatura). */
	bool IsLowClearance(const FNDHouseLayoutConnection& Link) const;

	int32 CountMarkers(ENDRoomMarkerType Type) const;
};

/** Comprobaciones de una casa resuelta (GDD 15.4). */
struct NDE_API FNDHouseValidator
{
	/** true si la casa es jugable; si no, OutErrors dice por qué. */
	static bool Validate(const FNDHouseLayout& Layout, const FNDHouseGenParams& Params, TArray<FString>& OutErrors);
};
