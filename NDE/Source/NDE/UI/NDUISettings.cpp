#include "UI/NDUISettings.h"

#include "UI/Data/NDUIExpeditionDataProvider.h"

UNDUISettings::UNDUISettings()
{
	DataProviderClass = UNDUIExpeditionDataProvider::StaticClass();
}
