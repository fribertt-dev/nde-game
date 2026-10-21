#include "UI/Widgets/NDUIBaseList.h"

#include "Components/Widget.h"

void UNDUIBaseList::SetFilterText(const FText& InFilterText)
{
	FilterText = InFilterText;
	BP_RebuildList();
}

bool UNDUIBaseList::PassesFilter(const FText& Candidate) const
{
	const FString Filter = FilterText.ToString().TrimStartAndEnd();
	return Filter.IsEmpty() || Candidate.ToString().Contains(Filter, ESearchCase::IgnoreCase);
}

void UNDUIBaseList::SetIsEmpty(bool bIsEmpty)
{
	if (EmptyState)
	{
		EmptyState->SetVisibility(bIsEmpty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
