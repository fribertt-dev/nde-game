#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NDLoadingScreenSubsystem.generated.h"

class SNDLoadingScreen;
class UTexture2D;
class UWorld;
struct FWorldContext;

/**
 * Pantalla de transición entre mapas (UI.Screen.Loading, GDD 32.2: Ready -> Loading).
 * Sale sola en cada viaje: menú -> La Madriguera, La Madriguera -> casa y vuelta.
 * - Viaje con carga que bloquea (OpenLevel, ClientTravel): la pinta el MoviePlayer, fuera del editor.
 * - Viaje sin cortes (ServerTravel con seamless travel): va encima del viewport.
 * Al terminar la carga sigue tapando hasta que el jugador local tiene personaje (la casa se
 * genera antes de dar pawn) o pasa MaxHoldSeconds, y luego se desvanece.
 * Imágenes y tiempos: Project Settings > Game > NDE UI > Loading Screen.
 */
UCLASS()
class NDE_API UNDLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Tapa la pantalla ya, antes de que empiece el viaje (p. ej. al pulsar Crear sala). Si el viaje no llega, se quita sola. */
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void ShowTransition(const FText& Headline);

	/** Quita la pantalla con un fundido (p. ej. si falló crear la sala). */
	UFUNCTION(BlueprintCallable, Category = "NDE|UI")
	void HideTransition();

	/** Muestra la pantalla unos segundos, para revisarla sin viajar (nde.UI.ShowLoading). */
	void PreviewTransition(float Seconds);

	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	bool IsTransitionVisible() const { return Widget.IsValid(); }

	/** Una de las imágenes de transición al azar; nullptr si no hay ninguna. También sirve de fondo del menú. */
	UFUNCTION(BlueprintPure, Category = "NDE|UI")
	UTexture2D* GetRandomImage() const;

	/** Texto del destino según el mapa: "Rumbo a la casa...", "Volviendo a La Madriguera...". */
	static FText GetHeadlineForMap(const FString& MapName);

private:
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandleSeamlessTravelStart(UWorld* World, const FString& MapName);
	void HandlePostLoadMap(UWorld* World);
	bool Tick(float DeltaTime);

	/** Crea el widget si falta y lo pone encima del viewport. */
	void ShowWidget(const FText& Headline);
	void AttachToViewport();
	void RemoveWidget();
	void BeginHide();
	bool IsLocalPlayerReady() const;
	TSharedRef<SNDLoadingScreen> MakeScreen(const FText& Headline) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Images;

	/** Imagen de esta transición, la misma en el MoviePlayer y en el viewport. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentImage;

	TSharedPtr<SNDLoadingScreen> Widget;
	FText CurrentTip;

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle SeamlessTravelHandle;

	double ShownAt = 0.0;
	double LoadFinishedAt = 0.0;
	float Opacity = 0.f;
	float TargetOpacity = 0.f;
	float MinSecondsThisTime = 0.f;
	/** ShowTransition sin viaje todavía. */
	bool bWaitingForTravel = false;
	/** Entre PreLoadMap (o inicio del seamless travel) y PostLoadMap. */
	bool bLoading = false;
	/** Carga terminada: esperando al personaje del jugador local. */
	bool bWaitingForPlayer = false;
};
