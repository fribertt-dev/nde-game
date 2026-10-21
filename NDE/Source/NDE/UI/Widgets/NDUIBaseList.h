#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/NDUIBaseScreen.h"
#include "NDUIBaseList.generated.h"

/**
 * Pantalla con colección filtrable: tablón, tienda, inventario, trofeos (UNDUIBaseList, GDD 32.3).
 * El WBP construye las entradas en BP_RebuildList y llama a SetIsEmpty con el resultado.
 */
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIBaseList : public UNDUIBaseScreen
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetFilterText(const FText& InFilterText);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	FText GetFilterText() const { return FilterText; }

	// Coincidencia sin distinguir mayúsculas. Filtro vacío deja pasar todo.
	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	bool PassesFilter(const FText& Candidate) const;

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetIsEmpty(bool bIsEmpty);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_RebuildList();

	// Se muestra cuando no hay entradas que mostrar.
	UPROPERTY(BlueprintReadOnly, Category = "List", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyState;

private:
	FText FilterText;
};
