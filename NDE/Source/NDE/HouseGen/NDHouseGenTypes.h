// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "NDHouseGenTypes.generated.h"

namespace NDHouseGen
{
	/** Lado de una celda de la rejilla de casas, en cm (GDD 15). */
	constexpr float CellSize = 400.f;
	/**
	 * Altura de cada piso, en cm: la planta 0 tiene el suelo en Z = 0, la 1 en Z = 400 y el sótano (-1)
	 * en Z = -400. Las paredes llegan justo al forjado del piso de arriba (380 + 20 de forjado).
	 */
	constexpr float FloorHeight = 400.f;
	constexpr float WallThickness = 20.f;
	/** Alto de pared y de hueco de puerta de los módulos greybox, en cm. */
	constexpr float WallHeight = FloorHeight - WallThickness;
	constexpr float DoorHeight = 220.f;
	/** Pisos posibles: sótano, planta baja y piso superior. */
	constexpr int32 BasementFloor = -1;
	constexpr int32 GroundFloor = 0;
	constexpr int32 UpperFloor = 1;
	/** Versión del manifiesto; súbela si cambia su formato. */
	constexpr int32 SchemaVersion = 1;
	/** Versión del algoritmo; con la misma semilla y versión sale la misma casa. */
	inline const TCHAR* GenerationVersion = TEXT("housegen-0.3");
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
	R14_Library UMETA(ToolTip = "Biblioteca."),
	R15_Playroom UMETA(ToolTip = "Habitación de juguetes."),
	R16_Basement UMETA(ToolTip = "Sótano o taller bajo la planta baja."),
	R17_Stairwell UMETA(ToolTip = "Escalera entre plantas."),
	R18_Garden UMETA(ToolTip = "Jardín o patio.")
};

/** Parte de una escalera: cada escalera son dos salas iguales, una en cada piso. */
UENUM(BlueprintType)
enum class ENDStairPart : uint8
{
	None UMETA(ToolTip = "No es una escalera."),
	Bottom UMETA(ToolTip = "Arranque: rampa que sube al piso de arriba; sin techo."),
	Top UMETA(ToolTip = "Llegada: hueco en el suelo sobre la rampa.")
};

/** Si una casa tiene un piso extra: decide el tamaño de casa, o siempre sí / siempre no. */
UENUM(BlueprintType)
enum class ENDFloorOption : uint8
{
	Auto UMETA(ToolTip = "Según el tamaño de casa (las grandes lo tienen más a menudo)."),
	Never UMETA(ToolTip = "Nunca."),
	Always UMETA(ToolTip = "Siempre que quepa.")
};

/** Tamaño de casa (GDD 15.2): fija cuántas salas tiene y el lote mínimo. */
UENUM(BlueprintType)
enum class ENDHouseSize : uint8
{
	Small UMETA(ToolTip = "Casa pequeña: 5-8 salas."),
	Medium UMETA(ToolTip = "Casa mediana: 7-11 salas."),
	Large UMETA(ToolTip = "Casa grande: 10-17 salas, con 1-2 bucles y casi siempre una segunda salida.")
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

/** Cómo se esconde un Desvelado; decide cuántos caben y si se le ve desde fuera. */
UENUM(BlueprintType)
enum class ENDHidingSpotKind : uint8
{
	UnderFurniture UMETA(ToolTip = "Debajo de un mueble (cama, sofá): caben dos, no se les ve de pie."),
	InsideCabinet UMETA(ToolTip = "Dentro de un armario o mueble con puerta: cabe uno, no se le ve."),
	BehindFurniture UMETA(ToolTip = "Detrás de un mueble: cabe uno, se le ve desde un lado.")
};

/** Peligro fijo de la casa que hace ruido o atrapa. */
UENUM(BlueprintType)
enum class ENDHouseHazardType : uint8
{
	CreakyFloor UMETA(ToolTip = "Suelo que cruje al pisarlo."),
	FragileItem UMETA(ToolTip = "Objeto frágil en un mueble: se cae y suena si se roza."),
	MouseTrap UMETA(ToolTip = "Trampa de ratón: atrapa un momento y hace ruido.")
};

/** Mueble greybox de decoración: da cobertura y algunos sirven de escondite. */
UENUM(BlueprintType)
enum class ENDDecorationKind : uint8
{
	Table UMETA(ToolTip = "Mesa: se esconden debajo dos."),
	Armchair UMETA(ToolTip = "Sillón: uno detrás."),
	Wardrobe UMETA(ToolTip = "Armario: uno dentro."),
	Shelf UMETA(ToolTip = "Estantería: solo cobertura."),
	Bed UMETA(ToolTip = "Cama pequeña: dos debajo."),
	Crate UMETA(ToolTip = "Caja o baúl: solo cobertura.")
};

/** Dónde va el objetivo de un contrato dentro de la casa. */
UENUM(BlueprintType)
enum class ENDContractAnchor : uint8
{
	ObjectiveRoom UMETA(ToolTip = "En la sala objetivo."),
	RequiredRoom UMETA(ToolTip = "En la primera sala obligatoria del contrato (p. ej. la cocina)."),
	BedHeadboard UMETA(ToolTip = "En la cabecera de la cama de la criatura, en el borde accesible."),
	RescueContainer UMETA(ToolTip = "Dentro de un tarro que hay que abrir, en la sala objetivo.")
};

/** Lo que un contrato pide a la casa (GDD 12 y 15). La casa se genera y se valida con esto. */
USTRUCT(BlueprintType)
struct FNDContractHouseRequirements
{
	GENERATED_BODY()

