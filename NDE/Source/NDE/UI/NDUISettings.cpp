#include "UI/NDUISettings.h"

#include "UI/Data/NDUIExpeditionDataProvider.h"
#include "UI/Screens/NDUINativeRootLayout.h"

UNDUISettings::UNDUISettings()
{
	DataProviderClass = UNDUIExpeditionDataProvider::StaticClass();
	// Raíz hecha en C++ hasta tener WBP_RootLayout.
	RootLayoutClass = UNDUINativeRootLayout::StaticClass();
}
