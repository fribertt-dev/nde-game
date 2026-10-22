#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/NDUIRootLayout.h"
#include "NDUINativeRootLayout.generated.h"

/**
 * Raíz por capas construida en C++: el Overlay con las cuatro pilas (Game, GameMenu, Menu, Modal).
 * Es la raíz por defecto mientras no exista WBP_RootLayout; un WBP hijo de UNDUIRootLayout la sustituye
 * desde Project Settings > Game > NDE UI sin tocar código.
 */
UCLASS()
class NDE_API UNDUINativeRootLayout : public UNDUIRootLayout
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
};
