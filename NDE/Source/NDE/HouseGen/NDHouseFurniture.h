// No Despiertes Eso
// Muebles de arte de la casa: qué malla, de qué tamaño y en qué salas va cada uno.

#pragma once

#include "CoreMinimal.h"
#include "HouseGen/NDHouseGenTypes.h"

/** Dónde se pone un mueble dentro de la sala. */
enum class ENDFurniturePlacement : uint8
{
	/** Con el respaldo pegado a una pared (las esquinas cuentan). */
	Wall,
	/** En el centro de la sala (mesas); solo en salas de 2 celdas o más. */
	Center,
	/** En el suelo, sin colisión (alfombras): no tapa el paso. */
	Flat
};

/** Pieza que acompaña a un mueble: sillas de la mesa (con colisión) o adornos encima (sin colisión). */
struct FNDFurnitureCompanion
{
	FName Id;
	/** Relativo al mueble: X a lo largo, Y hacia el respaldo (la pared), Z hacia arriba. */
	FVector Offset = FVector::ZeroVector;
	float Yaw = 0.f;
	/** Va encima o colgado: sin colisión y sin comprobar el paso. */
	bool bDecor = true;
};

/**
 * Mueble de arte. Size es su tamaño en cm en el marco del mueble: X a lo largo de la pared, Y de fondo, Z de alto.
 * En ese marco el frente mira a -Y (hacia la sala) y el respaldo a +Y (contra la pared). Las mallas tienen el pivote
 * abajo en el centro; MeshYaw las gira dentro del marco para que su frente quede en -Y.
 */
struct FNDFurnitureArt
{
	FName Id;
	FString MeshPath;
	FVector Size = FVector(100.f);
	ENDDecorationKind Kind = ENDDecorationKind::Appliance;
	/**
	 * Giro de la malla para que su frente mire a -Y. Las mallas importadas de Blender tienen el frente hacia +Y:
	 * casi todas llevan 180. 90 = de lado (la cama con el lado largo contra la pared, la silla con el frente en -X).
	 */
	float MeshYaw = 180.f;
	/** Escala de la malla (alfombras más pequeñas y planas); Size ya la incluye. */
	FVector MeshScale = FVector::OneVector;
	TArray<FNDFurnitureCompanion> Companions;
};

/** Mueble que puede ir en un tipo de sala, en orden de prioridad. */
struct FNDRoomFurniture
{
	FName Id;
	ENDFurniturePlacement Placement = ENDFurniturePlacement::Wall;
	/** Cuántos como mucho en la misma sala. */
	int32 MaxCount = 1;
	/** Probabilidad de intentarlo (1 = siempre). */
	float Chance = 1.f;
};

namespace NDHouseFurniture
{
	/** Todos los muebles de arte; el índice es estable y lo guarda cada FNDHouseDecoration. */
	NDE_API const TArray<FNDFurnitureArt>& GetArt();
	NDE_API int32 FindArt(FName Id);
	/** Muebles de cada tipo de sala, en orden de prioridad (los primeros son los importantes). */
	NDE_API const TArray<FNDRoomFurniture>& GetRoomFurniture(ENDRoomType RoomType);
	/** La cama de la criatura en el dormitorio principal. */
	NDE_API FName GetCreatureBedId();
}

class UStaticMesh;

/** Mallas de arte de la casa (paredes, suelos, muebles), encima de la colisión greybox, que queda invisible. */
namespace NDHouseArt
{
	/** nde.HouseGen.Art: 1 = mallas de arte, 0 = solo greybox (depuración). */
	NDE_API bool IsEnabled();
	/** Carga una malla por ruta; nullptr si no existe (entonces se ve el greybox). */
	NDE_API UStaticMesh* LoadMesh(const FString& Path);
	/** Ruta de una pieza de Props/Structure, p. ej. "Wall_400". */
	NDE_API FString GetStructurePath(const TCHAR* Name);
}
