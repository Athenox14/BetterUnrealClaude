// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

/**
 * Information about a single pin on a Blueprint node/function
 */
struct FNodePinInfo
{
	FString Name;
	FString Direction;  // "Input" or "Output"
	FString Type;       // "bool", "float", "int", "string", "Vector", "exec", etc.
	FString DefaultValue;
	FString SubCategory; // For struct/object types
	bool bIsExec = false;

	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Search result for a Blueprint node/function
 */
struct FNodeSearchResult
{
	FString DisplayName;
	FString ClassName;
	FString FunctionName;
	FString Category;
	FString NodeType;     // "Function", "PureFunction", "K2Node", "Event", "Macro"
	FString Module;
	FString FullReference; // "ClassName::FunctionName"
	bool bIsPure = false;
	TArray<FNodePinInfo> Pins;

	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Common Blueprint function library classes
 * Centralized list to avoid duplication across multiple files
 */
struct UNREALCLAUDE_API FCommonLibraryClasses
{
	struct FLibraryEntry
	{
		const TCHAR* Name;
		UClass* (*GetClass)();
	};

	static TArray<FLibraryEntry> GetAll();
};

/**
 * Blueprint node discovery and introspection
 *
 * Scans loaded UBlueprintFunctionLibrary classes and other Blueprint-callable
 * functions to discover available nodes, extract pin layouts, and provide
 * search functionality for Blueprint node discovery.
 *
 * Used by the search_nodes MCP tool.
 */
class UNREALCLAUDE_API FBlueprintNodeSearcher
{
public:
	/**
	 * Search for Blueprint nodes matching a keyword
	 * @param Keyword - Search term to match against function/class names
	 * @param CategoryFilter - Optional category filter
	 * @param bIncludeK2Nodes - Include K2 internal nodes
	 * @param bIncludeProjectFunctions - Include project Blueprint functions
	 * @param MaxResults - Maximum results to return
	 * @return Array of matching search results
	 */
	static TArray<FNodeSearchResult> SearchNodes(
		const FString& Keyword,
		const FString& CategoryFilter = FString(),
		bool bIncludeK2Nodes = false,
		bool bIncludeProjectFunctions = true,
		int32 MaxResults = 50
	);

	/**
	 * Get exact pin layout for a specific function reference
	 * @param FunctionReference - "ClassName::FunctionName" format
	 * @return Search result with complete pin info, or empty if not found
	 */
	static TOptional<FNodeSearchResult> GetNodePinLayout(const FString& FunctionReference);

	/**
	 * List all loaded UBlueprintFunctionLibrary classes
	 * @return Array of library class names with metadata
	 */
	static TArray<TSharedPtr<FJsonObject>> ListFunctionLibraries();

	/**
	 * List all Blueprint-callable functions in a specific library class
	 * @param ClassName - Name of the UBlueprintFunctionLibrary subclass
	 * @return Array of search results for each function
	 */
	static TArray<FNodeSearchResult> ListLibraryFunctions(const FString& ClassName);

	/**
	 * Search for Event nodes by scanning multicast delegate properties
	 * Discovers events dynamically without hardcoding (BeginPlay, Overlap, etc.)
	 * @param Keyword - Search term to match event names
	 * @param BaseClassName - Base class to scan (e.g., "Actor", "ActorComponent", empty for all)
	 * @param MaxResults - Maximum results to return
	 * @return Array of Event node search results
	 */
	static TArray<FNodeSearchResult> SearchEventNodes(
		const FString& Keyword = FString(),
		const FString& BaseClassName = TEXT("Actor"),
		int32 MaxResults = 50
	);

	/**
	 * Find a single node by keyword (convenience wrapper around SearchNodes)
	 * Useful for quick lookups when you know the function name
	 * @param Keyword - Search term to match
	 * @param MaxResults - Maximum results to consider (defaults to 10)
	 * @return First matching result, or empty if none found
	 */
	static TOptional<FNodeSearchResult> FindNodeForKeyword(
		const FString& Keyword,
		int32 MaxResults = 10
	);

	/**
	 * Invalidate the internal cache (call on hot-reload)
	 */
	static void InvalidateCache();

private:
	/**
	 * Extract pin information from a UFunction
	 * @param Function - The function to extract pins from
	 * @return Array of pin info
	 */
	static TArray<FNodePinInfo> ExtractFunctionPins(UFunction* Function);

	/**
	 * Convert an FProperty to a pin type string
	 * @param Property - The property to convert
	 * @return Type string (bool, float, int, string, Vector, Object, etc.)
	 */
	static FString PropertyToPinType(FProperty* Property);

	/**
	 * Get subcategory info for a property (struct name, class name, etc.)
	 * @param Property - The property to inspect
	 * @return Subcategory string
	 */
	static FString PropertyToSubCategory(FProperty* Property);

	/**
	 * Resolve a class by name with multiple strategies
	 * @param ClassName - Class name to resolve
	 * @return UClass* or nullptr
	 */
	static UClass* ResolveClass(const FString& ClassName);

	/**
	 * Build a search result from a UFunction
	 * @param Function - The function
	 * @param OwnerClass - The owning class
	 * @return Populated search result
	 */
	static FNodeSearchResult BuildResultFromFunction(UFunction* Function, UClass* OwnerClass);

	/**
	 * Check if a function is Blueprint-callable
	 */
	static bool IsBlueprintCallable(UFunction* Function);

	// Cache
	static TMap<FString, TArray<FNodeSearchResult>> CachedLibraryFunctions;
	static bool bCacheValid;
};
