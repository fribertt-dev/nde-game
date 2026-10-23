// No Despiertes Eso

#include "Players/NDDesveladoBodyComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Players/NDHidingComponent.h"
#include "Players/NDScaleSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace NDBody
{
	/** Altura de diseño de las recetas, en cm (cápsula de 34 de media altura). */
	constexpr float DesignHalfHeight = 34.f;

	const FLinearColor White(0.95f, 0.95f, 0.93f);
	const FLinearColor Black(0.02f, 0.02f, 0.025f);
	const FLinearColor Leather(0.32f, 0.19f, 0.10f);
	const FLinearColor LeatherDark(0.18f, 0.10f, 0.05f);
	const FLinearColor Pink(0.75f, 0.42f, 0.42f);

	/** Paleta de referencia de cada especie (GDD 3.2). */
	struct FPalette
	{
		FLinearColor Primary;
		FLinearColor Secondary;
		FLinearColor Accent;
		FLinearColor Iris;
		float EyeScale = 1.f;
	};

	FPalette GetPalette(ENDDesveladoSpecies Species)
	{
		switch (Species)
		{
		case ENDDesveladoSpecies::Lemur:  return { FLinearColor(0.50f, 0.50f, 0.52f), FLinearColor(0.93f, 0.93f, 0.92f), Black, FLinearColor(1.f, 0.50f, 0.04f), 1.3f };
		case ENDDesveladoSpecies::Bat:    return { FLinearColor(0.20f, 0.13f, 0.10f), FLinearColor(0.42f, 0.38f, 0.36f), FLinearColor(0.11f, 0.07f, 0.06f), FLinearColor(0.08f, 0.05f, 0.04f), 0.8f };
		case ENDDesveladoSpecies::Fox:    return { FLinearColor(0.86f, 0.50f, 0.20f), FLinearColor(0.98f, 0.92f, 0.80f), FLinearColor(0.16f, 0.10f, 0.07f), FLinearColor(0.85f, 0.55f, 0.10f), 1.f };
		case ENDDesveladoSpecies::Ferret: return { FLinearColor(0.90f, 0.82f, 0.66f), FLinearColor(0.97f, 0.93f, 0.85f), FLinearColor(0.42f, 0.27f, 0.15f), FLinearColor(0.06f, 0.04f, 0.03f), 0.9f };
		case ENDDesveladoSpecies::Owl:    return { FLinearColor(0.45f, 0.30f, 0.16f), FLinearColor(0.95f, 0.87f, 0.70f), FLinearColor(0.72f, 0.52f, 0.24f), FLinearColor(1.f, 0.78f, 0.08f), 1.35f };
		case ENDDesveladoSpecies::Raccoon:
		default:                          return { FLinearColor(0.42f, 0.42f, 0.45f), FLinearColor(0.86f, 0.86f, 0.83f), FLinearColor(0.07f, 0.07f, 0.08f), FLinearColor(0.20f, 0.12f, 0.06f), 1.f };
		}
	}
}

UNDDesveladoBodyComponent::UNDDesveladoBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	SphereMesh = Sphere.Object;
	ConeMesh = Cone.Object;
	CylinderMesh = Cylinder.Object;
	CubeMesh = Cube.Object;
	ShapeMaterial = Material.Object;
}

FLinearColor UNDDesveladoBodyComponent::GetSpeciesColor(ENDDesveladoSpecies InSpecies)
{
	return NDBody::GetPalette(InSpecies).Primary;
}

UStaticMesh* UNDDesveladoBodyComponent::GetShapeMesh(EShape Shape) const
{
	switch (Shape)
	{
	case EShape::Cone:     return ConeMesh;
	case EShape::Cylinder: return CylinderMesh;
	case EShape::Cube:     return CubeMesh;
	default:               return SphereMesh;
	}
}

void UNDDesveladoBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	PlaceOnCapsule();
	// Sin animaciones en un servidor dedicado: nadie lo ve.
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UNDDesveladoBodyComponent::PlaceOnCapsule()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float HalfHeight = UNDScaleSettings::Get()->CapsuleHalfHeight;
	BodyScale = HalfHeight / NDBody::DesignHalfHeight;

	// Mesh de pie por defecto (el agacharse lo sube y así los pies siguen en el suelo).
	FTransform MeshRelative = FTransform::Identity;
	if (Character && GetAttachParent() == Character->GetMesh())
	{
		const ACharacter* Defaults = Character->GetClass()->GetDefaultObject<ACharacter>();
		MeshRelative = FTransform(Defaults->GetBaseRotationOffset(), Defaults->GetBaseTranslationOffset());
	}
	const FTransform FeetOnCapsule(FQuat::Identity, FVector(0.f, 0.f, -HalfHeight), FVector(BodyScale));
	SetRelativeTransform(FeetOnCapsule * MeshRelative.Inverse());
}

