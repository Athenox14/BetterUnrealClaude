// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

// Forward declarations
class ACharacter;
class UCharacterMovementComponent;
class UCharacterConfigDataAsset;
class UDataTable;
struct FCharacterStatsRow;

/**
 * MCP Tool: Character Management
 *
 * Query and modify ACharacter actors in the current level.
 * Provides access to movement parameters, skeletal mesh, animation, and components.
 * Also handles character configuration DataAssets and stats DataTables.
 *
 * Character Query Operations:
 *   - list_characters: Find all ACharacter actors with optional filtering
 *   - get_character_info: Get detailed character info (mesh, anim, transform)
 *   - get_movement_params: Query CharacterMovementComponent properties
 *   - get_components: List all components attached to a character
 *
 * Character Modify Operations:
 *   - set_movement_params: Modify movement values (speed, jump, friction, etc.)
 *
 * DataAsset Operations:
 *   - create_character_data: Create new UCharacterConfigDataAsset
 *   - query_character_data: Search character config assets
 *   - get_character_data: Get details of a specific config
 *   - update_character_data: Modify existing config
 *
 * DataTable Operations:
 *   - create_stats_table: Create new stats DataTable
 *   - query_stats_table: Get stats from DataTable
 *   - add_stats_row: Add row to stats table
 *   - update_stats_row: Modify existing row
 *   - remove_stats_row: Delete row from table
 *
 * Application:
 *   - apply_character_data: Apply config to a runtime character
 *
 * All character actors are identified by name or label.
 */
