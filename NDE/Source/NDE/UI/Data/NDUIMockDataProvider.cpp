#include "UI/Data/NDUIMockDataProvider.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/NDUIFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "NDUIMock"

namespace
{
	FNDPlayerCardVM MakePlayer(const TCHAR* Id, const FText& Name, ENDDesveladoSpecies Species, bool bLocal, bool bHost, bool bReady, int32 Reputation)
	{
		FNDPlayerCardVM Player;
		Player.PlayerId = Id;
		Player.DisplayName = Name;
		Player.Species = Species;
		Player.bIsLocalPlayer = bLocal;
		Player.bIsHost = bHost;
		Player.bIsReady = bReady;
		Player.Reputation = Reputation;
		return Player;
	}

	FNDContractCardVM MakeContract(FName Id, const FText& Title, const FText& Family, int32 Tier, const FText& Main, const FText& Secondary, float Minutes)
	{
		FNDContractCardVM Contract;
		Contract.ContractId = Id;
		Contract.Title = Title;
		Contract.Family = Family;
		Contract.Tier = Tier;
		Contract.MainObjective = Main;
		Contract.SecondaryObjective = Secondary;
		Contract.NightDurationMinutes = Minutes;
		return Contract;
	}

	FNDShopItemVM MakeItem(FName Id, const FText& Name, const FText& Description, const FText& Drawback, int32 Price, bool bOwned = false)
	{
		FNDShopItemVM Item;
		Item.ItemId = Id;
		Item.DisplayName = Name;
		Item.Description = Description;
		Item.Drawback = Drawback;
		Item.Price = Price;
		Item.bOwned = bOwned;
		return Item;
	}
}

