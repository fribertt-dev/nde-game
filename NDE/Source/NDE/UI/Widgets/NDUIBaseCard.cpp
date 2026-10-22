#include "UI/Widgets/NDUIBaseCard.h"

void UNDUIBaseCard::SetSelected(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		BP_OnSelectedChanged(bSelected);
	}
}
