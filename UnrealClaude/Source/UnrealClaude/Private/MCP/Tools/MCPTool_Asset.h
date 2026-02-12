// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * MCP Tool for generic asset operations.
 *
 * Provides operations for:
 * - Setting properties on any loaded asset using reflection
 * - Saving assets to disk
 * - Getting asset information
 * - Searching for assets
 * - Querying asset relationships
 *
 * This tool works with assets in the Content Browser, not actors in a level.
 * Use set_property for actor properties, use this tool for asset properties.
 *
 * Operations:
 * - set_asset_property: Set a property on an asset using reflection
 * - save_asset: Save an asset to disk (mark dirty and/or save)
 * - get_asset_info: Get information about an asset
 * - search: Search for assets by class, path, or name pattern
 * - dependencies: Get assets that a specific asset depends on
 * - referencers: Get assets that reference a specific asset
 */
class FMCPTool_Asset : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override;
	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Operation handlers
	FMCPToolResult ExecuteSetAssetProperty(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSaveAsset(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetAssetInfo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSearch(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteDependencies(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteReferencers(const TSharedRef<FJsonObject>& Params);

	// Property reflection helpers (adapted from SetProperty tool)
	bool NavigateToProperty(
		UObject* StartObject,
		const TArray<FString>& PathParts,
		UObject*& OutObject,
		FProperty*& OutProperty,
		FString& OutError);

	bool SetPropertyFromJson(
		UObject* Object,
		const FString& PropertyPath,
		const TSharedPtr<FJsonValue>& Value,
		FString& OutError);

	bool SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);
	bool SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);
	bool SetObjectPropertyValue(FObjectProperty* ObjProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutError);

	// Asset loading
	UObject* LoadAssetByPath(const FString& AssetPath, FString& OutError);

	// Utility
	TSharedPtr<FJsonObject> BuildAssetInfoJson(UObject* Asset);
	TArray<TSharedPtr<FJsonValue>> GetAssetProperties(UObject* Asset, bool bEditableOnly);
	TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData) const;
	FMCPToolResult ExecuteRelations(const TSharedRef<FJsonObject>& Params, bool bIsDependencies);
};
