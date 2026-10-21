// Actor del nivel que aloja el reloj y las reglas de la expedición.
// Permite probar el bucle sin depender del GameMode; al integrar con la red,
// sus componentes pueden pasar a ANDExpeditionGameState.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/NDTypes.h"
#include "Expedition/NDExpeditionTypes.h"
#include "NDExpeditionDirector.generated.h"

class UBoxComponent;
class UNDNightClockComponent;
class UNDExpeditionComponent;
class ADirectionalLight;

UCLASS(Blueprintable)
class NDE_API ANDExpeditionDirector : public AActor
{
	GENERATED_BODY()

public:
	ANDExpeditionDirector();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<UNDNightClockComponent> NightClock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<UNDExpeditionComponent> Expedition;

	/** Entrada de la casa: el primer jugador que la cruza arranca el reloj (GDD 7.1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<UBoxComponent> HouseEntry;

	/** Luz principal que se mueve con la noche. Solo visual y opcional (GDD 7.6). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NDE|Expedition")
	TObjectPtr<ADirectionalLight> SkyLightSource;

	/** Inclinación de la luz en noche profunda y al amanecer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	float NightLightPitch = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	float DawnLightPitch = -12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	float NightLightIntensity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	float DawnLightIntensity = 6.f;

	/** Segundos mostrando el resultado antes de pedir la vuelta a La Madriguera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NDE|Expedition")
	float ResultsDisplaySeconds = 4.f;

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition", meta = (WorldContext = "WorldContextObject"))
	static ANDExpeditionDirector* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition", meta = (WorldContext = "WorldContextObject"))
	static UNDExpeditionComponent* GetExpedition(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "NDE|Expedition", meta = (WorldContext = "WorldContextObject"))
	static UNDNightClockComponent* GetNightClock(const UObject* WorldContextObject);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UFUNCTION()
	void HandleHouseEntryOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void UpdateSkyVisuals();

	/** Con ANDExpeditionGameState la sesión de red manda: arranca en InProgress. */
	UFUNCTION()
	void HandleSessionPhaseChanged(ENDExpeditionSessionPhase NewPhase);

	UFUNCTION()
	void HandleExpeditionSettled(const FNDRunSummary& Summary);

	void RequestReturnToLobby();

	FTimerHandle ReturnTimer;
};
