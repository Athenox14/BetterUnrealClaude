// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: search_nodes
 *
 * Dynamically discover Blueprint nodes, their pins, and function libraries.
 * Read-only tool for introspection - does not modify anything.
 *
 * Operations:
 * - search: Find nodes matching a keyword
 * - get_node_pins: Get exact pin layout for a specific function
 * - list_libraries: List all loaded UBlueprintFunctionLibrary classes
 * - list_library_functions: List all functions in a specific library
 */
class FMCPTool_SearchNodes : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("search_nodes");
		Info.Description = TEXT("Discover Blueprint nodes and their pin layouts. "
			"Operations: 'search' (find nodes by keyword), 'get_node_pins' (exact pin layout for a function), "
			"'list_libraries' (all loaded function libraries), 'list_library_functions' (functions in a library). "
			"Use this to find the correct node types and pin names before creating Blueprint nodes.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'search', 'get_node_pins', 'list_libraries', 'list_library_functions'"), true),
			FMCPToolParameter(TEXT("keyword"), TEXT("string"),
				TEXT("Search keyword for 'search' operation (e.g., 'Print', 'Delay', 'MoveTo')"), false),
			FMCPToolParameter(TEXT("function_reference"), TEXT("string"),
				TEXT("Function reference for 'get_node_pins' (e.g., 'KismetSystemLibrary::Delay', 'GameplayStatics::GetPlayerController')"), false),
			FMCPToolParameter(TEXT("library_class"), TEXT("string"),
				TEXT("Library class name for 'list_library_functions' (e.g., 'KismetSystemLibrary', 'GameplayStatics')"), false),
			FMCPToolParameter(TEXT("category_filter"), TEXT("string"),
				TEXT("Optional category filter for 'search' operation (e.g., 'Math', 'String', 'Utilities')"), false),
			FMCPToolParameter(TEXT("include_k2_nodes"), TEXT("boolean"),
				TEXT("Include internal K2 nodes in search results (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("include_project_functions"), TEXT("boolean"),
				TEXT("Include project Blueprint functions in search results (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("max_results"), TEXT("number"),
				TEXT("Maximum number of results to return (default: 50, max: 500)"), false, TEXT("50")),
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult HandleSearch(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleGetNodePins(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleListLibraries(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleListLibraryFunctions(const TSharedRef<FJsonObject>& Params);
};
