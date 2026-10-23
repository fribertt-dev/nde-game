#include "UI/NDUISettings.h"

#include "Engine/Texture2D.h"
#include "UI/Data/NDUIExpeditionDataProvider.h"
#include "UI/Screens/NDUINativeRootLayout.h"

UNDUISettings::UNDUISettings()
{
	DataProviderClass = UNDUIExpeditionDataProvider::StaticClass();
	// Raíz hecha en C++ hasta tener WBP_RootLayout.
	RootLayoutClass = UNDUINativeRootLayout::StaticClass();
	// Imágenes de transición importadas en Content/_NDE_/UI/Textures. Si faltan, fondo liso.
	for (const TCHAR* Name : { TEXT("T_UI_Transicion_01"), TEXT("T_UI_Transicion_02"), TEXT("T_UI_Transicion_03") })
	{
		LoadingScreenImages.Add(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/_NDE_/UI/Textures/%s.%s"), Name, Name))));
	}
}
