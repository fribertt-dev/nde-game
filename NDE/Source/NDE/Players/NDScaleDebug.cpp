// No Despiertes Eso

#include "Core/NDLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HouseGen/NDHouseGenTypes.h"
#include "Players/NDScaleSettings.h"

namespace NDScaleDebug
{
	/**
	 * nde.Scale.Debug [segundos]: delante del jugador dibuja una regla con las alturas de referencia del
	 * Desvelado (escalón, ojos, cápsula, salto) y de la casa (puerta, techo, piso de arriba), y el ancho
	 * de una puerta Door_M, para comprobar que la escala tiene sentido.
	 */
	void Draw(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogNDNet, Warning, TEXT("nde.Scale.Debug: no hay jugador local con personaje"));
			return;
		}
		const float Duration = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.f;
		const UNDScaleSettings* Scale = UNDScaleSettings::Get();

		// Regla vertical 150 cm delante del jugador, a ras del suelo que pisa.
		const FRotator Yaw(0.f, PC->GetControlRotation().Yaw, 0.f);
		const FVector Forward = Yaw.Vector();
		const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		const FVector Floor = Pawn->GetActorLocation() - FVector(0.f, 0.f, Scale->CapsuleHalfHeight);
		const FVector Base = Floor + Forward * 150.f;

		struct FMark { const TCHAR* Label; float Height; FColor Color; };
		const FMark Marks[] = {
			{ TEXT("Suelo"), 0.f, FColor::White },
			{ TEXT("Escalon max."), Scale->MaxStepHeight, FColor::Orange },
			{ TEXT("Ojos"), Scale->EyeHeight, FColor::Cyan },
			{ TEXT("Capsula"), Scale->CapsuleHalfHeight * 2.f, FColor::Green },
			{ TEXT("Salto"), Scale->CapsuleHalfHeight * 2.f + Scale->GetJumpApex(World->GetGravityZ()), FColor::Yellow },
			{ TEXT("Puerta"), NDHouseGen::DoorHeight, FColor::Magenta },
			{ TEXT("Techo"), NDHouseGen::WallHeight, FColor::Red },
			{ TEXT("Piso de arriba"), NDHouseGen::FloorHeight, FColor::Silver },
		};
		DrawDebugLine(World, Base, Base + FVector(0.f, 0.f, NDHouseGen::FloorHeight), FColor::White, false, Duration, 0, 2.f);
		for (const FMark& Mark : Marks)
		{
			const FVector Point = Base + FVector(0.f, 0.f, Mark.Height);
			DrawDebugLine(World, Point - Right * 40.f, Point + Right * 40.f, Mark.Color, false, Duration, 0, 2.f);
			DrawDebugString(World, Point + Right * 45.f, FString::Printf(TEXT("%s %.0f cm"), Mark.Label, Mark.Height), nullptr, Mark.Color, Duration, true);
		}

		// Ancho de una puerta Door_M y de la cápsula, en el suelo, para comparar.
		const float DoorWidth = NDHouseGen::GetDefaultSocketWidth(ENDRoomSocketType::Door_M);
		const FVector DoorBase = Floor + Forward * 100.f;
		DrawDebugLine(World, DoorBase - Right * DoorWidth * 0.5f, DoorBase + Right * DoorWidth * 0.5f, FColor::Magenta, false, Duration, 0, 3.f);
		DrawDebugCircle(World, Floor + FVector(0.f, 0.f, 1.f), Scale->CapsuleRadius, 24, FColor::Green, false, Duration, 0, 2.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		UE_LOG(LogNDNet, Log, TEXT("nde.Scale.Debug: ojos %.0f, capsula %.0f, escalon %.0f, salto %.0f, puerta %.0f x %.0f, techo %.0f cm"),
			Scale->EyeHeight, Scale->CapsuleHalfHeight * 2.f, Scale->MaxStepHeight, Scale->GetJumpApex(World->GetGravityZ()),
			DoorWidth, NDHouseGen::DoorHeight, NDHouseGen::WallHeight);
	}

	FAutoConsoleCommandWithWorldAndArgs DebugCommand(
		TEXT("nde.Scale.Debug"),
		TEXT("Dibuja delante del jugador una regla con las alturas de referencia (ojos, capsula, escalon, salto, puerta, techo). Uso: nde.Scale.Debug [segundos]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Draw));
}