	/** Salas que la casa tiene que tener, además de las de siempre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	TArray<ENDRoomType> RequiredRooms;

	/** Dónde va lo que pide el contrato. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	ENDContractAnchor ObjectiveAnchor = ENDContractAnchor::ObjectiveRoom;

	/** Ubicaciones candidatas para el objetivo (2-5); la semilla elige una y en otras van los señuelos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "1", ClampMax = "5"))
	int32 CandidateLocations = 2;

	/** Accesos (puertas) mínimos a la sala del objetivo: 1-3 rutas para llegar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MinRoutes = 1;

	/** Ancho mínimo de la ruta de la entrada al objetivo, en cm (0 = cualquiera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float MinRouteWidth = 0.f;

	/** Botín repartido mínimo frente a la cuota (1,35 = 35 % más). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen", meta = (ClampMin = "1"))
	float QuotaCoverage = 1.35f;

	/** Pide al menos una sala segura: con escondite y sin peligros. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NDE|HouseGen")
	bool bNeedsSafeRoom = false;
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

	/** Desplazamiento del hueco a lo largo de la pared, en cm (0 = centrado). La sala vecina lo copia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	float Offset = 0.f;

	/** Paso bajo (p. ej. bajo una rampa): caben los Desvelados, no la criatura. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	bool bLowClearance = false;
};

/**
 * Pieza de la geometría de una escalera, en cm respecto al centro del módulo sin rotar (Z = suelo).
 * Sin altura es una losa de 20 cm cuya cara de arriba va de Start a End (rampa o rellano);
 * con altura es un murete vertical de Start a End.
 */
USTRUCT(BlueprintType)
struct FNDStairSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FVector End = FVector::ZeroVector;

	/** Ancho de la rampa o grosor del murete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float Width = 360.f;

	/** 0 = losa (rampa o rellano); > 0 = murete de esta altura. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (ClampMin = "0"))
	float Height = 0.f;
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

	/** Si es una escalera, qué mitad es. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDStairPart StairPart = ENDStairPart::None;

	/** Escalera: el módulo de la otra mitad (la llegada si este es el arranque, y al revés). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	FName StairPairModuleId;

	/** Arranque de escalera: rampas, rellanos y muretes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FNDStairSegment> StairSegments;

	/** Celdas del rectángulo que NO son de la sala (salas en L). Vacío = rectángulo completo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	TArray<FIntPoint> BlockedCells;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath ArtLevel;

	/** Ancho útil del módulo (lado corto menos paredes), en cm. */
	float GetMinClearWidth() const
	{
		return FMath::Min(FootprintCells.X, FootprintCells.Y) * NDHouseGen::CellSize - 2.f * NDHouseGen::WallThickness;
	}

	bool IsAllowedIn(FName District) const
	{
		return AllowedDistricts.IsEmpty() || District.IsNone() || AllowedDistricts.Contains(District);
	}

	/** true si la celda (sin rotar) es parte de la sala. */
	bool ContainsCell(const FIntPoint& Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < FootprintCells.X && Cell.Y < FootprintCells.Y && !BlockedCells.Contains(Cell);
	}

	/** Celdas de la sala sin rotar, en orden estable. */
	void GetLocalCells(TArray<FIntPoint>& OutCells) const
	{
		for (int32 X = 0; X < FootprintCells.X; ++X)
		{
			for (int32 Y = 0; Y < FootprintCells.Y; ++Y)
			{
				if (ContainsCell(FIntPoint(X, Y)))
				{
					OutCells.Add(FIntPoint(X, Y));
				}
			}
		}
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

	/** Celda mínima que ocupa; Z es el piso (-1 sótano, 0 planta baja, 1 piso superior). */
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

	/** Tamaño de casa pedido. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|HouseGen")
	ENDHouseSize HouseSize = ENDHouseSize::Small;

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
	/**
	 * Puerta abierta en una pared sin hueco previsto en el módulo (para unir la segunda escalera).
	 * Su id en el manifiesto es "Auto_X_Y_D" con la celda y el lado del módulo sin girar.
	 */
	NDE_API FName MakeAutoSocketId(const FIntPoint& LocalCell, ENDCellDirection LocalDirection);
	NDE_API bool ParseAutoSocketId(FName SocketId, FIntPoint& OutLocalCell, ENDCellDirection& OutLocalDirection);
}
