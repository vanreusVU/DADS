// Selected Codes from project

// Loads a random dataset based on the given SourceType
bool UCPPFunctionLibrary::LoadDataFromCSV(ESourceType SourceType, TArray<FString>& Result)
{
	FString Path = FPaths::ProjectDir() / TEXT("CSV/");
	
	// Print the path
	UE_LOG(LogTemp, Log, TEXT("Path: %s"), *Path);

	// Get the file manager reference
	IFileManager& FileManager = IFileManager::Get();

	// Find all files in the directory
	TArray<FString> UAVFiles;
	TArray<FString> NoiseFiles;
	FileManager.FindFiles(UAVFiles, *(Path / TEXT("UAV/")), true, false); 
	FileManager.FindFiles(NoiseFiles, *(Path / TEXT("NOISE/")), true, false);

	// Print the number of files
	UE_LOG(LogTemp, Log, TEXT("Number of UAV files: %d"), UAVFiles.Num());
	UE_LOG(LogTemp, Log, TEXT("Number of NOISE files: %d"), NoiseFiles.Num());

	switch (SourceType)
	{
		case ESourceType::UAV:
			if (UAVFiles.Num() > 0) {
				Path = UAVFiles[FMath::RandRange(0, UAVFiles.Num() - 1)];
				UE_LOG(LogTemp, Log, TEXT("Selected file path: %s"), *Path);
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("No file selected"));
			}
			break;

		case ESourceType::Noise:
			if (NoiseFiles.Num() > 0) {
				Path = NoiseFiles[FMath::RandRange(0, NoiseFiles.Num() - 1)];
				UE_LOG(LogTemp, Log, TEXT("Selected file path: %s"), *Path);
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("No file selected"));
			}
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("No source selected"));
			break;
	}

	if (FFileHelper::LoadFileToStringArray(Result, *Path))
	{
		UE_LOG(LogTemp, Log, TEXT("Loaded %s"), *Path);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load %s"), *Path);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Result size: %d"), Result.Num());

	return true;
}

// Converts given line into a array of strings by seperating it by ","
void UCPPFunctionLibrary::ConvertLineIntoArray(FString Line, TArray<FString>& Result)
{
	Line.ParseIntoArray(Result, TEXT(","), true);
}

// Used to simulate the directional antennas cone 
bool UCPPFunctionLibrary::MultiConeTrace(UObject* WorldContextObject, FVector StartLocation, FVector Direction,
	float StartRadius, float EndRadius, float Length,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore, bool bBrakeOnHit,
	EDrawDebugType::Type DrawDebugType, TArray<FHitResult>& OutHits,
	bool bIgnoreSelf, float LineThickness, FLinearColor TraceColor, FLinearColor TraceHitColor,
	float DrawTime)
{
	// Create collision query params
#pragma region Collision Query
	static const FName SphereTraceMultiName(TEXT("ConeTraceMultiForObjects"));
	FCollisionQueryParams Params(SphereTraceMultiName, SCENE_QUERY_STAT_ONLY(KismetTraceUtils), false);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		const AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			const UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}
#pragma endregion

	// Create collision object params
