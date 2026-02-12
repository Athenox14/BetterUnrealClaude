// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_BlueprintQuery.h"
#include "BlueprintUtils.h"
#include "BlueprintNodeSearcher.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"

// Blueprint metadata cache (TTL: 30 seconds)
namespace
{
	struct FBlueprintListCache
	{
		TArray<FAssetData> CachedAssets;
		FDateTime LastRefresh;
		static constexpr double CacheTTL = 30.0; // seconds

		bool IsValid() const
		{
			return (FDateTime::Now() - LastRefresh).GetTotalSeconds() < CacheTTL;
		}

		void Refresh(const TArray<FAssetData>& NewAssets)
		{
			CachedAssets = NewAssets;
			LastRefresh = FDateTime::Now();
		}

		void Invalidate()
		{
			CachedAssets.Empty();
		}
	};

	FBlueprintListCache GBlueprintListCache;
}

FMCPToolResult FMCPTool_BlueprintQuery::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Get operation type
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("list"))
	{
		return ExecuteList(Params);
	}
	else if (Operation == TEXT("inspect"))
	{
		return ExecuteInspect(Params);
	}
	else if (Operation == TEXT("get_graph"))
	{
		return ExecuteGetGraph(Params);
	}
	else if (Operation == TEXT("search"))
	{
		return HandleSearch(Params);
	}
	else if (Operation == TEXT("get_node_pins"))
	{
		return HandleGetNodePins(Params);
	}
	else if (Operation == TEXT("list_libraries"))
	{
		return HandleListLibraries(Params);
	}
	else if (Operation == TEXT("list_library_functions"))
	{
		return HandleListLibraryFunctions(Params);
	}
	else if (Operation == TEXT("search_events"))
	{
		return HandleSearchEvents(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid operations: 'list', 'inspect', 'get_graph', 'search', 'get_node_pins', 'list_libraries', 'list_library_functions', 'search_events'"), *Operation));
}

FMCPToolResult FMCPTool_BlueprintQuery::ExecuteList(const TSharedRef<FJsonObject>& Params)
{
	// Extract filters
	FString PathFilter = ExtractOptionalString(Params, TEXT("path_filter"), TEXT("/Game/"));
	FString TypeFilter = ExtractOptionalString(Params, TEXT("type_filter"));
	FString NameFilter = ExtractOptionalString(Params, TEXT("name_filter"));
	int32 Limit = ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25);

	// Clamp limit
	Limit = FMath::Clamp(Limit, 1, 1000);

	// Validate path filter
	FString ValidationError;
	if (!PathFilter.IsEmpty() && !FMCPParamValidator::ValidateBlueprintPath(PathFilter, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Try cache first (only if no filters applied)
	TArray<FAssetData> AssetDataList;
	bool bUseCache = PathFilter == TEXT("/Game/") && TypeFilter.IsEmpty() && NameFilter.IsEmpty();

	if (bUseCache && GBlueprintListCache.IsValid())
	{
		AssetDataList = GBlueprintListCache.CachedAssets;
		UE_LOG(LogUnrealClaude, Verbose, TEXT("Blueprint list cache hit"));
	}
	else
	{
		// Query AssetRegistry
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Build filter
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		if (!PathFilter.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*PathFilter));
		}

		// Get assets
		AssetRegistry.GetAssets(Filter, AssetDataList);

		// Cache if no filters
		if (bUseCache)
		{
			GBlueprintListCache.Refresh(AssetDataList);
			UE_LOG(LogUnrealClaude, Verbose, TEXT("Blueprint list cache refreshed (%d assets)"), AssetDataList.Num());
		}
	}

	// Process results
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 Count = 0;
	int32 TotalMatching = 0;

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Get parent class name for filtering
		FString ParentClassName;
		FAssetDataTagMapSharedView::FFindTagResult ParentClassTag = AssetData.TagsAndValues.FindTag(FName("ParentClass"));
		if (ParentClassTag.IsSet())
		{
			ParentClassName = ParentClassTag.GetValue();
		}

		// Apply type filter
		if (!TypeFilter.IsEmpty())
		{
			if (!ParentClassName.Contains(TypeFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Apply name filter
		if (!NameFilter.IsEmpty())
		{
			if (!AssetData.AssetName.ToString().Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TotalMatching++;

		// Check limit
		if (Count >= Limit)
		{
			continue;
		}

		// Get Blueprint type
		FString BlueprintType = TEXT("Normal");
		FAssetDataTagMapSharedView::FFindTagResult TypeTag = AssetData.TagsAndValues.FindTag(FName("BlueprintType"));
		if (TypeTag.IsSet())
		{
			BlueprintType = TypeTag.GetValue();
		}

		// Build result object
		TSharedPtr<FJsonObject> BPJson = MakeShared<FJsonObject>();
		BPJson->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		BPJson->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		BPJson->SetStringField(TEXT("blueprint_type"), BlueprintType);

		// Clean up parent class name (remove prefix)
		if (!ParentClassName.IsEmpty())
		{
			FString CleanParentName = ParentClassName;
			int32 LastDotIndex;
			if (CleanParentName.FindLastChar(TEXT('.'), LastDotIndex))
			{
				CleanParentName = CleanParentName.Mid(LastDotIndex + 1);
			}
			// Remove trailing '_C' from generated class names
			if (CleanParentName.EndsWith(TEXT("_C")))
			{
				CleanParentName = CleanParentName.LeftChop(2);
			}
			BPJson->SetStringField(TEXT("parent_class"), CleanParentName);
		}

		ResultsArray.Add(MakeShared<FJsonValueObject>(BPJson));
		Count++;
	}

	// Build response
	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("blueprints"), ResultsArray);
	ResponseData->SetNumberField(TEXT("count"), Count);
	ResponseData->SetNumberField(TEXT("total_matching"), TotalMatching);

	if (TotalMatching > Count)
	{
		ResponseData->SetBoolField(TEXT("truncated"), true);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d Blueprints (showing %d)"), TotalMatching, Count),
		ResponseData
	);
}

FMCPToolResult FMCPTool_BlueprintQuery::ExecuteInspect(const TSharedRef<FJsonObject>& Params)
{
	// Get Blueprint path
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Get options
	bool bIncludeVariables = ExtractOptionalBool(Params, TEXT("include_variables"), false);
	bool bIncludeFunctions = ExtractOptionalBool(Params, TEXT("include_functions"), false);
	bool bIncludeGraphs = ExtractOptionalBool(Params, TEXT("include_graphs"), false);

	// Serialize Blueprint info
	TSharedPtr<FJsonObject> BlueprintInfo = FBlueprintUtils::SerializeBlueprintInfo(
		Blueprint,
		bIncludeVariables,
		bIncludeFunctions,
		bIncludeGraphs
	);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Blueprint info for: %s"), *Blueprint->GetName()),
		BlueprintInfo
	);
}

FMCPToolResult FMCPTool_BlueprintQuery::ExecuteGetGraph(const TSharedRef<FJsonObject>& Params)
{
	// Get Blueprint path
	FString BlueprintPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return Error.GetValue();
	}

	// Validate path
	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(BlueprintPath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	// Load Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FBlueprintUtils::LoadBlueprint(BlueprintPath, LoadError);
	if (!Blueprint)
	{
		return FMCPToolResult::Error(LoadError);
	}

	// Get graph info
	TSharedPtr<FJsonObject> GraphInfo = FBlueprintUtils::GetGraphInfo(Blueprint);

	// Add Blueprint name for context
	GraphInfo->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	GraphInfo->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Graph info for: %s"), *Blueprint->GetName()),
		GraphInfo
	);
}

// ===== Node Discovery Operations =====

FMCPToolResult FMCPTool_BlueprintQuery::HandleSearch(const TSharedRef<FJsonObject>& Params)
{
	FString Keyword = ExtractOptionalString(Params, TEXT("keyword"));
	if (Keyword.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'keyword' parameter is required for 'search' operation"));
	}

	FString CategoryFilter = ExtractOptionalString(Params, TEXT("category_filter"));
	bool bIncludeK2Nodes = ExtractOptionalBool(Params, TEXT("include_k2_nodes"), false);
	bool bIncludeProjectFunctions = ExtractOptionalBool(Params, TEXT("include_project_functions"), true);
	int32 MaxResults = ExtractOptionalNumber<int32>(Params, TEXT("max_results"), 50);

	TArray<FNodeSearchResult> Results = FBlueprintNodeSearcher::SearchNodes(
		Keyword, CategoryFilter, bIncludeK2Nodes, bIncludeProjectFunctions, MaxResults);

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FNodeSearchResult& Result : Results)
	{
		ResultsArray.Add(MakeShared<FJsonValueObject>(Result.ToJson()));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("nodes"), ResultsArray);
	ResponseData->SetNumberField(TEXT("count"), Results.Num());
	ResponseData->SetStringField(TEXT("keyword"), Keyword);

	if (!CategoryFilter.IsEmpty())
	{
		ResponseData->SetStringField(TEXT("category_filter"), CategoryFilter);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d nodes matching '%s'"), Results.Num(), *Keyword),
		ResponseData);
}

