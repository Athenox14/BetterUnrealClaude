// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Consolidated actor operations
 * Handles: list, spawn, move, delete
 */
class FMCPTool_Actor : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("actor");
		Info.Description = TEXT(
			"Consolidated actor management tool for Unreal Engine.\n\n"
			"Operations:\n"
			"- list: Query actors in the current level with optional filtering\n"
			"- spawn: Create a new actor in the level\n"
			"- move: Transform an actor's location, rotation, and/or scale\n"
			"- delete: Remove actors from the level (destructive)\n\n"
			"Use the 'operation' parameter to specify which action to perform."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"), TEXT("Operation to perform: 'list', 'spawn', 'move', or 'delete'"), true),

			// LIST parameters
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"), TEXT("[list] Filter actors by class name (e.g., 'StaticMeshActor', 'PointLight')"), false),
			FMCPToolParameter(TEXT("name_filter"), TEXT("string"), TEXT("[list] Filter actors by name substring"), false),
			FMCPToolParameter(TEXT("include_hidden"), TEXT("boolean"), TEXT("[list] Include hidden actors in results"), false, TEXT("false")),
			FMCPToolParameter(TEXT("brief"), TEXT("boolean"), TEXT("[list] Return brief info (name/label/class only). Set false for full transform data"), false, TEXT("true")),
			FMCPToolParameter(TEXT("limit"), TEXT("number"), TEXT("[list] Maximum number of actors to return (1-1000)"), false, TEXT("25")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"), TEXT("[list] Number of actors to skip for pagination"), false, TEXT("0")),

			// SPAWN parameters
			FMCPToolParameter(TEXT("class"), TEXT("string"), TEXT("[spawn] Actor class path (e.g., '/Script/Engine.PointLight' or 'StaticMeshActor')"), false),
			FMCPToolParameter(TEXT("name"), TEXT("string"), TEXT("[spawn] Optional name for the spawned actor"), false),
			FMCPToolParameter(TEXT("location"), TEXT("object"), TEXT("[spawn/move] Location {x, y, z}"), false, TEXT("{\"x\":0,\"y\":0,\"z\":0}")),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"), TEXT("[spawn/move] Rotation {pitch, yaw, roll}"), false, TEXT("{\"pitch\":0,\"yaw\":0,\"roll\":0}")),
			FMCPToolParameter(TEXT("scale"), TEXT("object"), TEXT("[spawn/move] Scale {x, y, z}"), false, TEXT("{\"x\":1,\"y\":1,\"z\":1}")),

			// MOVE parameters
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("[move/delete] Actor name to transform or delete"), false),
			FMCPToolParameter(TEXT("relative"), TEXT("boolean"), TEXT("[move] If true, values are added to current transform"), false, TEXT("false")),

			// DELETE parameters
			FMCPToolParameter(TEXT("actor_names"), TEXT("array"), TEXT("[delete] Array of actor names to delete"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteList(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSpawn(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteMove(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteDelete(const TSharedRef<FJsonObject>& Params);
};
