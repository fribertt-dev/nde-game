// No Despiertes Eso

#include "UI/NDLobbyStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Networking/NDLobbyGameState.h"
#include "Networking/NDVoiceComponent.h"
#include "Players/NDPlayerController.h"
#include "Players/NDPlayerState.h"
#include "Progression/NDProfileSubsystem.h"
#include "UI/Screens/NDUINativeStyle.h"

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

	uint32 VotesHash = 0;
	for (int32 Index = 0; Index < LobbyState->GetContractOptions().Num(); ++Index)
	{
		VotesHash = HashCombine(VotesHash, GetTypeHash(LobbyState->GetVoteCount(Index)));
	}
	VotesHash = HashCombine(VotesHash, GetTypeHash(GetLocalVote()));
	if (VotesHash != LastVotesHash)
	{
		LastVotesHash = VotesHash;
		OnVotesChanged();
	}

	UpdateTexts(*LobbyState);
}

int32 UNDLobbyStatusWidget::GetLocalVote() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ANDPlayerState* PS = PC ? PC->GetPlayerState<ANDPlayerState>() : nullptr;
	return PS ? PS->GetVotedContractIndex() : INDEX_NONE;
}

void UNDLobbyStatusWidget::UpdateTexts(const ANDLobbyGameState& LobbyState)
{
	if (ReadyText)
	{
		FString Lines = FString::Printf(TEXT("La banda: %d de %d listos"), LastReady, LastTotal);
		for (const APlayerState* PS : LobbyState.PlayerArray)
		{
			const ANDPlayerState* NDPS = Cast<ANDPlayerState>(PS);
			Lines += FString::Printf(TEXT("\n   %s  %s"), (NDPS && NDPS->IsReady()) ? TEXT("[listo]") : TEXT("[ ... ]"), *PS->GetPlayerName());
		}
		ReadyText->SetText(FText::FromString(Lines));
	}

	if (CountdownText)
	{
		FString Message;
		switch (LastPhase)
		{
		case ENDLobbyPhase::ReadyCheck:
			Message = FString::Printf(TEXT("Salida hacia la casa en %d..."), FMath::CeilToInt(GetSecondsUntilTravel()));
			break;
		case ENDLobbyPhase::Traveling:
			Message = TEXT("Rumbo a la casa...");
			break;
		default:
			Message = TEXT("Entrad todos en la zona verde de SALIDA");
			break;
		}
		CountdownText->SetText(FText::FromString(Message));
	}

	if (ContractText)
	{
		const TArray<FNDContractOption>& Options = LobbyState.GetContractOptions();
		FString Lines = TEXT("Contrato de esta noche (1, 2 o 3 para votar):");
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const int32 Votes = LobbyState.GetVoteCount(Index);
			Lines += FString::Printf(TEXT("\n   %d.  %s   %d %s%s"), Index + 1, *Options[Index].DisplayName.ToString(),
				Votes, Votes == 1 ? TEXT("voto") : TEXT("votos"), GetLocalVote() == Index ? TEXT("   <- tu voto") : TEXT(""));
		}
		ContractText->SetText(FText::FromString(Lines));
	}

	if (ProfileText)
	{
		const UNDProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNDProfileSubsystem>() : nullptr;
		const ANDPlayerController* PC = Cast<ANDPlayerController>(GetOwningPlayer());
		const bool bTalking = PC && PC->GetVoiceComponent() && PC->GetVoiceComponent()->IsTransmitting();
		ProfileText->SetText(FText::FromString(FString::Printf(TEXT("Monedas %lld   Reputación %d   Expediciones %d%s"),
			Profile ? Profile->GetCoins() : 0, Profile ? Profile->GetReputation() : 0, Profile ? Profile->GetExpeditionsCompleted() : 0,
			bTalking ? TEXT("\nHablando...") : TEXT(""))));
	}
}

const ANDLobbyGameState* UNDLobbyStatusWidget::GetLobbyState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ANDLobbyGameState>() : nullptr;
}

void UNDLobbyStatusWidget::BuildFallbackLayout()
{
	using namespace NDUINativeStyle;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Panel arriba a la izquierda: deja ver la Madriguera y la zona de salida.
	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	PanelBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 10.f));
	PanelBorder->SetBrushColor(Panel);
	PanelBorder->SetPadding(FMargin(28.f, 22.f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelBorder);
	PanelSlot->SetHorizontalAlignment(HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Top);
	PanelSlot->SetPadding(FMargin(32.f));

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Estado"));
	PanelBorder->SetContent(Box);
	auto AddRow = [Box](UWidget* Widget, float Top)
	{
		Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};

	AddRow(MakeText(*WidgetTree, TEXT("Header"), NSLOCTEXT("NDE", "LobbyHeader", "La Madriguera"), 30, Amber, TEXT("Bold")), 0.f);
	CountdownText = MakeText(*WidgetTree, TEXT("CountdownText"), FText::GetEmpty(), 22, Text, TEXT("Bold"));
	AddRow(CountdownText, 6.f);
	ReadyText = MakeText(*WidgetTree, TEXT("ReadyText"), FText::GetEmpty(), 16, Text);
	AddRow(ReadyText, 16.f);
	ContractText = MakeText(*WidgetTree, TEXT("ContractText"), FText::GetEmpty(), 16, Text);
	AddRow(ContractText, 16.f);
	ProfileText = MakeText(*WidgetTree, TEXT("ProfileText"), FText::GetEmpty(), 14, Muted);
	AddRow(ProfileText, 16.f);

	// Teclas, abajo en el centro.
	UTextBlock* Hint = MakeText(*WidgetTree, TEXT("Hint"),
		NSLOCTEXT("NDE", "LobbyHint", "1 / 2 / 3  votar contrato     V  hablar     P  menú     Zona verde  listo"), 15, Muted);
	UOverlaySlot* HintSlot = Root->AddChildToOverlay(Hint);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetVerticalAlignment(VAlign_Bottom);
	HintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
}