void UNDDesveladoBodyComponent::ClearParts()
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	HeadParts.Reset();
}

USceneComponent* UNDDesveladoBodyComponent::EnsurePivot(EGroup Group)
{
	const int32 Index = static_cast<int32>(Group);
	if (Pivots.Num() == static_cast<int32>(EGroup::Count) && Pivots[Index])
	{
		return Pivots[Index];
	}
	if (Pivots.Num() != static_cast<int32>(EGroup::Count))
	{
		Pivots.SetNum(static_cast<int32>(EGroup::Count));
		PivotRest.SetNum(static_cast<int32>(EGroup::Count));
	}

	// Raíz (pies en el suelo) > Body (balanceo) > Head, Tail, manos. Los pies cuelgan de la raíz.
	static const FVector Rest[] = {
		FVector(0.f, 0.f, 0.f),     // Body
		FVector(2.f, 0.f, 54.f),    // Head (cuello)
		FVector(-15.f, 0.f, 16.f),  // Tail
		FVector(3.f, 8.f, 3.f),     // FootL
		FVector(3.f, -8.f, 3.f),    // FootR
		FVector(5.f, 16.f, 26.f),   // HandL
		FVector(5.f, -16.f, 26.f),  // HandR
	};
	USceneComponent* Parent = this;
	if (Group != EGroup::Body && Group != EGroup::FootL && Group != EGroup::FootR)
	{
		Parent = EnsurePivot(EGroup::Body);
	}
	const FName Name = MakeUniqueObjectName(GetOwner(), USceneComponent::StaticClass(), *FString::Printf(TEXT("BodyPivot_%d"), Index));
	USceneComponent* Pivot = NewObject<USceneComponent>(GetOwner(), Name, RF_Transient);
	Pivot->SetupAttachment(Parent);
	Pivot->SetRelativeLocation(Rest[Index]);
	Pivot->RegisterComponent();
	Pivots[Index] = Pivot;
	PivotRest[Index] = Rest[Index];
	return Pivot;
}

void UNDDesveladoBodyComponent::BuildSpecies(ENDDesveladoSpecies NewSpecies)
{
	if (NewSpecies == ENDDesveladoSpecies::None)
	{
		NewSpecies = ENDDesveladoSpecies::Raccoon;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	// Cuando el Blueprint tenga skeletal mesh, manda ese.
	if (Character && Character->GetMesh() && Character->GetMesh()->GetSkeletalMeshAsset())
	{
		ClearParts();
		return;
	}
	if (NewSpecies == Species && IsBuilt())
	{
		return;
	}
	ClearParts();
	Species = NewSpecies;

	TArray<FPart> Recipe;
	MakeRecipe(Species, Recipe);
	int32 Counter = 0;
	for (const FPart& Spec : Recipe)
	{
		USceneComponent* Pivot = EnsurePivot(Spec.Group);
		const FName Name = MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("BodyPart_%d"), Counter++));
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner(), Name, RF_Transient);
		Part->SetupAttachment(Pivot);
		Part->SetStaticMesh(GetShapeMesh(Spec.Shape));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->bCastHiddenShadow = true;
		Part->SetRelativeLocationAndRotation(Spec.Location, Spec.Rotation);
		// Las formas básicas miden 100 cm.
		Part->SetRelativeScale3D(Spec.Size / 100.f);
		Part->RegisterComponent();
		if (ShapeMaterial)
		{
			UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, ShapeMaterial);
			Material->SetVectorParameterValue(TEXT("Color"), Spec.Color);
		}
		Parts.Add(Part);
		if (Spec.Group == EGroup::Head)
		{
			HeadParts.Add(Part);
			Part->SetOwnerNoSee(bFirstPersonOwner);
		}
	}
}

void UNDDesveladoBodyComponent::SetFirstPersonOwner(bool bFirstPerson)
{
	bFirstPersonOwner = bFirstPerson;
	for (UStaticMeshComponent* Part : HeadParts)
	{
		if (Part)
		{
			Part->SetOwnerNoSee(bFirstPerson);
		}
	}
}

