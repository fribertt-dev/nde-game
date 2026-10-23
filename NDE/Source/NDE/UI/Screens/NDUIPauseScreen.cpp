#include "UI/Screens/NDUIPauseScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/NDGameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Networking/NDExpeditionGameMode.h"
#include "Networking/NDExpeditionGameState.h"
#include "UI/Loading/NDLoadingScreenSubsystem.h"
#include "UI/Screens/NDUINativeStyle.h"

#define LOCTEXT_NAMESPACE "NDUIPauseScreen"

UNDUIPauseScreen::UNDUIPauseScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = ENDUIInputMode::Menu;
	bIsBackHandler = true;
	SetIsFocusable(true);
}

bool UNDUIPauseScreen::IsPauseKey(const FKey& Key)
{
	return Key == EKeys::P || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

bool UNDUIPauseScreen::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return true;
	}
	using namespace NDUINativeStyle;
	UWidgetTree& Tree = *WidgetTree;
	UOverlay* Root = Tree.ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	Tree.RootWidget = Root;

	// Oscurece la partida, que sigue detrás.
	UBorder* Dim = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* PanelBorder = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	PanelBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 12.f));
	PanelBorder->SetBrushColor(Panel);
	PanelBorder->SetPadding(FMargin(40.f, 32.f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelBorder);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	USizeBox* Width = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
	Width->SetWidthOverride(520.f);
	PanelBorder->SetContent(Width);
	UVerticalBox* Box = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Width->SetContent(Box);

	Box->AddChildToVerticalBox(MakeText(Tree, TEXT("Title"), LOCTEXT("Title", "Pausa"), 36, Amber, TEXT("Bold")));
	Box->AddChildToVerticalBox(MakeText(Tree, TEXT("Subtitle"), LOCTEXT("Subtitle", "La partida sigue para el resto de la banda."), 15, Muted, TEXT("Italic")))
		->SetPadding(FMargin(0.f, 2.f, 0.f, 22.f));

	AddButton(*Box, LOCTEXT("Resume", "Seguir jugando"))->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);
	ReturnToHideoutButton = AddButton(*Box, LOCTEXT("Hideout", "Terminar la noche y volver a La Madriguera"));
	ReturnToHideoutButton->OnClicked.AddDynamic(this, &ThisClass::HandleReturnToHideoutClicked);
	AddButton(*Box, LOCTEXT("MainMenu", "Salir al menú principal"))->OnClicked.AddDynamic(this, &ThisClass::HandleMainMenuClicked);
	AddButton(*Box, LOCTEXT("Quit", "Cerrar el juego"))->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);

	UTextBlock* Controls = MakeText(Tree, TEXT("Controls"), LOCTEXT("Controls",
		"WASD  moverse     Ratón  mirar     Espacio  saltar\n"
		"Shift  correr     Ctrl  agacharse\n"
		"E  coger botín, votar o huir en la camioneta     N  votar no\n"
		"G  soltar     Clic izq. (mantener)  lanzar\n"
		"Clic der.  usar herramienta     1 / 2  cambiar     Q  consumible\n"
		"Clic central  ping     V  hablar     P / Esc  pausa"), 14, Muted);
	Box->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	return true;
}

UButton* UNDUIPauseScreen::AddButton(UVerticalBox& Box, const FText& Label)
{
	UTextBlock* Label_ = nullptr;
	UButton* Button = NDUINativeStyle::MakeButton(*WidgetTree, Label, Label_);
	UVerticalBoxSlot* ButtonSlot = Box.AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.f, 5.f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	Buttons.Add(Button);
	Labels.Add(Label_);
	return Button;
}

void UNDUIPauseScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (ReturnToHideoutButton)
	{
		ReturnToHideoutButton->SetVisibility(CanReturnToHideout() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	bFocusPending = true;
}

void UNDUIPauseScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bFocusPending && Buttons.Num() > 0)
	{
		bFocusPending = false;
		Buttons[0]->SetKeyboardFocus();
	}
	Highlighted = NDUINativeStyle::UpdateHighlight(Buttons, Labels, Highlighted.Get());
}

FReply UNDUIPauseScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Con la pausa abierta la entrada va a la UI: la misma tecla la cierra.
	if (IsPauseKey(InKeyEvent.GetKey()) || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		HandleResumeClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* UNDUIPauseScreen::NativeGetDesiredFocusTarget() const
{
	return Buttons.Num() > 0 ? Buttons[0].Get() : Super::NativeGetDesiredFocusTarget();
}

bool UNDUIPauseScreen::CanReturnToHideout() const
{
	const UWorld* World = GetWorld();
	const ANDExpeditionGameState* ExpState = World ? World->GetGameState<ANDExpeditionGameState>() : nullptr;
	return World && World->GetAuthGameMode<ANDExpeditionGameMode>() && ExpState
		&& ExpState->GetSessionPhase() == ENDExpeditionSessionPhase::InProgress;
}

void UNDUIPauseScreen::HandleResumeClicked()
{
	DeactivateWidget();
}

void UNDUIPauseScreen::HandleReturnToHideoutClicked()
{
	// La noche se cierra como si llegara su fin: resultado para todos y vuelta a La Madriguera.
	if (ANDExpeditionGameMode* GameMode = CanReturnToHideout() ? GetWorld()->GetAuthGameMode<ANDExpeditionGameMode>() : nullptr)
	{
		GameMode->EndExpedition();
	}
	DeactivateWidget();
}

void UNDUIPauseScreen::HandleMainMenuClicked()
{
	UNDGameInstance* GI = GetGameInstance<UNDGameInstance>();
	if (!GI)
	{
		return;
	}
	if (UNDLoadingScreenSubsystem* Loading = GI->GetSubsystem<UNDLoadingScreenSubsystem>())
	{
		Loading->ShowTransition(LOCTEXT("ToMenu", "Volviendo al menú..."));
	}
	GI->ReturnToMainMenuWithReason(ENDDisconnectReason::LeftVoluntarily);
}

void UNDUIPauseScreen::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
