// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "NDHouseGenTypes.generated.h"

class UWorld;

namespace NDHouseGen
{
	/** Lado de una celda de la rejilla de casas, en cm (GDD 15). */
	constexpr float CellSize = 400.f;
	/** Alto de pared y de hueco de puerta de los módulos greybox, en cm. */
	constexpr float WallHeight = 300.f;
	constexpr float DoorHeight = 220.f;
	constexpr float WallThickness = 20.f;
	/** Versión del manifiesto; súbela si cambia su formato. */
	constexpr int32 SchemaVersion = 1;
	/** Versión del algoritmo; con la misma semilla y versión sale la misma casa. */
	inline const TCHAR* GenerationVersion = TEXT("housegen-0.1");
}

/** Tipo de hueco en el borde de un módulo de sala (GDD 15.2). */
UENUM(BlueprintType)
enum class ENDRoomSocketType : uint8
{
	Door_S UMETA(ToolTip = "Puerta pequeña (100 cm). No deja pasar el bulto."),
	Door_M UMETA(ToolTip = "Puerta normal (160 cm de ancho libre)."),
	Door_L UMETA(ToolTip = "Puerta grande (220 cm)."),
	Arch_L UMETA(ToolTip = "Arco abierto grande (260 cm)."),
	Vent UMETA(ToolTip = "Rejilla de ventilación; solo conecta con otra rejilla."),
	Stairs UMETA(ToolTip = "Arranque de escalera hacia otra planta (aún sin uso)."),
	Outdoor UMETA(ToolTip = "Puerta al exterior: entrada y salida de la banda."),
	Secret UMETA(ToolTip = "Paso secreto (100 cm).")
};

/**
 * Tipo funcional de sala R01–R18. Nombres provisionales hasta cerrar la tabla del GDD 14;
 * el generador de H01 solo usa los de planta principal.
 */
UENUM(BlueprintType)
enum class ENDRoomType : uint8
{
	R01_Entry UMETA(ToolTip = "Vestíbulo / entrada. Aquí aparece la banda y está la salida."),
	R02_Corridor UMETA(ToolTip = "Pasillo o distribuidor."),
	R03_LivingRoom UMETA(ToolTip = "Sala de estar."),
	R04_Kitchen UMETA(ToolTip = "Cocina."),
	R05_DiningRoom UMETA(ToolTip = "Comedor."),
	R06_MasterBedroom UMETA(ToolTip = "Dormitorio principal: aquí duerme la criatura gigante."),
	R07_Bathroom UMETA(ToolTip = "Baño."),
	R08_Storage UMETA(ToolTip = "Almacén / trastero."),
	R09_Objective UMETA(ToolTip = "Sala objetivo del contrato."),
	R10_KidsBedroom UMETA(ToolTip = "Dormitorio infantil."),
	R11_Study UMETA(ToolTip = "Despacho."),
	R12_Pantry UMETA(ToolTip = "Despensa."),
	R13_Laundry UMETA(ToolTip = "Lavadero."),
	R14_Garage UMETA(ToolTip = "Garaje."),
	R15_Basement UMETA(ToolTip = "Sótano (otra planta)."),
	R16_Attic UMETA(ToolTip = "Desván (otra planta)."),
	R17_Stairwell UMETA(ToolTip = "Escalera entre plantas."),
	R18_Garden UMETA(ToolTip = "Jardín o patio.")
};

/** Lado de una celda. North = +X, East = +Y (ejes de Unreal). */
UENUM(BlueprintType)
enum class ENDCellDirection : uint8
{
	North UMETA(ToolTip = "Hacia +X."),
	East UMETA(ToolTip = "Hacia +Y."),
	South UMETA(ToolTip = "Hacia -X."),
	West UMETA(ToolTip = "Hacia -Y.")
};

/** Punto marcado dentro de un módulo. */
UENUM(BlueprintType)
enum class ENDRoomMarkerType : uint8
{
	PlayerSpawn UMETA(ToolTip = "Donde aparece un jugador al empezar."),
	Extraction UMETA(ToolTip = "Punto de extracción: aquí se entrega el botín."),
	LootSocket UMETA(ToolTip = "Hueco donde puede aparecer botín."),
	MonsterAnchor UMETA(ToolTip = "Donde duerme o vuelve la criatura gigante."),
	HideSpot UMETA(ToolTip = "Escondite para un Desvelado.")
};