#pragma region Collision Object
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = CollisionObjectTraces.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel& Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}
#pragma endregion

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("MultiConeTrace: Invalid object types"));
		return false;
	}

	// Check given variables
	if (Length <= 0 || StartRadius <= 0 || EndRadius <= 0)
	{
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("MultiConeTrace: Length: %f, StartRadius: %f, EndRadus: %f, None of them can be smaller or equal to zero"));
		return false;
	}

	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("MultiConeTrace: Cone Trace Direction can't be zero"));
		return false;
	}

	// Get world
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	// Get end location since we only know the start location, direction and the length
	FVector EndLocation = StartLocation + (Length * Direction);

	// Calculate Axes
	FQuat const CapsuleRot = FRotationMatrix::MakeFromZ(Direction).ToQuat();
	FMatrix Axes = FQuatRotationTranslationMatrix(CapsuleRot, FVector::ZeroVector);

	// Right and Up vector based
	FVector RightVector = Axes.GetScaledAxis(EAxis::X);
	FVector UpVector = RightVector.Cross(Direction);

	// For the part below here see tangent calculation https://mathworld.wolfram.com/Circle-CircleTangents.html

	// Direction from perpendicular radius to center of the other sphere
	FVector PerpendicularDirection = EndLocation - (StartLocation + (FMath::Abs(StartRadius - EndRadius) * RightVector));
	// OtherSphere.Center - ((RightVector * MainSphere.W) + MainSphere.Center);

	// Get the angle of the tangent line
	double Angle = UKismetMathLibrary::DegAcos(FMath::Abs(StartRadius - EndRadius) / PerpendicularDirection.Length());
	// Correct angle since when we want to turn on axis the turn is clockwise

	// Tangent direction from the center of the sphere to the radius
	FVector TangentStartDirection = RightVector.RotateAngleAxis(-90 + Angle, UpVector);
	// Direction from the center of the sphere angled half of the tangent angle (used to determine the location and radius of the in-between spheres)
	FVector RotatedTangent = RightVector.RotateAngleAxis(Angle / 2, UpVector).GetSafeNormal();
	// Intersection point of tangent direction and the MainSphere
	FVector TangentPoint = (TangentStartDirection.GetSafeNormal() * StartRadius) + StartLocation;

	// Planes are used to calculate the location of the in-between spheres
	// Turn the tangent line ito a plane
	FPlane TangentPlane = FPlane(TangentPoint, TangentStartDirection);
	// Turn the direction between spheres to a plane
	FPlane DirectionPlane = FPlane(StartLocation, RightVector);

	// In order to get a correct impact point from multi sweep the start and end locations must be different
	// to solve this we add a small of set to the start location
	constexpr double MULTI_TRACE_OFFSET = 0.01f;
	// Offset in vector format. The direction is from start to end since we don't wanna ruin any precision by moving the spheres forward
	const FVector OffsetVector = (StartLocation - EndLocation).GetSafeNormal() * MULTI_TRACE_OFFSET;


	// The Trace Starts From The StartLocation to EndLocation
	// Sweep for the StartSphere
	CustomSweepMultiByObjectType(World, StartLocation, StartLocation + OffsetVector, StartRadius, ObjectParams, Params, OutHits);

	// Starting from the StartLocation to EndLocation, this indicates the previously created/used sphere
	FSphere PrevSphere = FSphere(StartLocation, StartRadius);

	// Crate and check for sweeps for the in-between Spheres
	while (true)
	{
		// Calculating Trace Values
		FVector TangentIntersectPoint;
		FVector DirectionIntersectPoint;

		bool bIsIntersect = FMath::SegmentPlaneIntersection(PrevSphere.Center, (RotatedTangent * 10000) + PrevSphere.Center, TangentPlane, TangentIntersectPoint);
		if (!bIsIntersect) break;

		// Rotate the RotatedTangent 90° based on the intersect point
		RotatedTangent = (PrevSphere.Center - TangentIntersectPoint).RotateAngleAxis(-90, UpVector).GetSafeNormal();

		bIsIntersect = FMath::SegmentPlaneIntersection(TangentIntersectPoint, (RotatedTangent * 10000) + TangentIntersectPoint, DirectionPlane, DirectionIntersectPoint);
		if (!bIsIntersect) break;

		FVector NewSphereCenter = DirectionIntersectPoint;
		double NewSphereRadius = (NewSphereCenter - (PrevSphere.Center + (Direction.GetSafeNormal() * PrevSphere.W))).Length();

		RotatedTangent = (TangentIntersectPoint - NewSphereCenter).RotateAngleAxis(90, UpVector).GetSafeNormal();
		PrevSphere = FSphere(NewSphereCenter, NewSphereRadius);

		if ((EndLocation - StartLocation).Length() < (NewSphereCenter - StartLocation).Length())
		{
			break;
		}

		// Use this if we wanna show segments of the cylinder
		// DrawDebugCircle(World,PrevSphere.Center,PrevSphere.W,24,FColor::Purple,false, 0.f, 0, LineThickness, RightVector, UpVector, true);

		// Actual Trace Starts Here 
		CustomSweepMultiByObjectType(World, NewSphereCenter, NewSphereCenter + OffsetVector, NewSphereRadius, ObjectParams, Params, OutHits);

		// This can be used as standard traces.
		if (OutHits.Num() > 0 && OutHits[0].bBlockingHit && bBrakeOnHit)
		{
			FHitResult const& Hit = OutHits[0];

			// Empty the outhits (We are doing this since this is a multi sweep and we might have multiple results)
			OutHits.Empty();
			// Add the first element back in
			OutHits.Add(Hit);
		}
	}

	// Sweep for the final Sphere
	CustomSweepMultiByObjectType(World, EndLocation, EndLocation + OffsetVector, EndRadius, ObjectParams, Params, OutHits);

	FColor FinalColor = (OutHits.Num() > 0 ? TraceHitColor.ToFColor(true) : TraceColor.ToFColor(true));
	bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
	float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? DrawTime : 0.f;

