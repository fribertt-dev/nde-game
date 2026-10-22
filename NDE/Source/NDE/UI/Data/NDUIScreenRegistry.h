#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UI/NDUITypes.h"
#include "NDUIScreenRegistry.generated.h"

// DA_UIScreenRegistry: etiqueta de pantalla -> WBP y capa. Cambiar un WBP no requiere tocar código.
UCLASS(BlueprintType)
class NDE_API UNDUIScreenRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Screen", ForceInlineRow))
	TMap<FGameplayTag, FNDUIScreenEntry> Screens;

	const FNDUIScreenEntry* FindScreen(FGameplayTag ScreenTag) const { return Screens.Find(ScreenTag); }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