class FMCPTool_Character : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("character");
		Info.Description = TEXT(
			"Character management: runtime actors and data assets. "
			"Ops: list/get/set character info/movement/components, create/query/update character data/stats table, apply config. "
			"Ex: set_movement_params with max_walk_speed=600, jump_z_velocity=420."
		);
		Info.Parameters = {
			// Operation selector
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform (see description)"), true),

			// Character identification
			FMCPToolParameter(TEXT("character_name"), TEXT("string"),
				TEXT("Character actor name or label (required for single-character ops)"), false),

			// For list_characters filtering
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"),
				TEXT("Filter by character class name (e.g., 'BP_PlayerCharacter')"), false),
			FMCPToolParameter(TEXT("limit"), TEXT("number"),
				TEXT("Max results to return (default: 100)"), false, TEXT("100")),
			FMCPToolParameter(TEXT("offset"), TEXT("number"),
				TEXT("Skip first N results (default: 0)"), false, TEXT("0")),

			// Movement parameter modifications
			FMCPToolParameter(TEXT("max_walk_speed"), TEXT("number"),
				TEXT("Maximum walking speed (cm/s)"), false),
			FMCPToolParameter(TEXT("max_acceleration"), TEXT("number"),
				TEXT("Maximum acceleration (cm/s^2)"), false),
			FMCPToolParameter(TEXT("ground_friction"), TEXT("number"),
				TEXT("Ground friction coefficient"), false),
			FMCPToolParameter(TEXT("jump_z_velocity"), TEXT("number"),
				TEXT("Initial jump velocity (cm/s)"), false),
			FMCPToolParameter(TEXT("air_control"), TEXT("number"),
				TEXT("Air control factor (0.0-1.0)"), false),
			FMCPToolParameter(TEXT("gravity_scale"), TEXT("number"),
				TEXT("Gravity multiplier"), false),
			FMCPToolParameter(TEXT("max_step_height"), TEXT("number"),
				TEXT("Maximum step height (cm)"), false),
			FMCPToolParameter(TEXT("walkable_floor_angle"), TEXT("number"),
				TEXT("Max floor angle for walking (degrees)"), false),
			FMCPToolParameter(TEXT("braking_deceleration_walking"), TEXT("number"),
				TEXT("Braking deceleration when walking (cm/s^2)"), false),
			FMCPToolParameter(TEXT("braking_friction"), TEXT("number"),
				TEXT("Braking friction coefficient"), false),

			// For get_components filtering
			FMCPToolParameter(TEXT("component_class"), TEXT("string"),
				TEXT("Filter components by class name"), false),

			// DataAsset fields (from CharacterData tool)
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("[data] Package path for new assets (default: '/Game/Characters')"), false, TEXT("/Game/Characters")),
			FMCPToolParameter(TEXT("asset_name"), TEXT("string"),
				TEXT("[data] Name for new asset"), false),
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("[data] Full path to existing asset"), false),
			FMCPToolParameter(TEXT("table_path"), TEXT("string"),
				TEXT("[data] Path to stats DataTable"), false),
			FMCPToolParameter(TEXT("config_id"), TEXT("string"),
				TEXT("[data] Unique config identifier"), false),
			FMCPToolParameter(TEXT("display_name"), TEXT("string"),
				TEXT("[data] Display name for config"), false),
			FMCPToolParameter(TEXT("description"), TEXT("string"),
				TEXT("[data] Config description"), false),
			FMCPToolParameter(TEXT("skeletal_mesh"), TEXT("string"),
				TEXT("[data] Path to skeletal mesh asset"), false),
			FMCPToolParameter(TEXT("anim_blueprint"), TEXT("string"),
				TEXT("[data] Path to animation blueprint class"), false),
			FMCPToolParameter(TEXT("is_player_character"), TEXT("boolean"),
				TEXT("[data] Whether this is a player character config"), false),
			FMCPToolParameter(TEXT("apply_movement"), TEXT("boolean"),
				TEXT("[apply] Apply movement stats (default: true)"), false, TEXT("true")),
			FMCPToolParameter(TEXT("apply_mesh"), TEXT("boolean"),
				TEXT("[apply] Apply skeletal mesh (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("apply_anim"), TEXT("boolean"),
				TEXT("[apply] Apply animation blueprint (default: false)"), false, TEXT("false"))
		};
		// Mixed: query ops are read-only, set_movement_params is modifying
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Character Actor operation handlers
	FMCPToolResult ExecuteListCharacters(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetCharacterInfo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetMovementParams(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteSetMovementParams(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetComponents(const TSharedRef<FJsonObject>& Params);

	// DataAsset operation handlers (from CharacterData)
	FMCPToolResult ExecuteCreateCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQueryCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetCharacterData(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteUpdateCharacterData(const TSharedRef<FJsonObject>& Params);

	// DataTable operation handlers (from CharacterData)
	FMCPToolResult ExecuteCreateStatsTable(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteQueryStatsTable(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteAddStatsRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteUpdateStatsRow(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteRemoveStatsRow(const TSharedRef<FJsonObject>& Params);

	// Application (from CharacterData)
	FMCPToolResult ExecuteApplyCharacterData(const TSharedRef<FJsonObject>& Params);

	// Helper methods - Character Actors
	ACharacter* FindCharacterByName(UWorld* World, const FString& NameOrLabel, FString& OutError);
	TSharedPtr<FJsonObject> CharacterToJson(ACharacter* Character, bool bIncludeMovement = false);
	TSharedPtr<FJsonObject> MovementComponentToJson(UCharacterMovementComponent* Movement);
	TSharedPtr<FJsonObject> ComponentToJson(UActorComponent* Component);

	// Helper methods - DataAssets (from CharacterData)
	UCharacterConfigDataAsset* LoadCharacterConfig(const FString& Path, FString& OutError);
	UDataTable* LoadStatsTable(const FString& Path, FString& OutError);
	bool SaveAsset(UObject* Asset, FString& OutError);
	TSharedPtr<FJsonObject> ConfigToJson(UCharacterConfigDataAsset* Config);
	TSharedPtr<FJsonObject> StatsRowToJson(const FCharacterStatsRow& Row, const FName& RowName);
	void PopulateConfigFromParams(UCharacterConfigDataAsset* Config, const TSharedRef<FJsonObject>& Params);
	void PopulateStatsRowFromParams(FCharacterStatsRow& Row, const TSharedRef<FJsonObject>& Params);
};