UNDUIMockDataProvider::UNDUIMockDataProvider()
{
	LobbyPlayers = {
		MakePlayer(TEXT("mock-0"), LOCTEXT("PlayerYou", "Tú"), ENDDesveladoSpecies::None, true, true, false, 120),
		MakePlayer(TEXT("mock-1"), LOCTEXT("Player1", "Bigotes"), ENDDesveladoSpecies::Raccoon, false, false, true, 340),
		MakePlayer(TEXT("mock-2"), LOCTEXT("Player2", "Pelusa"), ENDDesveladoSpecies::Owl, false, false, false, 80),
		MakePlayer(TEXT("mock-3"), LOCTEXT("Player3", "Ceniza"), ENDDesveladoSpecies::Ferret, false, false, true, 510),
	};

	// Subconjunto del catálogo de contratos del GDD, capítulo 12.
	Contracts = {
		MakeContract(TEXT("C01"), LOCTEXT("C01", "La Primera Siesta"), LOCTEXT("FamQuota", "Cuota"), 1,
			LOCTEXT("C01Main", "Extraer botín igual o superior a la cuota"), LOCTEXT("C01Sec", "Ningún jarrón roto"), 8.f),
		MakeContract(TEXT("C03"), LOCTEXT("C03", "El Reloj Perdido"), LOCTEXT("FamUnique", "Único"), 1,
			LOCTEXT("C03Main", "Recuperar el reloj objetivo"), LOCTEXT("C03Sec", "No alterar otros relojes"), 10.f),
		MakeContract(TEXT("C05"), LOCTEXT("C05", "La Hora del Ronquido"), LOCTEXT("FamBody", "Cuerpo"), 1,
			LOCTEXT("C05Main", "Robar la medalla de la cabecera"), LOCTEXT("C05Sec", "Solo 2 alertas por encima de ligero"), 10.f),
		MakeContract(TEXT("C07"), LOCTEXT("C07", "El Televisor Prestado"), LOCTEXT("FamHeavy", "Pesado"), 2,
			LOCTEXT("C07Main", "Transportar el televisor a la camioneta"), LOCTEXT("C07Sec", "Sin golpes fuertes"), 12.f),
	};
	Contracts.Last().bLocked = true;

	// Subconjunto del catálogo de herramientas del GDD, capítulo 18.
	ShopItems = {
		MakeItem(TEXT("T03"), LOCTEXT("T03", "Linterna de bolsillo"), LOCTEXT("T03Desc", "Ilumina zonas"), LOCTEXT("T03Con", "Puede delatarte"), 100, true),
		MakeItem(TEXT("T04"), LOCTEXT("T04", "Roncómetro"), LOCTEXT("T04Desc", "Indica la ventana de ronquido"), LOCTEXT("T04Con", "Falla ante pesadillas"), 220),
		MakeItem(TEXT("T05"), LOCTEXT("T05", "Guantes de ventosa"), LOCTEXT("T05Desc", "Mejor agarre y control"), LOCTEXT("T05Con", "Más lento al portar bultos"), 180),
		MakeItem(TEXT("T07"), LOCTEXT("T07", "Carrito plegable"), LOCTEXT("T07Desc", "Lleva un bulto pesado"), LOCTEXT("T07Con", "Ruidoso en suelos duros"), 350),
		MakeItem(TEXT("T08"), LOCTEXT("T08", "Ganzúa improvisada"), LOCTEXT("T08Desc", "Abre cerraduras con minijuego"), LOCTEXT("T08Con", "Tarda y puede chasquear"), 210),
		MakeItem(TEXT("T10"), LOCTEXT("T10", "Espejito de esquina"), LOCTEXT("T10Desc", "Ver tras las puertas"), LOCTEXT("T10Con", "Refleja luz en ciertos mapas"), 160),
	};

	// Dos slots activos y un consumible rápido (GDD 20.1).
	Loadout.SetNum(3);
	for (int32 Index = 0; Index < Loadout.Num(); ++Index)
	{
		Loadout[Index].SlotIndex = Index;
	}
	Loadout[0].ItemId = TEXT("T03");
	Loadout[0].DisplayName = ShopItems[0].DisplayName;
	Loadout[2].bIsQuickConsumable = true;

	NightClock.Phase = ENDUINightPhase::Insertion;
	NightClock.Quota = 500;
	NightClock.SecuredLootValue = 0;

	for (const FNDPlayerCardVM& Player : LobbyPlayers)
	{
		FNDPlayerVoiceVM Voice;
		Voice.PlayerId = Player.PlayerId;
		Voice.bIsAudible = !Player.bIsLocalPlayer;
		VoiceStates.Add(Voice);
	}
	VoiceStates[1].bIsSpeaking = true;
	VoiceStates[1].MonsterHearingRisk = 0.5f;

	Results.Outcome = ENDUIRunOutcome::EscapedBeforeDawn;
	Results.bContractCompleted = true;
	Results.MoneyEarned = 320;
	Results.ReputationEarned = 15;
	Results.FunnyIncidents = {
		LOCTEXT("Incident1", "Bigotes estornudó dentro del armario"),
		LOCTEXT("Incident2", "El televisor viajó en el carrito con Pelusa encima"),
	};
	Results.OutcomeTitle = LOCTEXT("MockOutcome", "Éxito");
	Results.LootPoints = 880;
	Results.Quota = 700;

	ExpeditionHUD.ContractName = LOCTEXT("MockContract", "La casa del relojero");
	ExpeditionHUD.ObjectiveText = LOCTEXT("MockObjective", "Llevad 700 puntos de botín a la camioneta");
	ExpeditionHUD.HandsItem = LOCTEXT("MockHands", "Reloj cucú");
	ExpeditionHUD.PocketCount = 1;
}

FNDPlayerCardVM UNDUIMockDataProvider::GetLocalPlayer_Implementation() const
{
	const int32 Index = FindLocalPlayerIndex();
	return LobbyPlayers.IsValidIndex(Index) ? LobbyPlayers[Index] : FNDPlayerCardVM();
}

int32 UNDUIMockDataProvider::FindLocalPlayerIndex() const
{
	return LobbyPlayers.IndexOfByPredicate([](const FNDPlayerCardVM& Player) { return Player.bIsLocalPlayer; });
}

