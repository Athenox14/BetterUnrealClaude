// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Actor.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "UnrealClaudeUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

FMCPToolResult FMCPTool_Actor::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract operation parameter
	FString Operation;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, ParamError))
	{
		return ParamError.GetValue();
	}

	// Route to appropriate handler
	if (Operation.Equals(TEXT("list"), ESearchCase::IgnoreCase))
	{
		return ExecuteList(Params);
	}
	else if (Operation.Equals(TEXT("spawn"), ESearchCase::IgnoreCase))
	{
		return ExecuteSpawn(Params);
	}
	else if (Operation.Equals(TEXT("move"), ESearchCase::IgnoreCase))
	{
		return ExecuteMove(Params);
	}
	else if (Operation.Equals(TEXT("delete"), ESearchCase::IgnoreCase))
	{
		return ExecuteDelete(Params);
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Invalid operation '%s'. Valid operations: list, spawn, move, delete"),
			*Operation
		));
	}
}

FMCPToolResult FMCPTool_Actor::ExecuteList(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Parse parameters
	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);

	// Validate filter strings (basic length check to prevent abuse)
	FString ValidationError;
	if (!ClassFilter.IsEmpty() && !FMCPParamValidator::ValidateStringLength(ClassFilter, TEXT("class_filter"), 256, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}
	if (!NameFilter.IsEmpty() && !FMCPParamValidator::ValidateStringLength(NameFilter, TEXT("name_filter"), 256, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	bool bIncludeHidden = false;
	Params->TryGetBoolField(TEXT("include_hidden"), bIncludeHidden);

	bool bBrief = ExtractOptionalBool(Params, TEXT("brief"), true);

	int32 Limit = 25;
	Params->TryGetNumberField(TEXT("limit"), Limit);
	if (Limit <= 0) Limit = 25;
	if (Limit > 1000) Limit = 1000; // Cap at 1000 for performance

	int32 Offset = 0;
	Params->TryGetNumberField(TEXT("offset"), Offset);
	if (Offset < 0) Offset = 0;

	// Collect actors
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	int32 MatchIndex = 0;  // Index among matching actors
	int32 AddedCount = 0;  // Count of actors added to result
	int32 TotalMatching = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Skip hidden actors if not requested
		if (!bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		// Apply class filter
		if (!ClassFilter.IsEmpty())
		{
			FString ActorClassName = Actor->GetClass()->GetName();
			if (!ActorClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Apply name filter
		if (!NameFilter.IsEmpty())
		{
			FString ActorName = Actor->GetName();
			FString ActorLabel = Actor->GetActorLabel();
			if (!ActorName.Contains(NameFilter, ESearchCase::IgnoreCase) &&
				!ActorLabel.Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TotalMatching++;

		// Apply offset - skip until we reach the offset
		if (MatchIndex < Offset)
		{
			MatchIndex++;
			continue;
		}

		// Apply limit - stop adding after limit reached
		if (AddedCount >= Limit)
		{
			MatchIndex++;
			continue; // Keep counting total but don't add more
		}

		// Build actor info using base class helper
		TSharedPtr<FJsonObject> ActorJson = bBrief
			? BuildActorInfoJson(Actor)
			: BuildActorInfoWithTransformJson(Actor);

		if (!bBrief)
		{
			ActorJson->SetBoolField(TEXT("hidden"), Actor->IsHidden());

			// Add tags if any
			if (Actor->Tags.Num() > 0)
			{
				TArray<FString> TagStrings;
				for (const FName& Tag : Actor->Tags)
				{
					TagStrings.Add(Tag.ToString());
				}
				ActorJson->SetArrayField(TEXT("tags"), StringArrayToJsonArray(TagStrings));
			}
		}

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		AddedCount++;
		MatchIndex++;
	}

	// Build result with pagination metadata
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("actors"), ActorsArray);
	ResultData->SetNumberField(TEXT("count"), AddedCount);
	ResultData->SetNumberField(TEXT("total"), TotalMatching);
	ResultData->SetNumberField(TEXT("offset"), Offset);
	ResultData->SetNumberField(TEXT("limit"), Limit);
	ResultData->SetBoolField(TEXT("hasMore"), (Offset + AddedCount) < TotalMatching);
	if ((Offset + AddedCount) < TotalMatching)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), Offset + AddedCount);
	}
	ResultData->SetStringField(TEXT("levelName"), World->GetMapName());

	FString Message = FString::Printf(TEXT("Found %d actors"), AddedCount);
	if (TotalMatching > AddedCount)
	{
		Message += FString::Printf(TEXT(" (showing %d-%d of %d total)"), Offset + 1, Offset + AddedCount, TotalMatching);
	}

	return FMCPToolResult::Success(Message, ResultData);
}

FMCPToolResult FMCPTool_Actor::ExecuteSpawn(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Extract and validate class path using base class helper
	FString ClassPath;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("class"), ClassPath, ParamError))
	{
		return ParamError.GetValue();
	}

	// Validate class path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateClassPath(ClassPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load the actor class using base class helper (handles fallback prefixes)
	UClass* ActorClass = LoadActorClass(ClassPath, ParamError);
	if (!ActorClass)
	{
		return ParamError.GetValue();
	}

	// Parse transform using base class helpers (consolidated transform extraction)
	FVector Location = ExtractVectorParam(Params, TEXT("location"));
	FRotator Rotation = ExtractRotatorParam(Params, TEXT("rotation"));
	FVector Scale = ExtractScaleParam(Params, TEXT("scale"));

	// Get optional name using base class helper
	FString ActorName = ExtractOptionalString(Params, TEXT("name"));

	// Validate actor name if provided
	if (!ActorName.IsEmpty())
	{
		if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
		{
			return FMCPToolResult::Error(ValidationError);
		}
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	if (!ActorName.IsEmpty())
	{
		SpawnParams.Name = FName(*ActorName);
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FTransform SpawnTransform(Rotation, Location, Scale);

	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

	if (!SpawnedActor)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to spawn actor of class: %s"), *ClassPath));
	}

	// Mark the level as dirty using base class helper
	MarkWorldDirty(World);

	// Build result using shared JSON utilities
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actorName"), SpawnedActor->GetName());
	ResultData->SetStringField(TEXT("actorClass"), ActorClass->GetName());
	ResultData->SetStringField(TEXT("actorLabel"), SpawnedActor->GetActorLabel());
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Location));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Spawned actor '%s' of class '%s'"), *SpawnedActor->GetName(), *ActorClass->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Actor::ExecuteMove(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Extract and validate actor name using base class helper
	FString ActorName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractActorName(Params, TEXT("actor_name"), ActorName, ParamError))
	{
		return ParamError.GetValue();
	}

	// Find the actor using base class helper
	AActor* Actor = FindActorByNameOrLabel(World, ActorName);
	if (!Actor)
	{
		return ActorNotFoundError(ActorName);
	}

	// Get current transform
	FVector CurrentLocation = Actor->GetActorLocation();
	FRotator CurrentRotation = Actor->GetActorRotation();
	FVector CurrentScale = Actor->GetActorScale3D();

	// Check if relative mode using base class helper
	bool bRelative = ExtractOptionalBool(Params, TEXT("relative"), false);

	// Apply new location if provided (using base class transform helpers)
	FVector NewLocation = CurrentLocation;
	bool bLocationChanged = ExtractVectorComponents(Params, TEXT("location"), NewLocation, bRelative);
	if (bLocationChanged)
	{
		Actor->SetActorLocation(NewLocation);
	}

	// Apply new rotation if provided (using base class transform helpers)
	FRotator NewRotation = CurrentRotation;
	bool bRotationChanged = ExtractRotatorComponents(Params, TEXT("rotation"), NewRotation, bRelative);
	if (bRotationChanged)
	{
		Actor->SetActorRotation(NewRotation);
	}

	// Apply new scale if provided
	// Note: Scale uses multiplicative relative mode, handled specially
	FVector NewScale = CurrentScale;
	bool bScaleChanged = false;
	if (HasVectorParam(Params, TEXT("scale")))
	{
		if (bRelative)
		{
			// Multiplicative scale for relative mode
			FVector ScaleMultiplier = ExtractVectorParam(Params, TEXT("scale"), FVector::OneVector);
			NewScale = CurrentScale * ScaleMultiplier;
		}
		else
		{
			// Absolute scale replacement
			ExtractVectorComponents(Params, TEXT("scale"), NewScale, false);
		}
		Actor->SetActorScale3D(NewScale);
		bScaleChanged = true;
	}

	// Check if anything changed
	if (!bLocationChanged && !bRotationChanged && !bScaleChanged)
	{
		return FMCPToolResult::Error(TEXT("No transform changes specified. Provide location, rotation, or scale."));
	}

	// Mark dirty using base class helper
	Actor->MarkPackageDirty();
	MarkWorldDirty(World);

	// Build result with new transform using shared utilities
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("actor"), Actor->GetName());
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorLocation()));
	ResultData->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(Actor->GetActorRotation()));
	ResultData->SetObjectField(TEXT("scale"), UnrealClaudeJsonUtils::VectorToJson(Actor->GetActorScale3D()));

	// Build change description
	TArray<FString> Changes;
	if (bLocationChanged) Changes.Add(TEXT("location"));
	if (bRotationChanged) Changes.Add(TEXT("rotation"));
	if (bScaleChanged) Changes.Add(TEXT("scale"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated %s for actor '%s'"), *FString::Join(Changes, TEXT(", ")), *Actor->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Actor::ExecuteDelete(const TSharedRef<FJsonObject>& Params)
{
	// Validate editor context using base class
	UWorld* World = nullptr;
	if (auto Error = ValidateEditorContext(World))
	{
		return Error.GetValue();
	}

	// Collect actors to delete
	TArray<AActor*> ActorsToDelete;
	TArray<FString> DeletedNames;
	TArray<FString> NotFoundNames;
	FString ValidationError;

	// Check for single actor_name
	FString SingleActorName;
	if (Params->TryGetStringField(TEXT("actor_name"), SingleActorName))
	{
		// Validate actor name
		if (!FMCPParamValidator::ValidateActorName(SingleActorName, ValidationError))
		{
			return FMCPToolResult::Error(ValidationError);
		}

		// Use base class helper to find actor
		if (AActor* Actor = FindActorByNameOrLabel(World, SingleActorName))
		{
			ActorsToDelete.Add(Actor);
			DeletedNames.Add(Actor->GetName());
		}
		else
		{
			NotFoundNames.Add(SingleActorName);
		}
	}

	// Check for actor_names array
	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray;
	if (Params->TryGetArrayField(TEXT("actor_names"), ActorNamesArray))
	{
		for (const TSharedPtr<FJsonValue>& NameValue : *ActorNamesArray)
		{
			FString ActorName;
			if (NameValue->TryGetString(ActorName))
			{
				// Validate each actor name
				if (!FMCPParamValidator::ValidateActorName(ActorName, ValidationError))
				{
					return FMCPToolResult::Error(ValidationError);
				}

				// Use base class helper to find actor
				if (AActor* Actor = FindActorByNameOrLabel(World, ActorName))
				{
					ActorsToDelete.AddUnique(Actor);
					DeletedNames.AddUnique(Actor->GetName());
				}
				else
				{
					NotFoundNames.Add(ActorName);
				}
			}
		}
	}

	// Check for class filter
	FString ClassFilter;
	if (Params->TryGetStringField(TEXT("class_filter"), ClassFilter) && !ClassFilter.IsEmpty())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor)
			{
				FString ActorClassName = Actor->GetClass()->GetName();
				if (ActorClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
				{
					ActorsToDelete.AddUnique(Actor);
					DeletedNames.AddUnique(Actor->GetName());
				}
			}
		}
	}

	// Validate we have something to delete
	if (ActorsToDelete.Num() == 0)
	{
		if (NotFoundNames.Num() > 0)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("No actors found: %s"), *FString::Join(NotFoundNames, TEXT(", "))));
		}
		return FMCPToolResult::Error(TEXT("No actors specified or found to delete. Provide actor_name, actor_names array, or class_filter."));
	}

	// Delete actors
	for (AActor* Actor : ActorsToDelete)
	{
		if (Actor && IsValid(Actor))
		{
			World->EditorDestroyActor(Actor, false);
		}
	}

	// Mark dirty using base class helper
	MarkWorldDirty(World);

	// Build result using base class helpers for JSON array construction
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("deleted"), StringArrayToJsonArray(DeletedNames));
	ResultData->SetNumberField(TEXT("count"), DeletedNames.Num());

	if (NotFoundNames.Num() > 0)
	{
		ResultData->SetArrayField(TEXT("notFound"), StringArrayToJsonArray(NotFoundNames));
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted %d actor(s)"), DeletedNames.Num()),
		ResultData
	);
}
