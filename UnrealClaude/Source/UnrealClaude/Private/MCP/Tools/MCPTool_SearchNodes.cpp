// Copyright Natali Caggiano. All Rights Reserved.

#include "Tools/MCPTool_SearchNodes.h"
#include "BlueprintNodeSearcher.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_SearchNodes::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("search"))
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

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid operations: 'search', 'get_node_pins', 'list_libraries', 'list_library_functions'"),
		*Operation));
}

FMCPToolResult FMCPTool_SearchNodes::HandleSearch(const TSharedRef<FJsonObject>& Params)
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

	// Build response
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

FMCPToolResult FMCPTool_SearchNodes::HandleGetNodePins(const TSharedRef<FJsonObject>& Params)
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

FMCPToolResult FMCPTool_SearchNodes::HandleListLibraries(const TSharedRef<FJsonObject>& Params)
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

FMCPToolResult FMCPTool_SearchNodes::HandleListLibraryFunctions(const TSharedRef<FJsonObject>& Params)
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
