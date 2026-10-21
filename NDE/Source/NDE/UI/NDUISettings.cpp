#include "UI/NDUISettings.h"

#include "UI/Data/NDUIMockDataProvider.h"

UNDUISettings::UNDUISettings()
{
	DataProviderClass = UNDUIMockDataProvider::StaticClass();
}