#if ENABLE_DRAW_DEBUG
	if (DrawDebugType != EDrawDebugTrace::None)
	{
		// Draw Hits
		for (int32 HitIdx = 0; HitIdx < OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, 15, FColor::Red, bPersistent, LifeTime);
		}

		// Draw Ends
		DrawDebugCircle(World, EndLocation, EndRadius, 24, FinalColor, bPersistent, LifeTime, 0, LineThickness, RightVector, UpVector, true);
		DrawDebugCircle(World, StartLocation, StartRadius, 24, FinalColor, bPersistent, LifeTime, 0, LineThickness, RightVector, UpVector, true);
		DrawHalfCircle(World, EndLocation, RightVector, Direction, FinalColor, EndRadius, 16, bPersistent, LifeTime, 0, LineThickness);
		DrawHalfCircle(World, EndLocation, UpVector, Direction, FinalColor, EndRadius, 16, bPersistent, LifeTime, 0, LineThickness);

		// Draw Capsule Lines
		DrawDebugLine(World, (TangentStartDirection.GetSafeNormal() * StartRadius) + StartLocation,
			(TangentStartDirection.GetSafeNormal() * EndRadius) + EndLocation,
			FinalColor, bPersistent, LifeTime, SDPG_World, LineThickness);
		DrawDebugLine(
			World,
			(TangentStartDirection.RotateAngleAxis(90, Direction).GetSafeNormal() * StartRadius) + StartLocation,
			(TangentStartDirection.RotateAngleAxis(90, Direction).GetSafeNormal() * EndRadius) + EndLocation,
			FinalColor, bPersistent, LifeTime, SDPG_World, LineThickness);
		DrawDebugLine(
			World,
			(TangentStartDirection.RotateAngleAxis(180, Direction).GetSafeNormal() * StartRadius) + StartLocation,
			(TangentStartDirection.RotateAngleAxis(180, Direction).GetSafeNormal() * EndRadius) + EndLocation,
			FinalColor, bPersistent, LifeTime, SDPG_World, LineThickness);
		DrawDebugLine(
			World,
			(TangentStartDirection.RotateAngleAxis(-90, Direction).GetSafeNormal() * StartRadius) + StartLocation,
			(TangentStartDirection.RotateAngleAxis(-90, Direction).GetSafeNormal() * EndRadius) + EndLocation,
			FinalColor, bPersistent, LifeTime, SDPG_World, LineThickness);
	}
#endif

	return (OutHits.Num() > 0);