void UNDUIMockDataProvider::RequestSelectSpecies_Implementation(ENDDesveladoSpecies Species)
{
	const int32 LocalIndex = FindLocalPlayerIndex();
	if (!LobbyPlayers.IsValidIndex(LocalIndex) || Species == ENDDesveladoSpecies::None)
	{
		NotifyCommandResult(TEXT("SelectSpecies"), false, LOCTEXT("InvalidSpecies", "Especie no válida"));
		return;
	}

	// Cada especie es única por partida.
	const bool bTaken = LobbyPlayers.ContainsByPredicate([Species](const FNDPlayerCardVM& Player)
	{
		return !Player.bIsLocalPlayer && Player.Species == Species;
	});
	if (bTaken)
	{
		NotifyCommandResult(TEXT("SelectSpecies"), false, LOCTEXT("SpeciesTaken", "Otro Desvelado ya eligió esa especie"));
		return;
	}

	LobbyPlayers[LocalIndex].Species = Species;
	NotifyCommandResult(TEXT("SelectSpecies"), true, FText::GetEmpty());
	NotifyDataChanged(NDUITopics::Lobby);
	NotifyDataChanged(NDUITopics::Profile);
}

void UNDUIMockDataProvider::RequestSetReady_Implementation(bool bReady)
{
	const int32 LocalIndex = FindLocalPlayerIndex();
	if (!LobbyPlayers.IsValidIndex(LocalIndex))
	{
		NotifyCommandResult(TEXT("SetReady"), false, FText::GetEmpty());
		return;
	}
	if (bReady && LobbyPlayers[LocalIndex].Species == ENDDesveladoSpecies::None)
	{
		NotifyCommandResult(TEXT("SetReady"), false, LOCTEXT("PickSpeciesFirst", "Elige una especie antes de marcarte listo"));
		return;
	}

	LobbyPlayers[LocalIndex].bIsReady = bReady;
	NotifyCommandResult(TEXT("SetReady"), true, FText::GetEmpty());
	NotifyDataChanged(NDUITopics::Lobby);
}

void UNDUIMockDataProvider::RequestVoteContract_Implementation(FName ContractId)
{
	FNDContractCardVM* Target = Contracts.FindByPredicate([ContractId](const FNDContractCardVM& Contract) { return Contract.ContractId == ContractId; });
	if (!Target || Target->bLocked)
	{
		NotifyCommandResult(TEXT("VoteContract"), false, LOCTEXT("ContractUnavailable", "Ese contrato no está disponible"));
		return;
	}

	if (FNDContractCardVM* Previous = Contracts.FindByPredicate([this](const FNDContractCardVM& Contract) { return Contract.ContractId == VotedContractId; }))
	{
		Previous->Votes = FMath::Max(0, Previous->Votes - 1);
	}
	Target->Votes++;
	VotedContractId = ContractId;

	NotifyCommandResult(TEXT("VoteContract"), true, FText::GetEmpty());
	NotifyDataChanged(NDUITopics::Contracts);
}

void UNDUIMockDataProvider::RequestPurchase_Implementation(FName ItemId)
{
	FNDShopItemVM* Item = ShopItems.FindByPredicate([ItemId](const FNDShopItemVM& Candidate) { return Candidate.ItemId == ItemId; });
	if (!Item)
	{
		NotifyCommandResult(TEXT("Purchase"), false, LOCTEXT("UnknownItem", "Objeto desconocido"));
		return;
	}
	if (Item->bOwned)
	{
		NotifyCommandResult(TEXT("Purchase"), false, LOCTEXT("AlreadyOwned", "Ya lo tienes"));
		return;
	}
	if (Money < Item->Price)
	{
		NotifyCommandResult(TEXT("Purchase"), false, LOCTEXT("NoFunds", "Fondos insuficientes"));
		return;
	}

	Money -= Item->Price;
	Item->bOwned = true;
	NotifyCommandResult(TEXT("Purchase"), true, LOCTEXT("SimulatedPurchase", "Compra simulada: no se guarda"));
	NotifyDataChanged(NDUITopics::Shop);
	NotifyDataChanged(NDUITopics::Profile);
}