/** Hueco de puerta en el borde de un módulo, en coordenadas del módulo sin rotar. */
USTRUCT(BlueprintType)
struct FNDRoomDoorSocket
{
	GENERATED_BODY()

	/** Id único dentro del módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName SocketId;

	/** Celda del módulo (0,0 = esquina mínima) en cuyo borde está el hueco. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FIntPoint Cell = FIntPoint::ZeroValue;

	/** Lado de la celda donde está el hueco; debe dar al exterior del módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDCellDirection Direction = ENDCellDirection::North;

	/** Tipo de hueco. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDRoomSocketType Type = ENDRoomSocketType::Door_M;

	/** Ancho libre del hueco en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float Width = 160.f;
};

/** Punto marcado de un módulo, relativo al centro del módulo sin rotar. */
USTRUCT(BlueprintType)
struct FNDRoomMarker
{
	GENERATED_BODY()

	/** Qué marca este punto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDRoomMarkerType Type = ENDRoomMarkerType::LootSocket;

	/** Posición en cm respecto al centro del módulo (Z = 0 es el suelo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FVector LocalPosition = FVector::ZeroVector;

	/** Giro en grados respecto al módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	float Yaw = 0.f;
};

/** Definición de un módulo de sala (GDD 15.2). */
USTRUCT(BlueprintType)
struct FNDRoomModuleDefinition
{
	GENERATED_BODY()

	/** Id estable; es lo que guarda el manifiesto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName RoomModuleId;

	/** Tipo funcional de la sala. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDRoomType RoomType = ENDRoomType::R03_LivingRoom;

	/** Tamaño en celdas de 400 cm (X, Y) sin rotar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "1"))
	FIntPoint FootprintCells = FIntPoint(1, 1);

	/** Huecos posibles. Los que no use el generador se tapan con pared. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FNDRoomDoorSocket> DoorSockets;

	/** Distritos donde puede salir (p. ej. D01). Vacío = todos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FName> AllowedDistricts;

	/** Cuánto sube la dificultad de la casa al usar este módulo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float DifficultyWeight = 1.f;

	/** Spawns, extracción, botín, criatura y escondites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FNDRoomMarker> Markers;

	/** Más adelante: Level Instance con el arte de Blender que sustituye al greybox. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TSoftObjectPtr<UWorld> ArtLevel;

	/** Ancho útil del módulo (lado corto menos paredes), en cm. */
	float GetMinClearWidth() const
	{
		return FMath::Min(FootprintCells.X, FootprintCells.Y) * NDHouseGen::CellSize - 2.f * NDHouseGen::WallThickness;
	}

	bool IsAllowedIn(FName District) const
	{
		return AllowedDistricts.IsEmpty() || District.IsNone() || AllowedDistricts.Contains(District);
	}
};

/**
 * Parcela donde se levanta la casa. La rejilla del lote empieza en su esquina mínima;
 * la fachada (FrontDirection) da a la calle y deja FrontYardCells libres para la extracción.
 */
USTRUCT(BlueprintType)
struct FNDHouseLotInfo
{
	GENERATED_BODY()

	/** Id del lote en la calle; con varias casas por calle distingue cada una. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName LotId = TEXT("Lot01");

	/** Transform del centro del lote en el mundo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FTransform LotTransform;

	/** Tamaño máximo del lote en celdas de 400 cm (X, Y). Ninguna sala puede salirse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "3", ClampMax = "32"))
	FIntPoint SizeCells = FIntPoint(10, 10);

	/** Lado del lote que da a la calle (en ejes del lote). Ahí van la entrada y la extracción. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDCellDirection FrontDirection = ENDCellDirection::South;

	/** Filas de jardín delantero sin salas, entre la fachada y la calle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "1", ClampMax = "4"))
	int32 FrontYardCells = 1;

	bool IsInsideLot(const FIntPoint& Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < SizeCells.X && Cell.Y < SizeCells.Y;
	}

	/** true si la celda está en el jardín delantero. */
	bool IsFrontYard(const FIntPoint& Cell) const
	{
		switch (FrontDirection)
		{
		case ENDCellDirection::North: return Cell.X >= SizeCells.X - FrontYardCells;
		case ENDCellDirection::East: return Cell.Y >= SizeCells.Y - FrontYardCells;
		case ENDCellDirection::South: return Cell.X < FrontYardCells;
		default: return Cell.Y < FrontYardCells;
		}
	}