void UNDDesveladoBodyComponent::MakeRecipe(ENDDesveladoSpecies InSpecies, TArray<FPart>& Out)
{
	using namespace NDBody;
	const FPalette P = GetPalette(InSpecies);
	const bool bOwl = InSpecies == ENDDesveladoSpecies::Owl;
	const bool bFerret = InSpecies == ENDDesveladoSpecies::Ferret;

	auto Add = [&Out](EShape Shape, EGroup Group, FVector Location, FVector Size, FLinearColor Color, FRotator Rotation = FRotator::ZeroRotator)
	{
		Out.Add({ Shape, Group, Location, Rotation, Size, Color });
	};
	// Pareja izquierda/derecha: Y y el alabeo cambian de signo.
	auto AddPair = [&Add](EShape Shape, EGroup Group, FVector Location, FVector Size, FLinearColor Color, FRotator Rotation = FRotator::ZeroRotator)
	{
		Add(Shape, Group, Location, Size, Color, Rotation);
		Add(Shape, Group, FVector(Location.X, -Location.Y, Location.Z), Size, Color, FRotator(Rotation.Pitch, -Rotation.Yaw, -Rotation.Roll));
	};

	// --- Cuerpo, barriga y mochila de ladrón.
	if (bFerret)
	{
		Add(EShape::Sphere, EGroup::Body, FVector(-2.f, 0.f, 20.f), FVector(42.f, 26.f, 30.f), P.Primary);
		Add(EShape::Sphere, EGroup::Body, FVector(9.f, 0.f, 19.f), FVector(18.f, 18.f, 22.f), P.Secondary);
	}
	else
	{
		Add(EShape::Sphere, EGroup::Body, FVector(0.f, 0.f, 24.f), FVector(30.f, 30.f, 36.f), P.Primary);
		Add(EShape::Sphere, EGroup::Body, FVector(8.f, 0.f, 22.f), FVector(16.f, 22.f, 26.f), P.Secondary);
	}
	Add(EShape::Cube, EGroup::Body, FVector(-15.f, 0.f, 30.f), FVector(10.f, 20.f, 22.f), Leather);
	Add(EShape::Cube, EGroup::Body, FVector(-20.5f, 0.f, 37.f), FVector(2.f, 18.f, 7.f), LeatherDark);
	AddPair(EShape::Cube, EGroup::Body, FVector(0.f, 9.f, 34.f), FVector(28.f, 2.f, 3.f), LeatherDark, FRotator(-20.f, 0.f, 0.f));

	// --- Pies y manos (manos exageradas, GDD 3.1).
	const FLinearColor FootColor = bOwl ? FLinearColor(0.95f, 0.72f, 0.18f) : P.Accent;
	Add(EShape::Sphere, EGroup::FootL, FVector(2.f, 0.f, 0.f), FVector(14.f, 9.f, 6.f), FootColor);
	Add(EShape::Sphere, EGroup::FootR, FVector(2.f, 0.f, 0.f), FVector(14.f, 9.f, 6.f), FootColor);
	const FLinearColor HandColor = (InSpecies == ENDDesveladoSpecies::Raccoon || InSpecies == ENDDesveladoSpecies::Lemur || InSpecies == ENDDesveladoSpecies::Fox) ? P.Accent : P.Primary;
	Add(EShape::Sphere, EGroup::HandL, FVector::ZeroVector, FVector(9.f, 9.f, 9.f), HandColor);
	Add(EShape::Sphere, EGroup::HandR, FVector::ZeroVector, FVector(9.f, 9.f, 9.f), HandColor);

	// --- Cabeza y ojos grandes.
	Add(EShape::Sphere, EGroup::Head, FVector(0.f, 0.f, 2.f), bOwl ? FVector(34.f, 34.f, 32.f) : FVector(32.f, 32.f, 30.f), P.Primary);
	const float E = P.EyeScale;
	const float EyeY = 6.f * FMath::Max(1.f, E * 0.95f);
	const float EyeZ = bOwl ? 6.f : 5.f;
	AddPair(EShape::Sphere, EGroup::Head, FVector(13.f, EyeY, EyeZ), FVector(9.f, 9.f, 10.f) * E, White);
	AddPair(EShape::Sphere, EGroup::Head, FVector(13.f + 3.f * E, EyeY + 0.2f, EyeZ), FVector(6.f, 6.f, 7.f) * E, P.Iris);
	AddPair(EShape::Sphere, EGroup::Head, FVector(13.f + 5.4f * E, EyeY + 0.3f, EyeZ + 0.2f), FVector(3.f, 3.f, 3.5f) * E, Black);
	AddPair(EShape::Sphere, EGroup::Head, FVector(13.f + 6.f * E, EyeY - 0.9f * E, EyeZ + 1.5f * E), FVector(1.4f, 1.4f, 1.4f) * E, White);

	switch (InSpecies)
	{
	case ENDDesveladoSpecies::Lemur:
		Add(EShape::Sphere, EGroup::Head, FVector(14.f, 0.f, -4.f), FVector(14.f, 9.f, 8.f), P.Accent);
		Add(EShape::Sphere, EGroup::Head, FVector(21.f, 0.f, -3.f), FVector(4.f, 5.f, 3.5f), Black);
		Add(EShape::Sphere, EGroup::Head, FVector(9.f, 0.f, 12.f), FVector(10.f, 14.f, 6.f), P.Secondary);
		AddPair(EShape::Sphere, EGroup::Head, FVector(-2.f, 12.f, 12.f), FVector(4.f, 9.f, 9.f), P.Secondary);
		for (int32 i = 0; i < 7; ++i)
		{
			const float T = i / 6.f;
			Add(EShape::Sphere, EGroup::Tail, FVector(-4.f - 18.f * T, 0.f, 26.f * T * T), FVector(8.f, 8.f, 8.f), i % 2 == 0 ? P.Secondary : P.Accent);
		}
		break;

	case ENDDesveladoSpecies::Bat:
		Add(EShape::Sphere, EGroup::Head, FVector(14.f, 0.f, -3.f), FVector(8.f, 9.f, 7.f), P.Primary);
		Add(EShape::Sphere, EGroup::Head, FVector(18.f, 0.f, -2.f), FVector(3.5f, 4.f, 3.f), Pink);
		AddPair(EShape::Cone, EGroup::Head, FVector(0.f, 10.f, 21.f), FVector(5.f, 13.f, 22.f), P.Primary, FRotator(0.f, 0.f, -14.f));
		AddPair(EShape::Cone, EGroup::Head, FVector(1.5f, 10.f, 20.f), FVector(2.f, 8.f, 16.f), Pink, FRotator(0.f, 0.f, -14.f));
		AddPair(EShape::Sphere, EGroup::Body, FVector(-6.f, 17.f, 30.f), FVector(4.f, 18.f, 30.f), P.Accent, FRotator(0.f, 0.f, -22.f));
		break;

	case ENDDesveladoSpecies::Fox:
		Add(EShape::Cone, EGroup::Head, FVector(19.f, 0.f, -4.f), FVector(11.f, 11.f, 16.f), P.Secondary, FRotator(-90.f, 0.f, 0.f));
		Add(EShape::Sphere, EGroup::Head, FVector(27.f, 0.f, -4.f), FVector(4.f, 4.f, 3.5f), Black);
		AddPair(EShape::Sphere, EGroup::Head, FVector(9.f, 8.f, -5.f), FVector(10.f, 9.f, 8.f), P.Secondary);
		AddPair(EShape::Cone, EGroup::Head, FVector(-1.f, 9.f, 20.f), FVector(6.f, 13.f, 18.f), P.Primary, FRotator(0.f, 0.f, -12.f));
		AddPair(EShape::Cone, EGroup::Head, FVector(0.5f, 9.f, 19.f), FVector(2.f, 8.f, 13.f), P.Accent, FRotator(0.f, 0.f, -12.f));
		Add(EShape::Sphere, EGroup::Tail, FVector(-5.f, 0.f, -1.f), FVector(13.f, 13.f, 13.f), P.Primary);
		Add(EShape::Sphere, EGroup::Tail, FVector(-13.f, 0.f, 3.f), FVector(16.f, 15.f, 15.f), P.Primary);
		Add(EShape::Sphere, EGroup::Tail, FVector(-21.f, 0.f, 9.f), FVector(12.f, 12.f, 12.f), P.Secondary);
		break;

	case ENDDesveladoSpecies::Ferret:
		Add(EShape::Sphere, EGroup::Head, FVector(14.f, 0.f, -4.f), FVector(13.f, 10.f, 8.f), P.Secondary);
		Add(EShape::Sphere, EGroup::Head, FVector(20.f, 0.f, -3.f), FVector(4.f, 4.5f, 3.f), Pink);
		Add(EShape::Sphere, EGroup::Head, FVector(12.f, 0.f, 5.f), FVector(6.f, 25.f, 8.f), P.Accent);
		AddPair(EShape::Sphere, EGroup::Head, FVector(-2.f, 11.f, 11.f), FVector(4.f, 8.f, 7.f), P.Secondary);
		Add(EShape::Sphere, EGroup::Tail, FVector(-5.f, 0.f, -4.f), FVector(9.f, 9.f, 9.f), P.Accent);
		Add(EShape::Sphere, EGroup::Tail, FVector(-11.f, 0.f, -5.f), FVector(8.f, 8.f, 8.f), P.Accent);
		Add(EShape::Sphere, EGroup::Tail, FVector(-17.f, 0.f, -5.f), FVector(7.f, 7.f, 7.f), P.Accent);
		break;

	case ENDDesveladoSpecies::Owl:
		Add(EShape::Sphere, EGroup::Head, FVector(11.f, 0.f, 3.f), FVector(8.f, 30.f, 26.f), P.Secondary);
		Add(EShape::Cone, EGroup::Head, FVector(17.f, 0.f, -1.f), FVector(5.f, 5.f, 8.f), FLinearColor(0.95f, 0.72f, 0.18f), FRotator(-150.f, 0.f, 0.f));
		AddPair(EShape::Cone, EGroup::Head, FVector(-2.f, 9.f, 16.f), FVector(4.f, 6.f, 11.f), P.Primary, FRotator(0.f, 0.f, -20.f));
		AddPair(EShape::Sphere, EGroup::Body, FVector(-3.f, 15.f, 26.f), FVector(8.f, 10.f, 26.f), P.Accent, FRotator(0.f, 0.f, -10.f));
		Add(EShape::Sphere, EGroup::Tail, FVector(-3.f, 0.f, -4.f), FVector(6.f, 12.f, 4.f), P.Accent);
		break;

	case ENDDesveladoSpecies::Raccoon:
	default:
		Add(EShape::Sphere, EGroup::Head, FVector(14.f, 0.f, -3.f), FVector(12.f, 10.f, 8.f), P.Secondary);
		Add(EShape::Sphere, EGroup::Head, FVector(20.f, 0.f, -2.f), FVector(4.f, 5.f, 3.5f), Black);
		Add(EShape::Sphere, EGroup::Head, FVector(12.f, 0.f, 5.f), FVector(6.f, 26.f, 9.f), P.Accent);
		Add(EShape::Sphere, EGroup::Head, FVector(10.f, 0.f, 12.f), FVector(8.f, 12.f, 5.f), P.Secondary);
		AddPair(EShape::Sphere, EGroup::Head, FVector(-1.f, 10.f, 15.f), FVector(5.f, 10.f, 11.f), P.Primary);
		AddPair(EShape::Sphere, EGroup::Head, FVector(0.5f, 10.f, 15.f), FVector(3.f, 6.f, 7.f), P.Accent);
		for (int32 i = 0; i < 5; ++i)
		{
			const float T = i / 4.f;
			Add(EShape::Sphere, EGroup::Tail, FVector(-4.f - 18.f * T, 0.f, 13.f * T * T), FVector(11.f, 11.f, 11.f) * (1.f - 0.15f * T), i % 2 == 0 ? P.Primary : P.Accent);
		}
		break;
	}
}

void UNDDesveladoBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsBuilt())
	{
		return;
	}
	// Dentro o debajo de un mueble no se ve; detrás, sí (asoma).
	const UNDHidingComponent* Hiding = GetOwner() ? GetOwner()->FindComponentByClass<UNDHidingComponent>() : nullptr;
	const bool bConcealed = Hiding && Hiding->IsHidden() && Hiding->GetHiddenSpotKind() != ENDHidingSpotKind::BehindFurniture;
	if (IsVisible() == bConcealed)
	{
		SetVisibility(!bConcealed, true);
	}
	if (bProceduralMotion && !bConcealed)
	{
		UpdateMotion(DeltaTime);
	}
}

void UNDDesveladoBodyComponent::UpdateMotion(float DeltaTime)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Pivots.Num() != static_cast<int32>(EGroup::Count))
	{
		return;
	}
	auto Pivot = [this](EGroup Group) { return Pivots[static_cast<int32>(Group)].Get(); };
	auto Rest = [this](EGroup Group) { return PivotRest[static_cast<int32>(Group)]; };

	const float Speed = Character->GetVelocity().Size2D();
	const bool bGrounded = !Character->GetMovementComponent() || Character->GetMovementComponent()->IsMovingOnGround();
	MoveAlpha = FMath::FInterpTo(MoveAlpha, bGrounded ? FMath::Clamp(Speed / 300.f, 0.f, 1.f) : 0.f, DeltaTime, 8.f);
	// Un ciclo de dos pasos cada 110 cm.
	WalkPhase = FMath::Fmod(WalkPhase + DeltaTime * Speed / 110.f * 2.f * PI, 2.f * PI);
	IdleTime += DeltaTime;

	const float S = FMath::Sin(WalkPhase);
	const float C = FMath::Cos(WalkPhase);

	// Cuerpo: rebote al andar, respiración en reposo y algo de inclinación hacia delante al correr.
	const float Breath = FMath::Sin(IdleTime * 2.2f) * 0.6f * (1.f - MoveAlpha);
	const float Bounce = FMath::Abs(S) * 2.5f * MoveAlpha;
	if (USceneComponent* Body = Pivot(EGroup::Body))
	{
		Body->SetRelativeLocationAndRotation(Rest(EGroup::Body) + FVector(0.f, 0.f, Bounce + Breath), FRotator(-6.f * MoveAlpha * FMath::Clamp(Speed / 450.f, 0.f, 1.f), 0.f, S * 3.f * MoveAlpha));
	}
	// En el aire, piernas recogidas.
	const float AirTuck = bGrounded ? 0.f : 4.f;
	if (USceneComponent* Foot = Pivot(EGroup::FootL))
	{
		Foot->SetRelativeLocation(Rest(EGroup::FootL) + FVector(S * 7.f * MoveAlpha, 0.f, FMath::Max(0.f, C) * 4.f * MoveAlpha + AirTuck));
	}
	if (USceneComponent* Foot = Pivot(EGroup::FootR))
	{
		Foot->SetRelativeLocation(Rest(EGroup::FootR) + FVector(-S * 7.f * MoveAlpha, 0.f, FMath::Max(0.f, -C) * 4.f * MoveAlpha + AirTuck));
	}
	// Manos al revés que los pies; arriba si salta.
	const float HandsUp = bGrounded ? 0.f : 10.f;
	if (USceneComponent* Hand = Pivot(EGroup::HandL))
	{
		Hand->SetRelativeLocation(Rest(EGroup::HandL) + FVector(-S * 5.f * MoveAlpha, 0.f, HandsUp));
	}
	if (USceneComponent* Hand = Pivot(EGroup::HandR))
	{
		Hand->SetRelativeLocation(Rest(EGroup::HandR) + FVector(S * 5.f * MoveAlpha, 0.f, HandsUp));
	}
	// Cola: meneo lento en reposo y al paso al andar.
	if (USceneComponent* Tail = Pivot(EGroup::Tail))
	{
		const float Wag = FMath::Sin(IdleTime * 2.5f) * 12.f * (1.f - MoveAlpha) + S * 10.f * MoveAlpha;
		Tail->SetRelativeRotation(FRotator(0.f, Wag, 0.f));
	}
	// Cabeza: sigue el cabeceo de la mirada (replicado), algo menos que la cámara.
	if (USceneComponent* Head = Pivot(EGroup::Head))
	{
		const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch) * 0.6f, -35.f, 35.f);
		const float Tilt = FMath::Sin(IdleTime * 0.9f) * 3.f * (1.f - MoveAlpha);
		Head->SetRelativeRotation(FRotator(Pitch, 0.f, Tilt));
	}

	// Agachado: más bajo y ancho, con los pies en el suelo (la raíz está en los pies).
	const float Crouch = Character->bIsCrouched ? 1.f : 0.f;
	const FVector TargetScale = FVector(BodyScale) * FVector(1.f + 0.1f * Crouch, 1.f + 0.1f * Crouch, 1.f - 0.35f * Crouch);
	SetRelativeScale3D(FMath::VInterpTo(GetRelativeScale3D(), TargetScale, DeltaTime, 14.f));
}
