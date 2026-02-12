// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Query Blueprint information (read-only operations)
 *
 * Operations:
 *   - list: List all Blueprints in project (with optional filters)
 *   - inspect: Get detailed Blueprint info (variables, functions, parent class)
 *   - get_graph: Get graph information (node count, events)
 */
class FMCPTool_BlueprintQuery : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("blueprint_query");
		Info.Description = TEXT(
			"Query Blueprint information and discover nodes (read-only).\n\n"
			"Operations:\n"
			"- 'list': Find Blueprints in project with optional filters\n"
			"- 'inspect': Get detailed Blueprint info (variables, functions, parent class)\n"
			"- 'get_graph': Get graph structure (node count, events, connections)\n"
			"- 'search': Find Blueprint nodes matching a keyword\n"
			"- 'get_node_pins': Get exact pin layout for a specific function\n"
			"- 'list_libraries': List all loaded UBlueprintFunctionLibrary classes\n"
			"- 'list_library_functions': List all functions in a specific library\n\n"
			"Use 'list' to discover Blueprints, 'search' to find nodes before adding them.\n\n"
			"Example paths:\n"
			"- '/Game/Blueprints/BP_Character'\n"
			"- '/Game/UI/WBP_MainMenu'\n"
			"- '/Game/Characters/ABP_Hero' (Animation Blueprint)\n\n"
			"Returns: Blueprint metadata, variables, functions, graph structure, or node information."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'list', 'inspect', 'get_graph', 'search', 'get_node_pins', 'list_libraries', 'list_library_functions'"), true),
			FMCPToolParameter(TEXT("path_filter"), TEXT("string"),
				TEXT("Path prefix filter (e.g., '/Game/Blueprints/')"), false, TEXT("/Game/")),
			FMCPToolParameter(TEXT("type_filter"), TEXT("string"),
				TEXT("Blueprint type filter: 'Actor', 'Object', 'Widget', 'AnimBlueprint', etc."), false),
			FMCPToolParameter(TEXT("name_filter"), TEXT("string"),
				TEXT("Name substring filter"), false),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum results to return (1-1000, default: 25)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Full Blueprint asset path (required for inspect/get_graph)"), false),
			FMCPToolParameter(TEXT("include_variables"), TEXT("boolean"),
				TEXT("Include variable list in inspect result (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("include_functions"), TEXT("boolean"),
				TEXT("Include function list in inspect result (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("include_graphs"), TEXT("boolean"),
				TEXT("Include graph info in inspect result"), false, TEXT("false")),
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
				TEXT("Maximum number of results to return for 'search' (default: 50, max: 500)"), false, TEXT("50"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** List Blueprints matching filters */
	FMCPToolResult ExecuteList(const TSharedRef<FJsonObject>& Params);

	/** Get detailed Blueprint info */
	FMCPToolResult ExecuteInspect(const TSharedRef<FJsonObject>& Params);

	/** Get graph information */
	FMCPToolResult ExecuteGetGraph(const TSharedRef<FJsonObject>& Params);

	/** Search for nodes matching a keyword */
	FMCPToolResult HandleSearch(const TSharedRef<FJsonObject>& Params);

	/** Get exact pin layout for a function */
	FMCPToolResult HandleGetNodePins(const TSharedRef<FJsonObject>& Params);

	/** List all loaded function libraries */
	FMCPToolResult HandleListLibraries(const TSharedRef<FJsonObject>& Params);

	/** List all functions in a specific library */
	FMCPToolResult HandleListLibraryFunctions(const TSharedRef<FJsonObject>& Params);
};
