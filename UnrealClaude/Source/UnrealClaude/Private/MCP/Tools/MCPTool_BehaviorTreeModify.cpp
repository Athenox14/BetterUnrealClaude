// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_BehaviorTreeModify.h"
#include "BehaviorTreeEditor.h"
#include "UnrealClaudeModule.h"

FMCPToolResult FMCPTool_BehaviorTreeModify::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	// Blackboard operations
	if (Operation == TEXT("create_blackboard"))
	{
		return HandleCreateBlackboard(Params);
	}
	else if (Operation == TEXT("get_blackboard_info"))
	{
		return HandleGetBlackboardInfo(Params);
	}
	else if (Operation == TEXT("add_key"))
	{
		return HandleAddKey(Params);
	}
	else if (Operation == TEXT("remove_key"))
	{
		return HandleRemoveKey(Params);
	}
	else if (Operation == TEXT("rename_key"))
	{
		return HandleRenameKey(Params);
	}
	// Behavior Tree operations
	else if (Operation == TEXT("create_behavior_tree"))
	{
		return HandleCreateBehaviorTree(Params);
	}
	else if (Operation == TEXT("get_tree_info"))
	{
		return HandleGetTreeInfo(Params);
	}
	else if (Operation == TEXT("add_composite"))
	{
		return HandleAddComposite(Params);
	}
	else if (Operation == TEXT("add_task"))
	{
		return HandleAddTask(Params);
	}
	else if (Operation == TEXT("add_decorator"))
	{
		return HandleAddDecorator(Params);
	}
	else if (Operation == TEXT("add_service"))
	{
		return HandleAddService(Params);
	}
	else if (Operation == TEXT("remove_node"))
	{
		return HandleRemoveNode(Params);
	}
	else if (Operation == TEXT("move_node"))
	{
		return HandleMoveNode(Params);
	}
	else if (Operation == TEXT("set_node_property"))
	{
		return HandleSetNodeProperty(Params);
	}
	else if (Operation == TEXT("set_blackboard_key"))
	{
		return HandleSetBlackboardKey(Params);
	}
	else if (Operation == TEXT("connect_to_blackboard"))
	{
		return HandleConnectToBlackboard(Params);
	}
	else if (Operation == TEXT("batch"))
	{
		return HandleBatch(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid operations: create_blackboard, get_blackboard_info, add_key, remove_key, rename_key, "
		"create_behavior_tree, get_tree_info, add_composite, add_task, add_decorator, add_service, "
		"remove_node, move_node, set_node_property, set_blackboard_key, connect_to_blackboard, batch"),
		*Operation));
}