FMCPToolResult FMCPTool_BlueprintQuery::HandleGetNodePins(const TSharedRef<FJsonObject>& Params)
{
	FString FunctionReference = ExtractOptionalString(Params, TEXT("function_reference"));
	if (FunctionReference.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'function_reference' parameter is required for 'get_node_pins' operation. "
			"Format: 'ClassName::FunctionName' (e.g., 'KismetSystemLibrary::Delay')"));
	}

	TOptional<FNodeSearchResult> Result = FBlueprintNodeSearcher::GetNodePinLayout(FunctionReference);
	if (!Result.IsSet())
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Function '%s' not found. Try using 'search' operation first to find the correct reference."),
			*FunctionReference));
	}

	TSharedPtr<FJsonObject> ResponseData = Result.GetValue().ToJson();

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found pin layout for '%s' (%d pins)"),
			*FunctionReference, Result.GetValue().Pins.Num()),
		ResponseData);
}

FMCPToolResult FMCPTool_BlueprintQuery::HandleListLibraries(const TSharedRef<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonObject>> Libraries = FBlueprintNodeSearcher::ListFunctionLibraries();

	TArray<TSharedPtr<FJsonValue>> LibArray;
	for (const TSharedPtr<FJsonObject>& Lib : Libraries)
	{
		LibArray.Add(MakeShared<FJsonValueObject>(Lib));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("libraries"), LibArray);
	ResponseData->SetNumberField(TEXT("count"), Libraries.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d function libraries"), Libraries.Num()),
		ResponseData);
}

