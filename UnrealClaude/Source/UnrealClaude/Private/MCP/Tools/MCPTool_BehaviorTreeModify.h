// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: behavior_tree_modify
 *
 * Create, modify, and inspect Behavior Trees and Blackboards.
 *
 * Blackboard Operations:
 * - create_blackboard: Create a new Blackboard asset
 * - get_blackboard_info: Get Blackboard keys and types
 * - add_key: Add a key to a Blackboard
 * - remove_key: Remove a key from a Blackboard
 * - rename_key: Rename a Blackboard key
 *
 * Behavior Tree Operations:
 * - create_behavior_tree: Create a new BT with root Selector
 * - get_tree_info: Get full tree structure as JSON
 * - add_composite: Add Selector/Sequence/SimpleParallel node
 * - add_task: Add Wait/MoveTo/custom task node
 * - add_decorator: Add decorator to a node
 * - add_service: Add service to a node
 * - remove_node: Remove a node by path
 * - move_node: Move a child node to a new index
 * - set_node_property: Set a property on a node via reflection
 * - set_blackboard_key: Set a FBlackboardKeySelector property
 * - connect_to_blackboard: Link BT to a Blackboard asset
 * - batch: Execute multiple operations in sequence
 *
 * Node paths: "root", "root/0", "root/0/1", "root/0.decorator.0"
 */
class FMCPTool_BehaviorTreeModify : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("behavior_tree_modify");
		Info.Description = TEXT("Create and modify Behavior Trees and Blackboards. "
			"Operations: create_blackboard, get_blackboard_info, add_key, remove_key, rename_key, "
			"create_behavior_tree, get_tree_info, add_composite, add_task, add_decorator, add_service, "
			"remove_node, move_node, set_node_property, set_blackboard_key, connect_to_blackboard, batch. "
			"Node paths use 'root/0/1' format for tree navigation.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform (see tool description for full list)"), true),
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
				TEXT("Path to BT or Blackboard asset (e.g., '/Game/AI/BT_Enemy')"), false),
			FMCPToolParameter(TEXT("blackboard_path"), TEXT("string"),
				TEXT("Path to Blackboard asset for create_behavior_tree or connect_to_blackboard"), false),
			FMCPToolParameter(TEXT("package_path"), TEXT("string"),
				TEXT("Package path for creating new assets (e.g., '/Game/AI')"), false),
			FMCPToolParameter(TEXT("asset_name"), TEXT("string"),
				TEXT("Asset name for creating new assets (e.g., 'BT_Enemy')"), false),
			FMCPToolParameter(TEXT("key_name"), TEXT("string"),
				TEXT("Blackboard key name for add_key, remove_key, rename_key"), false),
			FMCPToolParameter(TEXT("key_type"), TEXT("string"),
				TEXT("Blackboard key type: Bool, Int, Float, String, Name, Vector, Rotator, Object, Class, Enum"), false),
			FMCPToolParameter(TEXT("new_key_name"), TEXT("string"),
				TEXT("New key name for rename_key operation"), false),
			FMCPToolParameter(TEXT("base_class"), TEXT("string"),
				TEXT("Base class for Object key type (e.g., 'Actor')"), false),
			FMCPToolParameter(TEXT("parent_path"), TEXT("string"),
				TEXT("Parent node path for add_composite, add_task (e.g., 'root', 'root/0')"), false),
			FMCPToolParameter(TEXT("node_path"), TEXT("string"),
				TEXT("Node path for add_decorator, add_service, remove_node, etc."), false),
			FMCPToolParameter(TEXT("node_class"), TEXT("string"),
				TEXT("Node class: Selector, Sequence, Wait, MoveTo, Blackboard, Loop, etc."), false),
			FMCPToolParameter(TEXT("child_index"), TEXT("number"),
				TEXT("Insert position for add_composite, add_task (-1 for append)"), false, TEXT("-1")),
			FMCPToolParameter(TEXT("new_index"), TEXT("number"),
				TEXT("Target index for move_node operation"), false),
			FMCPToolParameter(TEXT("property_name"), TEXT("string"),
				TEXT("Property name for set_node_property or set_blackboard_key"), false),
			FMCPToolParameter(TEXT("property_value"), TEXT("string"),
				TEXT("Property value for set_node_property"), false),
			FMCPToolParameter(TEXT("blackboard_key"), TEXT("string"),
				TEXT("Blackboard key name for set_blackboard_key"), false),
			FMCPToolParameter(TEXT("operations"), TEXT("array"),
				TEXT("Array of operation objects for 'batch' mode"), false),
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	// Blackboard operations
	FMCPToolResult HandleCreateBlackboard(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleGetBlackboardInfo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddKey(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleRemoveKey(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleRenameKey(const TSharedRef<FJsonObject>& Params);

	// Behavior Tree operations
	FMCPToolResult HandleCreateBehaviorTree(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleGetTreeInfo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddComposite(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddTask(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddDecorator(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleAddService(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleRemoveNode(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleMoveNode(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetNodeProperty(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleSetBlackboardKey(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleConnectToBlackboard(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult HandleBatch(const TSharedRef<FJsonObject>& Params);
};
