// No Despiertes Eso

#include "HouseGen/NDHouseGenDebugActor.h"

#include "Components/TextRenderComponent.h"
#include "Core/NDLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HouseGen/NDHouseGenDebug.h"
#include "HouseGen/NDHouseGenerator.h"

ANDHouseGenDebugActor::ANDHouseGenDebugActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ANDHouseGenDebugActor::BeginPlay()
{
	Super::BeginPlay();
	if (bGenerateOnBeginPlay)
	{
		Generate();
	}
}

void ANDHouseGenDebugActor::Clear()
{
	FlushPersistentDebugLines(GetWorld());
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();
}

void ANDHouseGenDebugActor::NextSeed()
{
	++Seed;
	Generate();
}

void ANDHouseGenDebugActor::Generate()
{
	UWorld* World = GetWorld();
	if (!World || (World->IsGameWorld() && !HasAuthority()))
	{
		return;
	}
	Clear();

	FNDHouseGenParams Params = FNDHouseGenParams::ForArchetype(Archetype);
	Params.Seed = Seed;
	Params.RoomCount = RoomCount;
	Params.Lot = LotInfo;
	Params.Lot.LotTransform = GetActorTransform();

	TArray<FString> Errors;
	FNDHouseLayout Layout;
	FString LayoutError;
	if (!FNDHouseGenerator::Generate(Params, LastManifest, &Errors) || !FNDHouseLayout::FromManifest(LastManifest, Layout, LayoutError))
	{
		LastResult = FString::Printf(TEXT("Semilla %lld: FALLO (%s)"), Seed, Errors.Num() > 0 ? *Errors[0] : *LayoutError);
		UE_LOG(LogNDHouseGen, Warning, TEXT("%s"), *LastResult);
		return;
	}

	LastResult = FString::Printf(TEXT("Semilla %lld: %d salas, %d puertas, %d intentos%s"), Seed, LastManifest.Rooms.Num(),
		LastManifest.Connections.Num(), LastManifest.Attempts, LastManifest.bUsedFallback ? TEXT(", casa de respaldo") : TEXT(""));
	UE_LOG(LogNDHouseGen, Log, TEXT("%s\n%s"), *LastResult, *LastManifest.ToDebugString());

	NDHouseGenDebug::DrawLayout(World, GetActorTransform(), Layout, Params.CarryEnvelopeWidth, false);

	// DrawDebugString no se ve en el editor; los rótulos van en componentes de texto.
	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
		Label->SetupAttachment(Root);
		Label->RegisterComponent();
		Label->SetRelativeLocationAndRotation(Layout.GetRoomCenter(RoomIndex) + FVector(0.f, 0.f, 200.f), FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(45.f);
		Label->SetTextRenderColor(NDHouseGen::GetRoomTypeColor(Layout.Rooms[RoomIndex].Def->RoomType));
		Label->SetText(FText::FromString(NDHouseGenDebug::GetRoomLabel(Layout, RoomIndex)));
		Labels.Add(Label);
	}
}
