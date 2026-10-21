// No Despiertes Eso

#include "UI/NDMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/NDGameInstance.h"
#include "Core/NDLog.h"
#include "Kismet/KismetSystemLibrary.h"

void UNDMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// WBP recién creado (sin raíz o con un panel vacío): disposición mínima.
	const UPanelWidget* RootPanel = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (WidgetTree && (!WidgetTree->RootWidget || (RootPanel && RootPanel->GetChildrenCount() == 0)))
	{
		BuildFallbackLayout();
	}
}

void UNDMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostSessionComplete.AddUniqueDynamic(this, &ThisClass::HandleHostComplete);
		Sessions->OnFindSessionsComplete.AddUniqueDynamic(this, &ThisClass::HandleFindComplete);
		Sessions->OnJoinSessionComplete.AddUniqueDynamic(this, &ThisClass::HandleJoinComplete);
	}

	if (UNDGameInstance* GI = GetGameInstance<UNDGameInstance>())
	{
		const ENDDisconnectReason Reason = GI->ConsumeLastDisconnectReason();
		if (Reason != ENDDisconnectReason::None && Reason != ENDDisconnectReason::LeftVoluntarily)
		{
			FString Message = UNDGameInstance::GetDisconnectReasonText(Reason).ToString();
			if (GI->CanRejoinExpedition())
			{
				Message += FString::Printf(TEXT(" Puedes volver a la expedicion durante %.0f s."), GI->GetRejoinSecondsLeft());
			}
			SetStatus(Message);
			OnSessionError(Reason);
		}
	}
}

void UNDMainMenuWidget::NativeDestruct()
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnHostSessionComplete.RemoveAll(this);
		Sessions->OnFindSessionsComplete.RemoveAll(this);
		Sessions->OnJoinSessionComplete.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UNDMainMenuWidget::HostSession(bool bFriendsOnly)
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(TEXT("Creando sala..."));
		Sessions->HostSession(bFriendsOnly);
	}
}

void UNDMainMenuWidget::FindSessions()
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(TEXT("Buscando salas..."));
		Sessions->FindSessions();
	}
}

void UNDMainMenuWidget::JoinSession(int32 SearchIndex)
{
	if (UNDSessionSubsystem* Sessions = GetSessions())
	{
		SetStatus(FString::Printf(TEXT("Entrando en la sala %d..."), SearchIndex));
		Sessions->JoinSessionByIndex(SearchIndex);
	}
}

void UNDMainMenuWidget::JoinFirstCompatibleSession()
{
	for (const FNDSessionInfo& Info : LastSessions)
	{
		if (Info.bIsCompatible)
		{
			JoinSession(Info.SearchIndex);
			return;
		}
	}
	SetStatus(TEXT("No hay salas compatibles. Pulsa Buscar salas."));
}

void UNDMainMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

bool UNDMainMenuWidget::CanRejoinExpedition() const
{
	const UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	return GI && GI->CanRejoinExpedition();
}

void UNDMainMenuWidget::RejoinExpedition()
{
	UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	if (GI && GI->RejoinExpedition())
	{
		SetStatus(TEXT("Volviendo a la expedicion..."));
		return;
	}
	SetStatus(UNDGameInstance::GetDisconnectReasonText(ENDDisconnectReason::RejoinExpired).ToString());
}

void UNDMainMenuWidget::HandleHostComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetStatus(TEXT("No se pudo crear la sala."));
		OnSessionError(ENDDisconnectReason::JoinFailed);
	}
}

void UNDMainMenuWidget::HandleFindComplete(const TArray<FNDSessionInfo>& Results, bool bWasSuccessful)
{
	LastSessions = Results;

	FString Message = FString::Printf(TEXT("Salas encontradas: %d"), Results.Num());
	for (const FNDSessionInfo& Info : Results)
	{
		Message += FString::Printf(TEXT("\n[%d] %s  %d/%d  %d ms%s"), Info.SearchIndex, *Info.HostName,
			Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs, Info.bIsCompatible ? TEXT("") : TEXT("  (incompatible)"));
	}
	SetStatus(Message);
	OnSessionsFound(Results, bWasSuccessful);
}

void UNDMainMenuWidget::HandleJoinComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetStatus(TEXT("No se pudo entrar en la sala."));
		OnSessionError(ENDDisconnectReason::JoinFailed);
	}
}

void UNDMainMenuWidget::BuildFallbackLayout()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Botones"));
	WidgetTree->RootWidget = Box;

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Titulo"));
	Title->SetText(NSLOCTEXT("NDE", "MenuTitle", "No Despiertes Eso"));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 36;
	Title->SetFont(TitleFont);
	Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(40.f, 40.f, 40.f, 20.f));

	if (CanRejoinExpedition())
	{
		AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuRejoin", "Volver a la expedicion"))->OnClicked.AddDynamic(this, &ThisClass::HandleRejoinClicked);
	}
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuHost", "Crear sala"))->OnClicked.AddDynamic(this, &ThisClass::HandleHostClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuFind", "Buscar salas"))->OnClicked.AddDynamic(this, &ThisClass::HandleFindClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuJoin", "Unirse a la primera sala"))->OnClicked.AddDynamic(this, &ThisClass::HandleJoinClicked);
	AddFallbackButton(Box, NSLOCTEXT("NDE", "MenuQuit", "Salir"))->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(40.f, 20.f));
}

UButton* UNDMainMenuWidget::AddFallbackButton(UVerticalBox* Box, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button->AddChild(Text);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(40.f, 6.f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Left);
	return Button;
}

void UNDMainMenuWidget::SetStatus(const FString& Message)
{
	UE_LOG(LogNDNet, Log, TEXT("Menu: %s"), *Message);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

UNDSessionSubsystem* UNDMainMenuWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNDSessionSubsystem>() : nullptr;
}
