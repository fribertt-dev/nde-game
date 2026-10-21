#pragma once

#include "CoreMinimal.h"
#include "NDUITypes.generated.h"

class UNDUIBaseScreen;

// Capas de la raíz de UI, de abajo arriba. Cada capa es una pila de Common UI.
UENUM(BlueprintType)
enum class ENDUILayer : uint8
{
	Game,		// HUD de misión y avisos que no capturan el foco
	GameMenu,	// pausa, mapa, rueda de gestos: menús sobre la partida en curso
	Menu,		// pantallas completas de menú, guarida, tienda, resultados
	Modal,		// confirmaciones, errores y conexión perdida
};

UENUM(BlueprintType)
enum class ENDUIInputMode : uint8
{
	Default,	// no cambia el modo de entrada al activarse
	Game,
	GameAndMenu,
	Menu,
};

USTRUCT(BlueprintType)
struct FNDUIScreenEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftClassPtr<UNDUIBaseScreen> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	ENDUILayer Layer = ENDUILayer::Menu;
};

USTRUCT(BlueprintType)
struct FNDUIToast
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText Message;

	// Mayor prioridad se muestra antes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (ClampMin = "0.5"))
	float DurationSeconds = 4.f;
};