// ===== Blackboard Operations =====

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleCreateBlackboard(const TSharedRef<FJsonObject>& Params)
{
	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"));
	FString AssetName = ExtractOptionalString(Params, TEXT("asset_name"));

	if (PackagePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'package_path' parameter is required for create_blackboard"));
	}
	if (AssetName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_name' parameter is required for create_blackboard"));
	}

	FString Error;
	UBlackboardData* BB = FBehaviorTreeEditor::CreateBlackboard(PackagePath, AssetName, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	// Save
	FBehaviorTreeEditor::SaveAsset(BB, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), BB->GetName());
	Result->SetStringField(TEXT("path"), BB->GetPathName());
	Result->SetBoolField(TEXT("created"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Blackboard '%s'"), *BB->GetName()), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleGetBlackboardInfo(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' parameter is required for get_blackboard_info"));
	}

	FString Error;
	UBlackboardData* BB = FBehaviorTreeEditor::LoadBlackboard(AssetPath, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeBlackboardInfo(BB);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Blackboard '%s' has %d keys"), *BB->GetName(), BB->Keys.Num()),
		Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleAddKey(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString KeyName = ExtractOptionalString(Params, TEXT("key_name"));
	FString KeyType = ExtractOptionalString(Params, TEXT("key_type"));
	FString BaseClass = ExtractOptionalString(Params, TEXT("base_class"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for add_key"));
	}
	if (KeyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'key_name' is required for add_key"));
	}
	if (KeyType.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'key_type' is required for add_key (Bool, Int, Float, String, Name, Vector, Rotator, Object, Class, Enum)"));
	}

	FString Error;
	UBlackboardData* BB = FBehaviorTreeEditor::LoadBlackboard(AssetPath, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::AddBlackboardKey(BB, KeyName, KeyType, BaseClass, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BB, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeBlackboardInfo(BB);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added key '%s' (type: %s) to Blackboard"), *KeyName, *KeyType),
		Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleRemoveKey(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString KeyName = ExtractOptionalString(Params, TEXT("key_name"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for remove_key"));
	}
	if (KeyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'key_name' is required for remove_key"));
	}

	FString Error;
	UBlackboardData* BB = FBehaviorTreeEditor::LoadBlackboard(AssetPath, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::RemoveBlackboardKey(BB, KeyName, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BB, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeBlackboardInfo(BB);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed key '%s' from Blackboard"), *KeyName),
		Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleRenameKey(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString KeyName = ExtractOptionalString(Params, TEXT("key_name"));
	FString NewKeyName = ExtractOptionalString(Params, TEXT("new_key_name"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for rename_key"));
	}
	if (KeyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'key_name' is required for rename_key"));
	}
	if (NewKeyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'new_key_name' is required for rename_key"));
	}

	FString Error;
	UBlackboardData* BB = FBehaviorTreeEditor::LoadBlackboard(AssetPath, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::RenameBlackboardKey(BB, KeyName, NewKeyName, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BB, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeBlackboardInfo(BB);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Renamed key '%s' to '%s'"), *KeyName, *NewKeyName),
		Result);
}

// ===== Behavior Tree Operations =====

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleCreateBehaviorTree(const TSharedRef<FJsonObject>& Params)
{
	FString PackagePath = ExtractOptionalString(Params, TEXT("package_path"));
	FString AssetName = ExtractOptionalString(Params, TEXT("asset_name"));
	FString BlackboardPath = ExtractOptionalString(Params, TEXT("blackboard_path"));

	if (PackagePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'package_path' is required for create_behavior_tree"));
	}
	if (AssetName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_name' is required for create_behavior_tree"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::CreateBehaviorTree(PackagePath, AssetName, BlackboardPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	Result->SetBoolField(TEXT("created"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Behavior Tree '%s'"), *BT->GetName()), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleGetTreeInfo(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for get_tree_info"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Tree structure for '%s'"), *BT->GetName()), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleAddComposite(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString ParentPath = ExtractOptionalString(Params, TEXT("parent_path"), TEXT("root"));
	FString NodeClass = ExtractOptionalString(Params, TEXT("node_class"));
	int32 ChildIndex = ExtractOptionalNumber<int32>(Params, TEXT("child_index"), -1);

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for add_composite"));
	}
	if (NodeClass.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_class' is required for add_composite (Selector, Sequence, SimpleParallel)"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	UBTCompositeNode* NewNode = FBehaviorTreeEditor::AddCompositeNode(BT, ParentPath, NodeClass, ChildIndex, Error);
	if (!NewNode)
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	Result->SetStringField(TEXT("added_node_class"), NewNode->GetClass()->GetName());
	Result->SetStringField(TEXT("added_to"), ParentPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added composite '%s' to '%s'"), *NodeClass, *ParentPath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleAddTask(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString ParentPath = ExtractOptionalString(Params, TEXT("parent_path"), TEXT("root"));
	FString NodeClass = ExtractOptionalString(Params, TEXT("node_class"));
	int32 ChildIndex = ExtractOptionalNumber<int32>(Params, TEXT("child_index"), -1);

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for add_task"));
	}
	if (NodeClass.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_class' is required for add_task (Wait, MoveTo, RunBehavior, etc.)"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	UBTTaskNode* NewNode = FBehaviorTreeEditor::AddTaskNode(BT, ParentPath, NodeClass, ChildIndex, Error);
	if (!NewNode)
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	Result->SetStringField(TEXT("added_node_class"), NewNode->GetClass()->GetName());
	Result->SetStringField(TEXT("added_to"), ParentPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added task '%s' to '%s'"), *NodeClass, *ParentPath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleAddDecorator(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));
	FString DecoratorClass = ExtractOptionalString(Params, TEXT("node_class"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for add_decorator"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for add_decorator (e.g., 'root/0')"));
	}
	if (DecoratorClass.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_class' is required for add_decorator (Blackboard, ForceSuccess, Loop, TimeLimit, Cooldown)"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	UBTDecorator* NewDecorator = FBehaviorTreeEditor::AddDecorator(BT, NodePath, DecoratorClass, Error);
	if (!NewDecorator)
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	Result->SetStringField(TEXT("added_decorator_class"), NewDecorator->GetClass()->GetName());
	Result->SetStringField(TEXT("added_to"), NodePath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added decorator '%s' to '%s'"), *DecoratorClass, *NodePath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleAddService(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));
	FString ServiceClass = ExtractOptionalString(Params, TEXT("node_class"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for add_service"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for add_service"));
	}
	if (ServiceClass.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_class' is required for add_service (BlackboardBase, DefaultFocus)"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	UBTService* NewService = FBehaviorTreeEditor::AddService(BT, NodePath, ServiceClass, Error);
	if (!NewService)
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	Result->SetStringField(TEXT("added_service_class"), NewService->GetClass()->GetName());
	Result->SetStringField(TEXT("added_to"), NodePath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added service '%s' to '%s'"), *ServiceClass, *NodePath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleRemoveNode(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for remove_node"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for remove_node"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::RemoveNode(BT, NodePath, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed node at '%s'"), *NodePath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleMoveNode(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for move_node"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for move_node"));
	}
	if (!Params->HasField(TEXT("new_index")))
	{
		return FMCPToolResult::Error(TEXT("'new_index' is required for move_node"));
	}

	int32 NewIndex = ExtractOptionalNumber<int32>(Params, TEXT("new_index"), 0);

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::MoveNode(BT, NodePath, NewIndex, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = FBehaviorTreeEditor::SerializeTreeStructure(BT);
	return FMCPToolResult::Success(
		FString::Printf(TEXT("Moved node '%s' to index %d"), *NodePath, NewIndex), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleSetNodeProperty(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));
	FString PropertyName = ExtractOptionalString(Params, TEXT("property_name"));
	FString PropertyValue = GetJsonFieldAsString(Params, TEXT("property_value"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for set_node_property"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for set_node_property"));
	}
	if (PropertyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'property_name' is required for set_node_property"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::SetNodeProperty(BT, NodePath, PropertyName, PropertyValue, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("node_path"), NodePath);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("value"), PropertyValue);
	Result->SetBoolField(TEXT("success"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set '%s' = '%s' on node at '%s'"), *PropertyName, *PropertyValue, *NodePath), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleSetBlackboardKey(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString NodePath = ExtractOptionalString(Params, TEXT("node_path"));
	FString PropertyName = ExtractOptionalString(Params, TEXT("property_name"));
	FString BlackboardKey = ExtractOptionalString(Params, TEXT("blackboard_key"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for set_blackboard_key"));
	}
	if (NodePath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'node_path' is required for set_blackboard_key"));
	}
	if (PropertyName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'property_name' is required for set_blackboard_key"));
	}
	if (BlackboardKey.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'blackboard_key' is required for set_blackboard_key"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::SetBlackboardKeySelector(BT, NodePath, PropertyName, BlackboardKey, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("node_path"), NodePath);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("blackboard_key"), BlackboardKey);
	Result->SetBoolField(TEXT("success"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set BlackboardKeySelector '%s' to key '%s'"), *PropertyName, *BlackboardKey), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleConnectToBlackboard(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("asset_path"));
	FString BlackboardPath = ExtractOptionalString(Params, TEXT("blackboard_path"));

	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'asset_path' is required for connect_to_blackboard"));
	}
	if (BlackboardPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("'blackboard_path' is required for connect_to_blackboard"));
	}

	FString Error;
	UBehaviorTree* BT = FBehaviorTreeEditor::LoadBehaviorTree(AssetPath, Error);
	if (!BT)
	{
		return FMCPToolResult::Error(Error);
	}

	UBlackboardData* BB = FBehaviorTreeEditor::LoadBlackboard(BlackboardPath, Error);
	if (!BB)
	{
		return FMCPToolResult::Error(Error);
	}

	if (!FBehaviorTreeEditor::ConnectToBlackboard(BT, BB, Error))
	{
		return FMCPToolResult::Error(Error);
	}

	FBehaviorTreeEditor::SaveAsset(BT, Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("behavior_tree"), BT->GetPathName());
	Result->SetStringField(TEXT("blackboard"), BB->GetPathName());
	Result->SetBoolField(TEXT("connected"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected BT '%s' to Blackboard '%s'"), *BT->GetName(), *BB->GetName()), Result);
}

FMCPToolResult FMCPTool_BehaviorTreeModify::HandleBatch(const TSharedRef<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("operations"), OperationsArray) || !OperationsArray)
	{
		return FMCPToolResult::Error(TEXT("'operations' array is required for batch mode"));
	}

	// Collect inheritable params from parent - sub-ops inherit these if not set
	static const TArray<FString> InheritableKeys = {
		TEXT("asset_path"), TEXT("blackboard_path"), TEXT("package_path")
	};

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;

	for (int32 i = 0; i < OperationsArray->Num(); i++)
	{
		const TSharedPtr<FJsonValue>& OpValue = (*OperationsArray)[i];
		const TSharedPtr<FJsonObject>* OpObject = nullptr;
		if (!OpValue.IsValid() || !OpValue->TryGetObject(OpObject) || !OpObject || !(*OpObject).IsValid())
		{
			TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
			ErrorResult->SetNumberField(TEXT("index"), i);
			ErrorResult->SetBoolField(TEXT("success"), false);
			ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid operation object"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(ErrorResult));
			FailureCount++;
			continue;
		}

		// Inherit parent params into sub-operation if not already set
		TSharedRef<FJsonObject> OpParams = (*OpObject).ToSharedRef();
		for (const FString& Key : InheritableKeys)
		{
			if (!OpParams->HasField(Key) && Params->HasField(Key))
			{
				OpParams->SetStringField(Key, Params->GetStringField(Key));
			}
		}

		FMCPToolResult SubResult = Execute(OpParams);

		TSharedPtr<FJsonObject> SubResultJson = MakeShared<FJsonObject>();
		SubResultJson->SetNumberField(TEXT("index"), i);
		SubResultJson->SetBoolField(TEXT("success"), SubResult.bSuccess);
		SubResultJson->SetStringField(TEXT("message"), SubResult.Message);
		if (SubResult.Data.IsValid())
		{
			SubResultJson->SetObjectField(TEXT("data"), SubResult.Data);
		}

		ResultsArray.Add(MakeShared<FJsonValueObject>(SubResultJson));

		if (SubResult.bSuccess)
		{
			SuccessCount++;
		}
		else
		{
			FailureCount++;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("results"), ResultsArray);
	Result->SetNumberField(TEXT("total"), OperationsArray->Num());
	Result->SetNumberField(TEXT("succeeded"), SuccessCount);
	Result->SetNumberField(TEXT("failed"), FailureCount);

	if (FailureCount > 0)
	{
		return FMCPToolResult::Success(
			FString::Printf(TEXT("Batch: %d/%d operations succeeded, %d failed"),
				SuccessCount, OperationsArray->Num(), FailureCount),
			Result);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Batch: all %d operations succeeded"), SuccessCount),
		Result);
}