FMCPToolResult FMCPTool_BlueprintQuery::HandleListLibraryFunctions(const TSharedRef<FJsonObject>& Params)
{
	FString LibraryClass = ExtractOptionalString(Params, TEXT("library_class"));
	if (LibraryClass.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'library_class' parameter is required for 'list_library_functions' operation. "
			"Use 'list_libraries' to find available library class names."));
	}

	TArray<FNodeSearchResult> Functions = FBlueprintNodeSearcher::ListLibraryFunctions(LibraryClass);

	if (Functions.Num() == 0)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("No Blueprint-callable functions found in class '%s'. Check the class name is correct."),
			*LibraryClass));
	}

	TArray<TSharedPtr<FJsonValue>> FuncArray;
	for (const FNodeSearchResult& Func : Functions)
	{
		FuncArray.Add(MakeShared<FJsonValueObject>(Func.ToJson()));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("library_class"), LibraryClass);
	ResponseData->SetArrayField(TEXT("functions"), FuncArray);
	ResponseData->SetNumberField(TEXT("count"), Functions.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d functions in '%s'"), Functions.Num(), *LibraryClass),
		ResponseData);
}

FMCPToolResult FMCPTool_BlueprintQuery::HandleSearchEvents(const TSharedRef<FJsonObject>& Params)
{
	FString Keyword = ExtractOptionalString(Params, TEXT("keyword"));
	FString BaseClass = ExtractOptionalString(Params, TEXT("base_class"), TEXT("Actor"));
	int32 MaxResults = ExtractOptionalNumber<int32>(Params, TEXT("max_results"), 50);

	TArray<FNodeSearchResult> Results = FBlueprintNodeSearcher::SearchEventNodes(
		Keyword, BaseClass, MaxResults);

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FNodeSearchResult& Result : Results)
	{
		ResultsArray.Add(MakeShared<FJsonValueObject>(Result.ToJson()));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("events"), ResultsArray);
	ResponseData->SetNumberField(TEXT("count"), Results.Num());
	if (!Keyword.IsEmpty())
	{
		ResponseData->SetStringField(TEXT("keyword"), Keyword);
	}
	ResponseData->SetStringField(TEXT("base_class"), BaseClass);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d events"), Results.Num()),
		ResponseData
	);
}
