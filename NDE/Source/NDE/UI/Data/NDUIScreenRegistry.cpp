#include "UI/Data/NDUIScreenRegistry.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "NDUIScreenRegistry"

#if WITH_EDITOR
EDataValidationResult UNDUIScreenRegistry::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const TPair<FGameplayTag, FNDUIScreenEntry>& Pair : Screens)
	{
		if (!Pair.Key.IsValid())
		{
			Context.AddError(LOCTEXT("InvalidTag", "Hay una entrada sin etiqueta de pantalla"));
			Result = EDataValidationResult::Invalid;
		}
		else if (Pair.Value.WidgetClass.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingWidget", "La pantalla {0} no tiene WBP asignado"), FText::FromName(Pair.Key.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