	/** true si una sala puede ocupar la celda. */
	bool IsBuildable(const FIntPoint& Cell) const { return IsInsideLot(Cell) && !IsFrontYard(Cell); }

	/** Centro de una celda en cm, relativo al centro del lote. */
	FVector GetCellCenter(const FIntPoint& Cell) const
	{
		return FVector((Cell.X + 0.5f - SizeCells.X * 0.5f) * NDHouseGen::CellSize, (Cell.Y + 0.5f - SizeCells.Y * 0.5f) * NDHouseGen::CellSize, 0.f);
	}
};

/** Una sala colocada en la casa. */
USTRUCT(BlueprintType)
struct FNDHouseManifestRoom
{
	GENERATED_BODY()

	/** Índice de la sala en el manifiesto. La 0 es siempre la entrada. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 InstanceId = INDEX_NONE;

	/** Módulo usado (RoomModuleId). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName ModuleId;

	/** Celda mínima que ocupa (Z = planta; de momento siempre 0). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FIntVector Grid = FIntVector::ZeroValue;

	/** Giro en grados: 0, 90, 180 o 270. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 Yaw = 0;

	/** Puertas que hay que cruzar desde la entrada. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 Depth = 0;
};

/** Puerta abierta entre dos salas. */
USTRUCT(BlueprintType)
struct FNDHouseManifestConnection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 RoomA = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName SocketA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 RoomB = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName SocketB;

	/** true si es un bucle opcional (no hace falta para llegar a ninguna sala). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bIsLoop = false;
};

/**
 * Todo lo necesario para construir la misma casa en cualquier máquina (GDD 15.3).
 * El servidor lo genera y lo replica; los clientes solo lo construyen.
 */
USTRUCT(BlueprintType)
struct NDE_API FNDHouseGenerationManifest
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 SchemaVersion = NDHouseGen::SchemaVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FString GenerationVersion;

	/** Semilla pedida para la partida. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int64 RunSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName ContractId;

	/** Arquetipo de casa (H01...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName HouseArchetype;

	/** Distrito (D01...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName District;

	/** Lote donde se construye: id, transform, tamaño y fachada. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FNDHouseLotInfo Lot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FNDHouseManifestRoom> Rooms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FNDHouseManifestConnection> Connections;

	/** Sala con acceso secundario al patio trasero o lateral (INDEX_NONE = sin acceso). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 SecondaryExitRoom = INDEX_NONE;

	/** Hueco de esa sala que se abre al exterior. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName SecondaryExitSocket;

	/** Intentos que necesitó el generador (cada uno con una semilla derivada). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 Attempts = 0;

	/** true si ningún intento valió y se usó la casa de respaldo fija. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bUsedFallback = false;

	/** Sube en cada regeneración para que los clientes reconstruyan aunque se repita la semilla. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	int32 Revision = 0;

	bool IsValid() const { return Rooms.Num() > 0; }

	/** Texto con todo el contenido salvo Revision; sirve para comparar casas y para el log. */
	FString ToDebugString() const;
};

namespace NDHouseGen
{
	NDE_API FIntPoint DirectionToOffset(ENDCellDirection Direction);
	NDE_API ENDCellDirection RotateDirection(ENDCellDirection Direction, int32 YawSteps);
	NDE_API ENDCellDirection OppositeDirection(ENDCellDirection Direction);
	/** Celda de un módulo de tamaño Size tras girarlo YawSteps × 90°, con la esquina mínima en 0,0. */
	NDE_API FIntPoint RotateCell(const FIntPoint& Cell, const FIntPoint& Size, int32 YawSteps);
	NDE_API FIntPoint RotateSize(const FIntPoint& Size, int32 YawSteps);
	/** 0..3 a partir de grados múltiplos de 90. */
	NDE_API int32 YawToSteps(int32 YawDegrees);
	/** Ancho por defecto del hueco según su tipo, en cm. */
	NDE_API float GetDefaultSocketWidth(ENDRoomSocketType Type);
	/** true si un hueco de este tipo puede unir dos salas. */
	NDE_API bool IsConnectableSocket(ENDRoomSocketType Type);
	NDE_API bool AreSocketsCompatible(ENDRoomSocketType A, ENDRoomSocketType B);
	/** Color de depuración por tipo de sala. */
	NDE_API FColor GetRoomTypeColor(ENDRoomType Type);
	/** "R06_MasterBedroom" */
	NDE_API FString GetRoomTypeName(ENDRoomType Type);
}
