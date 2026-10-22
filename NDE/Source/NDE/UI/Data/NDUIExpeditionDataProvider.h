#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Expedition/NDExpeditionTypes.h"
#include "UI/Data/NDUIMockDataProvider.h"
#include "NDUIExpeditionDataProvider.generated.h"

class ANDExpeditionDirector;
class UWorld;

/**
 * Proveedor de la UI con datos reales de la expedición: reloj nocturno, cuota, botín asegurado
 * y resultado salen del ANDExpeditionDirector del mapa (replicado, vale en servidor y clientes).
 * Fuera de una expedición, y para el resto de temas (lobby, tienda...), sirve los datos simulados.
 * Avisa con toasts del pre-amanecer, el amanecer y el resultado.
 */
UCLASS(Blueprintable)
class NDE_API UNDUIExpeditionDataProvider : public UNDUIMockDataProvider
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

	virtual bool IsSimulated_Implementation() const override;
	virtual FNDNightClockVM GetNightClock_Implementation() const override;
	virtual FNDResultsVM GetLastResults_Implementation() const override;

	/** Fase del reloj de la UI a partir de las fases de reglas. */
	static ENDUINightPhase ToUIPhase(ENDExpeditionPhase ExpeditionPhase, ENDNightPhase NightPhase);

	/** Resultado de la pantalla de resultados a partir del resumen de la expedición. */
	static ENDUIRunOutcome ToUIOutcome(const FNDRunSummary& Summary);

protected:
	UWorld* GetPlayerWorld() const;
	ANDExpeditionDirector* GetDirector() const;

	/** Engancha los eventos del director del mapa actual (cambia al viajar). */
	bool PollDirector(float DeltaTime);

	void ShowToast(const FText& Message, int32 Priority) const;

	UFUNCTION()
	void HandleExpeditionStateChanged();

	UFUNCTION()
	void HandleNightPhaseChanged(ENDNightPhase NewPhase, ENDNightPhase OldPhase);

	UFUNCTION()
	void HandlePreDawnWarning(int32 SecondsUntilDawn);

	UFUNCTION()
	void HandleExpeditionSettled(const FNDRunSummary& Summary);

	TWeakObjectPtr<ANDExpeditionDirector> BoundDirector;
	FTSTicker::FDelegateHandle PollHandle;
};
