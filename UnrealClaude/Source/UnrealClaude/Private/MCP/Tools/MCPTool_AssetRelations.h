// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: asset_relations
 *
 * Query asset dependency relationships in both directions.
 * Replaces asset_dependencies and asset_referencers.
 *
 * Operations:
 * - dependencies: Get assets that a specific asset depends on
 * - referencers: Get assets that reference a specific asset
 */
class FMCPTool_AssetRelations : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("asset_relations");
		Info.Description = TEXT(
			"Query asset dependency relationships. "
			"Operations: 'dependencies' (what this asset needs), "
			"'referencers' (what uses this asset). "
			"Useful for impact analysis and understanding asset relationships.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'dependencies' or 'referencers'"), true),
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("Full asset path (e.g., '/Game/Blueprints/BP_Player')"), true),
			FMCPToolParameter(TEXT("include_soft"), TEXT("boolean"),
				TEXT("Include soft references (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Maximum results (1-1000, default: 25)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"),
				TEXT("Pagination offset (default: 0)"), false, TEXT("0"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
