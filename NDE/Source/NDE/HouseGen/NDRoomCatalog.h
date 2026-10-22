// No Despiertes Eso

#pragma once

#include "CoreMinimal.h"
#include "HouseGen/NDHouseGenTypes.h"

/**
 * Catálogo de módulos de sala. De momento son definiciones greybox escritas en C++
 * para D01/H01; más adelante se cargarán UNDRoomModuleData con su Level Instance de arte.
 * Mientras el RoomModuleId no cambie, los manifiestos antiguos siguen valiendo.
 */
struct NDE_API FNDRoomCatalog
{
	/** Todos los módulos disponibles. */
	static const TArray<FNDRoomModuleDefinition>& GetModules();

	/** nullptr si no existe. */
	static const FNDRoomModuleDefinition* Find(FName RoomModuleId);

	/** Módulos de un tipo válidos en un distrito, en orden estable. */
	static void GetModulesOfType(ENDRoomType Type, FName District, TArray<const FNDRoomModuleDefinition*>& OutModules);
};
