// No Despiertes Eso

#include "UI/NDLobbyStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Networking/NDLobbyGameState.h"
#include "Players/NDPlayerState.h"

void UNDLobbyStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// WBP recién creado (sin raíz o con un panel vacío): disposición mínima.
	const UPanelWidget* RootPanel = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (WidgetTree && (!WidgetTree->RootWidget || (RootPanel && RootPanel->GetChildrenCount() == 0)))
	{
		BuildFallbackLayout();
	}
}

float UNDLobbyStatusWidget::GetSecondsUntilTravel() const
{
	const ANDLobbyGameState* LobbyState = GetLobbyState();
	if (!LobbyState || LobbyState->GetPhase() != ENDLobbyPhase::ReadyCheck)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(LobbyState->GetTravelServerTime() - LobbyState->GetServerWorldTimeSeconds()));
}

void UNDLobbyStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ANDLobbyGameState* LobbyState = GetLobbyState();
	if (!LobbyState)
	{
		return;
	}

	const int32 Ready = LobbyState->GetNumReadyPlayers();
	const int32 Total = LobbyState->PlayerArray.Num();
	if (Ready != LastReady || Total != LastTotal)
	{
		LastReady = Ready;
		LastTotal = Total;
		OnReadyCountChanged(Ready, Total);
	}

	const ENDLobbyPhase Phase = LobbyState->GetPhase();
	if (Phase != LastPhase)
	{
		if (Phase == ENDLobbyPhase::ReadyCheck)
		{
			OnCountdownStarted(LobbyState->GetTravelServerTime());
		}
		else if (Phase == ENDLobbyPhase::Traveling)
		{
			OnTraveling();
		}
		else if (LastPhase == ENDLobbyPhase::ReadyCheck)
		{
			OnCountdownCancelled();
		}
		LastPhase = Phase;
	}

	UpdateTexts(*LobbyState);
}

void UNDLobbyStatusWidget::UpdateTexts(const ANDLobbyGameState& LobbyState)
{
	if (ReadyText)
	{
		FString Lines = FString::Printf(TEXT("La Madriguera - listos %d/%d"), LastReady, LastTotal);
		for (const APlayerState* PS : LobbyState.PlayerArray)
		{
			const ANDPlayerState* NDPS = Cast<ANDPlayerState>(PS);
			Lines += FString::Printf(TEXT("\n  %s  %s"), *PS->GetPlayerName(), (NDPS && NDPS->IsReady()) ? TEXT("LISTO") : TEXT("..."));
		}
		ReadyText->SetText(FText::FromString(Lines));
	}

	if (CountdownText)
	{
		FString Message;
		switch (LastPhase)
		{
		case ENDLobbyPhase::ReadyCheck:
			Message = FString::Printf(TEXT("Saliendo de expedicion en %d..."), FMath::CeilToInt(GetSecondsUntilTravel()));
			break;
		case ENDLobbyPhase::Traveling:
			Message = TEXT("Viajando a la expedicion...");
			break;
		default:
			Message = TEXT("Entrad todos en la zona verde de SALIDA");
			break;
		}
		CountdownText->SetText(FText::FromString(Message));
	}
}

const ANDLobbyGameState* UNDLobbyStatusWidget::GetLobbyState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ANDLobbyGameState>() : nullptr;
}

void UNDLobbyStatusWidget::BuildFallbackLayout()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Estado"));
	WidgetTree->RootWidget = Box;

	ReadyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReadyText"));
	Box->AddChildToVerticalBox(ReadyText)->SetPadding(FMargin(30.f, 30.f, 30.f, 10.f));

	CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountdownText"));
	FSlateFontInfo Font = CountdownText->GetFont();
	Font.Size = 32;
	CountdownText->SetFont(Font);
	Box->AddChildToVerticalBox(CountdownText)->SetPadding(FMargin(30.f, 10.f));
}
