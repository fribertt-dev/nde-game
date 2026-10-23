#include "UI/Loading/NDLoadingScreenSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"
#include "UI/Loading/SNDLoadingScreen.h"
#include "UI/NDUISettings.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "NDLoadingScreen"

DEFINE_LOG_CATEGORY_STATIC(LogNDLoading, Log, All);

namespace NDLoadingScreen
{
	/** Encima de todo lo demás del viewport (la raíz de UI va en 1000). */
	constexpr int32 ViewportZOrder = 20000;
	/** Si se pidió la pantalla y el viaje no empieza en este tiempo, se quita. */
	constexpr double TravelStartTimeoutSeconds = 15.0;
	constexpr float FadeInSeconds = 0.25f;

	TArray<FText> GetTips()
	{
		return {
			LOCTEXT("Tip01", "Los objetos pesados se llevan entre dos. O con el carrito, que no se queja."),
			LOCTEXT("Tip02", "Si el gigante ronca fuerte, es buen momento para cruzar el suelo que cruje."),
			LOCTEXT("Tip03", "Al amanecer tenéis 45 segundos para llegar a la camioneta. Ni uno más."),
			LOCTEXT("Tip04", "El botín en la camioneta no es victoria: alguien tiene que escapar con él."),
			LOCTEXT("Tip05", "La espuma silenciosa amortigua pasos y caídas. Los jarrones lo agradecen."),
			LOCTEXT("Tip06", "El ratón de cuerda distrae a cualquiera que lo oiga. Incluido tu compañero."),
			LOCTEXT("Tip07", "Hablar fuerte cerca del dueño es la forma más rápida de despertarlo."),
			LOCTEXT("Tip08", "La retirada se vota desde la camioneta: manda la mayoría."),
			LOCTEXT("Tip09", "Un objeto roto vale mucho menos. Y hace mucho ruido."),
			LOCTEXT("Tip10", "En la mochila caben tres cosas pequeñas. Elige bien."),
			LOCTEXT("Tip11", "Si un compañero cae en la jaula, todavía se le puede rescatar."),
			LOCTEXT("Tip12", "Los Desvelados nunca duermen. Los dueños de las casas, por suerte, sí."),
		};
	}

	FAutoConsoleCommandWithWorldAndArgs ShowLoadingCommand(
		TEXT("nde.UI.ShowLoading"),
		TEXT("Muestra la pantalla de transición unos segundos sin viajar. Uso: nde.UI.ShowLoading [segundos=4]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UNDLoadingScreenSubsystem* Loading = GameInstance ? GameInstance->GetSubsystem<UNDLoadingScreenSubsystem>() : nullptr;
			if (Loading)
			{
				Loading->PreviewTransition(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 4.f);
			}
		}));
}

bool UNDLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Un servidor dedicado no pinta nada.
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UNDLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Se cargan una vez y se quedan en memoria: durante la carga del mapa no hay tiempo de pedirlas.
	for (const TSoftObjectPtr<UTexture2D>& Image : GetDefault<UNDUISettings>()->LoadingScreenImages)
	{
		const FSoftObjectPath Path = Image.ToSoftObjectPath();
		if (Path.IsNull() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			continue;
		}
		if (UTexture2D* Texture = Image.LoadSynchronous())
		{
			Images.Add(Texture);
		}
	}
	UE_CLOG(Images.IsEmpty(), LogNDLoading, Log, TEXT("No loading screen images found (Project Settings > NDE UI > Loading Screen): using a plain background"));

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

void UNDLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	RemoveWidget();
	Super::Deinitialize();
}

UTexture2D* UNDLoadingScreenSubsystem::GetRandomImage() const
{
	return Images.IsEmpty() ? nullptr : Images[FMath::RandHelper(Images.Num())].Get();
}

FText UNDLoadingScreenSubsystem::GetHeadlineForMap(const FString& MapName)
{
	if (MapName.Contains(TEXT("Madriguera")))
	{
		return LOCTEXT("ToHideout", "Volviendo a La Madriguera...");
	}
	if (MapName.Contains(TEXT("Casa")) || MapName.Contains(TEXT("Expedicion")))
	{
		return LOCTEXT("ToHouse", "Rumbo a la casa...");
	}
	if (MapName.Contains(TEXT("Menu")))
	{
		return LOCTEXT("ToMenu", "Volviendo al menú...");
	}
	return LOCTEXT("Loading", "Cargando...");
}

void UNDLoadingScreenSubsystem::ShowTransition(const FText& Headline)
{
	bWaitingForTravel = !bLoading;
	bWaitingForPlayer = false;
	MinSecondsThisTime = GetDefault<UNDUISettings>()->LoadingScreenMinSeconds;
	ShowWidget(Headline);
}

void UNDLoadingScreenSubsystem::HideTransition()
{
	bWaitingForTravel = false;
	bWaitingForPlayer = false;
	BeginHide();
}

void UNDLoadingScreenSubsystem::PreviewTransition(float Seconds)
{
	ShowWidget(LOCTEXT("Preview", "Rumbo a la casa..."));
	bWaitingForTravel = false;
	bLoading = false;
	bWaitingForPlayer = true;
	MinSecondsThisTime = FMath::Max(0.5f, Seconds);
	LoadFinishedAt = FPlatformTime::Seconds();
}

void UNDLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}
	// El seamless travel también avisa aquí en algunas versiones: ya lo cubre HandleSeamlessTravelStart.
	const bool bSeamless = WorldContext.SeamlessTravelHandler.IsInTransition();
	UE_LOG(LogNDLoading, Log, TEXT("PreLoadMap %s (seamless %d)"), *MapName, bSeamless ? 1 : 0);

	const FText Headline = GetHeadlineForMap(MapName);
	bLoading = true;
	bWaitingForTravel = false;
	bWaitingForPlayer = false;
	if (!Widget.IsValid())
	{
		MinSecondsThisTime = GetDefault<UNDUISettings>()->LoadingScreenMinSeconds;
	}
	ShowWidget(Headline);

	// La carga bloquea el hilo del juego: el viewport no se repinta, así que la pinta el MoviePlayer.
	// En el editor (PIE) no hay MoviePlayer y se ve la pantalla del viewport al terminar.
	if (!bSeamless && !GIsEditor && IsMoviePlayerEnabled() && GetMoviePlayer())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.bWaitForManualStop = false;
		Attributes.WidgetLoadingScreen = MakeScreen(Headline);
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
}

void UNDLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& MapName)
{
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UE_LOG(LogNDLoading, Log, TEXT("Seamless travel to %s"), *MapName);
	bLoading = true;
	bWaitingForTravel = false;
	bWaitingForPlayer = false;
	if (!Widget.IsValid())
	{
		MinSecondsThisTime = GetDefault<UNDUISettings>()->LoadingScreenMinSeconds;
	}
	ShowWidget(GetHeadlineForMap(MapName));
}

void UNDLoadingScreenSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || !Widget.IsValid())
	{
		return;
	}
	// En el seamless travel esto llega también con el mapa de transición; el de destino reinicia la espera.
	bLoading = false;
	bWaitingForPlayer = true;
	LoadFinishedAt = FPlatformTime::Seconds();
	// LoadMap quita todo lo que había en el viewport: se vuelve a poner encima.
	AttachToViewport();
}

bool UNDLoadingScreenSubsystem::Tick(float DeltaTime)
{
	if (!Widget.IsValid())
	{
		return true;
	}
	const UNDUISettings* Settings = GetDefault<UNDUISettings>();
	const double Now = FPlatformTime::Seconds();

	if (bWaitingForTravel && Now - ShownAt > NDLoadingScreen::TravelStartTimeoutSeconds)
	{
		UE_LOG(LogNDLoading, Log, TEXT("No travel started after ShowTransition: hiding the loading screen"));
		HideTransition();
	}
	else if (bWaitingForPlayer && TargetOpacity > 0.f)
	{
		const bool bPlayerReady = IsLocalPlayerReady() || Now - LoadFinishedAt > Settings->LoadingScreenMaxHoldSeconds;
		if (bPlayerReady && Now - ShownAt >= MinSecondsThisTime)
		{
			bWaitingForPlayer = false;
			BeginHide();
		}
	}

	// Tras una carga larga DeltaTime es enorme: el fundido de entrada se completa de golpe.
	const float FadeSeconds = TargetOpacity > Opacity ? NDLoadingScreen::FadeInSeconds : FMath::Max(0.05f, Settings->LoadingScreenFadeSeconds);
	Opacity = FMath::FInterpConstantTo(Opacity, TargetOpacity, FMath::Min(DeltaTime, 0.1f), 1.f / FadeSeconds);
	Widget->SetRenderOpacity(Opacity);
	if (TargetOpacity <= 0.f && Opacity <= KINDA_SMALL_NUMBER)
	{
		RemoveWidget();
	}
	return true;
}

void UNDLoadingScreenSubsystem::ShowWidget(const FText& Headline)
{
	TargetOpacity = 1.f;
	if (Widget.IsValid())
	{
		Widget->SetHeadline(Headline);
		AttachToViewport();
		return;
	}
	ShownAt = FPlatformTime::Seconds();
	CurrentImage = GetRandomImage();
	const TArray<FText> Tips = NDLoadingScreen::GetTips();
	CurrentTip = Tips[FMath::RandHelper(Tips.Num())];
	Opacity = 0.f;
	Widget = MakeScreen(Headline);
	Widget->SetRenderOpacity(Opacity);
	AttachToViewport();
}

void UNDLoadingScreenSubsystem::AttachToViewport()
{
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!Viewport || !Widget.IsValid())
	{
		return;
	}
	TSharedRef<SWidget> Content = Widget.ToSharedRef();
	Viewport->RemoveViewportWidgetContent(Content);
	Viewport->AddViewportWidgetContent(Content, NDLoadingScreen::ViewportZOrder);
}

void UNDLoadingScreenSubsystem::RemoveWidget()
{
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	Widget.Reset();
	CurrentImage = nullptr;
	Opacity = 0.f;
	TargetOpacity = 0.f;
}

void UNDLoadingScreenSubsystem::BeginHide()
{
	TargetOpacity = 0.f;
}

bool UNDLoadingScreenSubsystem::IsLocalPlayerReady() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	// El menú usa un espectador; la casa da el personaje cuando ya está construida.
	return PlayerController && PlayerController->HasActorBegunPlay() && PlayerController->GetPawnOrSpectator() != nullptr;
}

TSharedRef<SNDLoadingScreen> UNDLoadingScreenSubsystem::MakeScreen(const FText& Headline) const
{
	return SNew(SNDLoadingScreen)
		.Image(CurrentImage.Get())
		.Headline(Headline)
		.Tip(CurrentTip);
}

#undef LOCTEXT_NAMESPACE