void UNDUIMockDataProvider::RequestEquip_Implementation(int32 SlotIndex, FName ItemId)
{
	if (!Loadout.IsValidIndex(SlotIndex))
	{
		NotifyCommandResult(TEXT("Equip"), false, LOCTEXT("InvalidSlot", "Slot no válido"));
		return;
	}

	FText DisplayName;
	if (!ItemId.IsNone())
	{
		const FNDShopItemVM* Item = ShopItems.FindByPredicate([ItemId](const FNDShopItemVM& Candidate) { return Candidate.ItemId == ItemId; });
		if (!Item || !Item->bOwned)
		{
			NotifyCommandResult(TEXT("Equip"), false, LOCTEXT("NotOwned", "No tienes ese objeto"));
			return;
		}
		DisplayName = Item->DisplayName;
	}

	Loadout[SlotIndex].ItemId = ItemId;
	Loadout[SlotIndex].DisplayName = DisplayName;
	NotifyCommandResult(TEXT("Equip"), true, FText::GetEmpty());
	NotifyDataChanged(NDUITopics::Loadout);
}

void UNDUIMockDataProvider::StartSimulatedNight(const UObject* WorldContextObject, float DurationSeconds)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	if (UWorld* PreviousWorld = SimulatedNightWorld.Get())
	{
		PreviousWorld->GetTimerManager().ClearTimer(SimulatedNightTimer);
	}
	SimulatedNightWorld = World;

	NightClock.Phase = ENDUINightPhase::Infiltration;
	NightClock.NightStartServerTime = UNDUIFunctionLibrary::GetServerTimeSeconds(World);
	NightClock.NightDurationSeconds = FMath::Max(1.f, DurationSeconds);
	NightClock.DawnTriggeredServerTime = 0.0;
	NightClock.SecuredLootValue = 0;

	World->GetTimerManager().SetTimer(SimulatedNightTimer, FTimerDelegate::CreateUObject(this, &ThisClass::HandleSimulatedDawn), NightClock.NightDurationSeconds, false);
	NotifyDataChanged(NDUITopics::NightClock);
}

void UNDUIMockDataProvider::HandleSimulatedDawn()
{
	UWorld* World = SimulatedNightWorld.Get();
	if (!World)
	{
		return;
	}

	NightClock.Phase = ENDUINightPhase::DawnEscapeGrace;
	NightClock.DawnTriggeredServerTime = UNDUIFunctionLibrary::GetServerTimeSeconds(World);
	World->GetTimerManager().SetTimer(SimulatedNightTimer, FTimerDelegate::CreateUObject(this, &ThisClass::HandleSimulatedEscapeEnd), NightClock.DawnEscapeSeconds, false);
	NotifyDataChanged(NDUITopics::NightClock);
}

void UNDUIMockDataProvider::HandleSimulatedEscapeEnd()
{
	NightClock.Phase = ENDUINightPhase::Settlement;
	NotifyDataChanged(NDUITopics::NightClock);
}

void UNDUIMockDataProvider::SetSimulatedOutcome(ENDUIRunOutcome Outcome)
{
	Results.Outcome = Outcome;
	Results.bContractCompleted = Outcome == ENDUIRunOutcome::EscapedBeforeDawn || Outcome == ENDUIRunOutcome::EscapedDuringDawnChase;
	Results.FailureReason = Results.bContractCompleted
		? FText::GetEmpty()
		: LOCTEXT("FailureReason", "Nadie llegó a la camioneta antes de que terminara la huida del amanecer");
	NotifyDataChanged(NDUITopics::Results);
}

#undef LOCTEXT_NAMESPACE
