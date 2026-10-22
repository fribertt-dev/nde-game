#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Data/NDUIViewModels.h"
#include "NDUIBaseCard.generated.h"

/**
 * Tarjeta reutilizable para contrato, herramienta, jugador o recompensa (UNDUIBaseCard, GDD 32.3).
 * El WBP hijo implementa el BP_Apply* que le corresponda y su aspecto de seleccionado.
 */
UCLASS(Abstract, Blueprintable)
class NDE_API UNDUIBaseCard : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetContract(const FNDContractCardVM& InContract) { BP_ApplyContract(InContract); }

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetShopItem(const FNDShopItemVM& InItem) { BP_ApplyShopItem(InItem); }

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetPlayer(const FNDPlayerCardVM& InPlayer) { BP_ApplyPlayer(InPlayer); }

	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	bool IsSelected() const { return bSelected; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_ApplyContract(const FNDContractCardVM& Contract);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_ApplyShopItem(const FNDShopItemVM& Item);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_ApplyPlayer(const FNDPlayerCardVM& Player);

	UFUNCTION(BlueprintImplementableEvent, Category = "NDE|UI")
	void BP_OnSelectedChanged(bool bIsSelected);

private:
	bool bSelected = false;
};
