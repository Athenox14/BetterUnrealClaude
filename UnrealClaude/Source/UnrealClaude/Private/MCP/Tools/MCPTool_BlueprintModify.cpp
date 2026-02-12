// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_BlueprintModify.h"
#include "BlueprintUtils.h"
#include "MCP/MCPParamValidator.h"
#include "MCP/MCPBlueprintLoadContext.h"
#include "UnrealClaudeModule.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// Operation name constants
namespace BlueprintModifyOps
{
	static const FString Create = TEXT("create");
	static const FString AddVariable = TEXT("add_variable");
	static const FString RemoveVariable = TEXT("remove_variable");
	static const FString AddFunction = TEXT("add_function");
	static const FString RemoveFunction = TEXT("remove_function");
	static const FString AddComponent = TEXT("add_component");
	static const FString AddNode = TEXT("add_node");
	static const FString AddNodes = TEXT("add_nodes");
	static const FString DeleteNode = TEXT("delete_node");
	static const FString ConnectPins = TEXT("connect_pins");
	static const FString DisconnectPins = TEXT("disconnect_pins");
	static const FString SetPinValue = TEXT("set_pin_value");
	static const FString Batch = TEXT("batch");
}

FMCPToolResult FMCPTool_BlueprintModify::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == BlueprintModifyOps::Create)
	{
		return ExecuteCreate(Params);
	}
	if (Operation == BlueprintModifyOps::AddVariable)
	{
		return ExecuteAddVariable(Params);
	}
	if (Operation == BlueprintModifyOps::RemoveVariable)
	{
		return ExecuteRemoveVariable(Params);
	}
	if (Operation == BlueprintModifyOps::AddFunction)
	{
		return ExecuteAddFunction(Params);
	}
	if (Operation == BlueprintModifyOps::RemoveFunction)
	{
		return ExecuteRemoveFunction(Params);
	}
	if (Operation == BlueprintModifyOps::AddComponent)
	{
		return ExecuteAddComponent(Params);
	}
	if (Operation == BlueprintModifyOps::AddNode)
	{
		return ExecuteAddNode(Params);
	}
	if (Operation == BlueprintModifyOps::AddNodes)
	{
		return ExecuteAddNodes(Params);
	}
	if (Operation == BlueprintModifyOps::DeleteNode)
	{
		return ExecuteDeleteNode(Params);
	}
	if (Operation == BlueprintModifyOps::ConnectPins)
	{
		return ExecuteConnectPins(Params);
	}
	if (Operation == BlueprintModifyOps::DisconnectPins)
	{
		return ExecuteDisconnectPins(Params);
	}
	if (Operation == BlueprintModifyOps::SetPinValue)
	{
		return ExecuteSetPinValue(Params);
	}

	if (Operation == BlueprintModifyOps::Batch)
	{
		return ExecuteBatch(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: create, add_variable, remove_variable, add_function, remove_function, add_component, add_node, add_nodes, delete_node, connect_pins, disconnect_pins, set_pin_value, batch"),
		*Operation));
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteCreate(const TSharedRef<FJsonObject>& Params)
{
	FString PackagePath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("package_path"), PackagePath, Error))
	{
		return Error.GetValue();
	}

	FString BlueprintName;
	if (!ExtractRequiredString(Params, TEXT("blueprint_name"), BlueprintName, Error))
	{
		return Error.GetValue();
	}

	FString ParentClassName;
	if (!ExtractRequiredString(Params, TEXT("parent_class"), ParentClassName, Error))
	{
		return Error.GetValue();
	}

	FString BlueprintTypeStr = ExtractOptionalString(Params, TEXT("blueprint_type"), TEXT("Normal"));

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintPath(PackagePath, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	if (!FMCPParamValidator::ValidateBlueprintVariableName(BlueprintName, ValidationError))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid Blueprint name: %s"), *ValidationError));
	}

	FString ClassError;
	UClass* ParentClass = FBlueprintUtils::FindParentClass(ParentClassName, ClassError);
	if (!ParentClass)
	{
		return FMCPToolResult::Error(ClassError);
	}

	EBlueprintType BlueprintType = ParseBlueprintType(BlueprintTypeStr);

	FString CreateError;
	UBlueprint* NewBlueprint = FBlueprintUtils::CreateBlueprint(
		PackagePath,
		BlueprintName,
		ParentClass,
		BlueprintType,
		CreateError
	);

	if (!NewBlueprint)
	{
		return FMCPToolResult::Error(CreateError);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("blueprint_name"), NewBlueprint->GetName());
	ResultData->SetStringField(TEXT("blueprint_path"), NewBlueprint->GetPathName());
	ResultData->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	ResultData->SetStringField(TEXT("blueprint_type"), FBlueprintUtils::GetBlueprintTypeString(BlueprintType));
	ResultData->SetBoolField(TEXT("compiled"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Blueprint: %s"), *NewBlueprint->GetPathName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddVariable(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString VariableName;
	if (!ExtractRequiredString(Params, TEXT("variable_name"), VariableName, Error))
	{
		return Error.GetValue();
	}

	FString VariableType;
	if (!ExtractRequiredString(Params, TEXT("variable_type"), VariableType, Error))
	{
		return Error.GetValue();
	}

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintVariableName(VariableName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FEdGraphPinType PinType;
	FString TypeError;
	if (!FBlueprintUtils::ParsePinType(VariableType, PinType, TypeError))
	{
		return FMCPToolResult::Error(TypeError);
	}

	FString DefaultValue = ExtractOptionalString(Params, TEXT("default_value"));

	FString AddError;
	if (!FBlueprintUtils::AddVariable(Context.Blueprint, VariableName, PinType, AddError, DefaultValue))
	{
		return FMCPToolResult::Error(AddError);
	}

	if (auto CompileError = Context.CompileAndFinalize(TEXT("Variable added")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("variable_name"), VariableName);
	ResultData->SetStringField(TEXT("variable_type"), VariableType);
	if (!DefaultValue.IsEmpty())
	{
		ResultData->SetStringField(TEXT("default_value"), DefaultValue);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added variable '%s' (%s) to Blueprint"), *VariableName, *VariableType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveVariable(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString VariableName;
	if (!ExtractRequiredString(Params, TEXT("variable_name"), VariableName, Error))
	{
		return Error.GetValue();
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString RemoveError;
	if (!FBlueprintUtils::RemoveVariable(Context.Blueprint, VariableName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Variable removed")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("variable_name"), VariableName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed variable '%s' from Blueprint"), *VariableName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddFunction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	FString ValidationError;
	if (!FMCPParamValidator::ValidateBlueprintFunctionName(FunctionName, ValidationError))
	{
		return FMCPToolResult::Error(ValidationError);
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString AddError;
	if (!FBlueprintUtils::AddFunction(Context.Blueprint, FunctionName, AddError))
	{
		return FMCPToolResult::Error(AddError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Function added")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("function_name"), FunctionName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added function '%s' to Blueprint"), *FunctionName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteRemoveFunction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString FunctionName;
	if (!ExtractRequiredString(Params, TEXT("function_name"), FunctionName, Error))
	{
		return Error.GetValue();
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString RemoveError;
	if (!FBlueprintUtils::RemoveFunction(Context.Blueprint, FunctionName, RemoveError))
	{
		return FMCPToolResult::Error(RemoveError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Function removed")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("function_name"), FunctionName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Removed function '%s' from Blueprint"), *FunctionName),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddComponent(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString ComponentClassName;
	if (!ExtractRequiredString(Params, TEXT("component_class"), ComponentClassName, Error))
	{
		return Error.GetValue();
	}

	FString ComponentName = ExtractOptionalString(Params, TEXT("component_name"));
	FString AttachTo = ExtractOptionalString(Params, TEXT("attach_to"));

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	USimpleConstructionScript* SCS = Context.Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return FMCPToolResult::Error(TEXT("Blueprint does not support components (no SimpleConstructionScript)"));
	}

	// Try to find the component class with various prefixes
	UClass* ComponentClass = nullptr;
	TArray<FString> Prefixes = {
		TEXT(""),
		TEXT("/Script/Engine."),
		TEXT("/Script/AIModule."),
		TEXT("/Script/NavigationSystem."),
		TEXT("/Script/PhysicsCore."),
		TEXT("/Script/UMG."),
		TEXT("/Script/EnhancedInput."),
		TEXT("/Script/HeadMountedDisplay.")
	};

	for (const FString& Prefix : Prefixes)
	{
		FString FullPath = Prefix + ComponentClassName;
		ComponentClass = LoadClass<UActorComponent>(nullptr, *FullPath);
		if (ComponentClass)
		{
			break;
		}
	}

	// Also try FindObject as fallback
	if (!ComponentClass)
	{
		ComponentClass = FindObject<UClass>(nullptr, *ComponentClassName);
		if (ComponentClass && !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
		{
			ComponentClass = nullptr;
		}
	}

	if (!ComponentClass)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not find component class: '%s'. Try full path like '/Script/Engine.StaticMeshComponent'"),
			*ComponentClassName));
	}

	USCS_Node* NewNode = SCS->CreateNode(ComponentClass, ComponentName.IsEmpty() ? *ComponentClass->GetName() : *ComponentName);
	if (!NewNode)
	{
		return FMCPToolResult::Error(TEXT("Failed to create SCS node"));
	}

	// Attach to parent or add as root
	if (!AttachTo.IsEmpty())
	{
		// Find parent node
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == AttachTo)
			{
				ParentNode = Node;
				break;
			}
			// Also try matching by component template name
			if (Node && Node->ComponentTemplate && Node->ComponentTemplate->GetName() == AttachTo)
			{
				ParentNode = Node;
				break;
			}
		}

		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
		}
		else
		{
			// Couldn't find parent, add as root and warn
			SCS->AddNode(NewNode);
			UE_LOG(LogUnrealClaude, Warning, TEXT("Parent component '%s' not found, added as root"), *AttachTo);
		}
	}
	else
	{
		SCS->AddNode(NewNode);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Component added")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("component_class"), ComponentClass->GetName());
	ResultData->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
	if (!AttachTo.IsEmpty())
	{
		ResultData->SetStringField(TEXT("attached_to"), AttachTo);
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added component '%s' (%s) to Blueprint"),
			*NewNode->GetVariableName().ToString(), *ComponentClass->GetName()),
		ResultData
	);
}

EBlueprintType FMCPTool_BlueprintModify::ParseBlueprintType(const FString& TypeString)
{
	FString LowerType = TypeString.ToLower();

	if (LowerType == TEXT("normal") || LowerType == TEXT("actor") || LowerType == TEXT("object"))
	{
		return BPTYPE_Normal;
	}
	if (LowerType == TEXT("functionlibrary") || LowerType == TEXT("function_library"))
	{
		return BPTYPE_FunctionLibrary;
	}
	if (LowerType == TEXT("interface"))
	{
		return BPTYPE_Interface;
	}
	if (LowerType == TEXT("macrolibrary") || LowerType == TEXT("macro_library") || LowerType == TEXT("macro"))
	{
		return BPTYPE_MacroLibrary;
	}

	// Default to normal
	return BPTYPE_Normal;
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNode(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString NodeType;
	if (!ExtractRequiredString(Params, TEXT("node_type"), NodeType, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);
	int32 PosX = (int32)ExtractOptionalNumber(Params, TEXT("pos_x"), 0);
	int32 PosY = (int32)ExtractOptionalNumber(Params, TEXT("pos_y"), 0);

	// Get node params object
	TSharedPtr<FJsonObject> NodeParams;
	const TSharedPtr<FJsonObject>* NodeParamsPtr;
	if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr))
	{
		NodeParams = *NodeParamsPtr;
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString NodeId;
	FString CreateError;
	UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
	if (!NewNode)
	{
		return FMCPToolResult::Error(CreateError);
	}

	// Apply pin default values if provided
	if (NodeParams.IsValid())
	{
		const TSharedPtr<FJsonObject>* PinValuesPtr;
		if (NodeParams->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
		{
			for (const auto& PinValue : (*PinValuesPtr)->Values)
			{
				FString PinValueStr = JsonValueToString(PinValue.Value);
				if (!PinValueStr.IsEmpty())
				{
					FString PinError;
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinValue.Key, PinValueStr, PinError);
				}
			}
		}
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Node created")))
	{
		return CompileError.GetValue();
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = FBlueprintUtils::SerializeNodeInfo(NewNode);
	ResultData->SetStringField(TEXT("blueprint_path"), Context.Blueprint->GetPathName());
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created node '%s' (type: %s)"), *NodeId, *NodeType),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteAddNodes(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	// Get nodes array
	const TArray<TSharedPtr<FJsonValue>>* NodesArray;
	if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		return FMCPToolResult::Error(TEXT("'nodes' array is required"));
	}

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Create all nodes using helper
	TArray<FString> CreatedNodeIds;
	TArray<TSharedPtr<FJsonValue>> CreatedNodes;
	FString CreateError;
	if (!CreateNodesFromSpec(Graph, *NodesArray, CreatedNodeIds, CreatedNodes, CreateError))
	{
		return FMCPToolResult::Error(CreateError);
	}

	// Process connections using helper
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray;
	if (Params->TryGetArrayField(TEXT("connections"), ConnectionsArray))
	{
		ProcessNodeConnections(Graph, *ConnectionsArray, CreatedNodeIds);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Nodes created")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("graph_name"), Graph->GetName());
	ResultData->SetArrayField(TEXT("nodes"), CreatedNodes);
	ResultData->SetNumberField(TEXT("node_count"), CreatedNodeIds.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created %d nodes"), CreatedNodeIds.Num()),
		ResultData
	);
}

bool FMCPTool_BlueprintModify::CreateNodesFromSpec(
	UEdGraph* Graph,
	const TArray<TSharedPtr<FJsonValue>>& NodesArray,
	TArray<FString>& OutCreatedNodeIds,
	TArray<TSharedPtr<FJsonValue>>& OutCreatedNodes,
	FString& OutError)
{
	for (int32 i = 0; i < NodesArray.Num(); i++)
	{
		const TSharedPtr<FJsonObject>* NodeSpec;
		if (!NodesArray[i]->TryGetObject(NodeSpec))
		{
			OutError = FString::Printf(TEXT("Node at index %d is not a valid object"), i);
			return false;
		}

		FString NodeType = (*NodeSpec)->GetStringField(TEXT("type"));
		if (NodeType.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Node at index %d missing 'type' field"), i);
			return false;
		}

		int32 PosX = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_x"));
		int32 PosY = (int32)(*NodeSpec)->GetNumberField(TEXT("pos_y"));

		// Get params (could be inline or nested)
		TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* ParamsPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("params"), ParamsPtr))
		{
			NodeParams = *ParamsPtr;
		}
		else
		{
			// Copy common fields to params
			if ((*NodeSpec)->HasField(TEXT("function")))
				NodeParams->SetStringField(TEXT("function"), (*NodeSpec)->GetStringField(TEXT("function")));
			if ((*NodeSpec)->HasField(TEXT("target_class")))
				NodeParams->SetStringField(TEXT("target_class"), (*NodeSpec)->GetStringField(TEXT("target_class")));
			if ((*NodeSpec)->HasField(TEXT("event")))
				NodeParams->SetStringField(TEXT("event"), (*NodeSpec)->GetStringField(TEXT("event")));
			if ((*NodeSpec)->HasField(TEXT("variable")))
				NodeParams->SetStringField(TEXT("variable"), (*NodeSpec)->GetStringField(TEXT("variable")));
			if ((*NodeSpec)->HasField(TEXT("num_outputs")))
				NodeParams->SetNumberField(TEXT("num_outputs"), (*NodeSpec)->GetNumberField(TEXT("num_outputs")));
		}

		// Create node
		FString NodeId;
		FString CreateError;
		UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, CreateError);
		if (!NewNode)
		{
			OutError = FString::Printf(TEXT("Failed to create node %d: %s"), i, *CreateError);
			return false;
		}

		OutCreatedNodeIds.Add(NodeId);

		// Apply pin default values if provided
		const TSharedPtr<FJsonObject>* PinValuesPtr;
		if ((*NodeSpec)->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
		{
			for (const auto& PinValue : (*PinValuesPtr)->Values)
			{
				FString PinValueStr = JsonValueToString(PinValue.Value);
				if (!PinValueStr.IsEmpty())
				{
					FString PinError;
					FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinValue.Key, PinValueStr, PinError);
				}
			}
		}

		// Add to result
		TSharedPtr<FJsonObject> NodeInfo = FBlueprintUtils::SerializeNodeInfo(NewNode);
		NodeInfo->SetNumberField(TEXT("index"), i);
		OutCreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
	}

	return true;
}

void FMCPTool_BlueprintModify::ProcessNodeConnections(
	UEdGraph* Graph,
	const TArray<TSharedPtr<FJsonValue>>& ConnectionsArray,
	const TArray<FString>& CreatedNodeIds)
{
	for (int32 i = 0; i < ConnectionsArray.Num(); i++)
	{
		const TSharedPtr<FJsonObject>* ConnSpec;
		if (!ConnectionsArray[i]->TryGetObject(ConnSpec))
		{
			continue;
		}

		// Get source - can be index or node_id
		FString SourceNodeId;
		if ((*ConnSpec)->HasTypedField<EJson::Number>(TEXT("from_node")))
		{
			int32 FromIndex = (int32)(*ConnSpec)->GetNumberField(TEXT("from_node"));
			if (FromIndex >= 0 && FromIndex < CreatedNodeIds.Num())
			{
				SourceNodeId = CreatedNodeIds[FromIndex];
			}
		}
		else if ((*ConnSpec)->HasTypedField<EJson::String>(TEXT("from_node")))
		{
			SourceNodeId = (*ConnSpec)->GetStringField(TEXT("from_node"));
		}

		// Get target - can be index or node_id
		FString TargetNodeId;
		if ((*ConnSpec)->HasTypedField<EJson::Number>(TEXT("to_node")))
		{
			int32 ToIndex = (int32)(*ConnSpec)->GetNumberField(TEXT("to_node"));
			if (ToIndex >= 0 && ToIndex < CreatedNodeIds.Num())
			{
				TargetNodeId = CreatedNodeIds[ToIndex];
			}
		}
		else if ((*ConnSpec)->HasTypedField<EJson::String>(TEXT("to_node")))
		{
			TargetNodeId = (*ConnSpec)->GetStringField(TEXT("to_node"));
		}

		FString SourcePin = (*ConnSpec)->GetStringField(TEXT("from_pin"));
		FString TargetPin = (*ConnSpec)->GetStringField(TEXT("to_pin"));

		if (!SourceNodeId.IsEmpty() && !TargetNodeId.IsEmpty())
		{
			FString ConnectError;
			FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError);
		}
	}
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDeleteNode(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString NodeId;
	if (!ExtractRequiredString(Params, TEXT("node_id"), NodeId, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString DeleteError;
	if (!FBlueprintUtils::DeleteNode(Graph, NodeId, DeleteError))
	{
		return FMCPToolResult::Error(DeleteError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Node deleted")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("node_id"), NodeId);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Deleted node '%s'"), *NodeId),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteConnectPins(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString SourceNodeId;
	if (!ExtractRequiredString(Params, TEXT("source_node_id"), SourceNodeId, Error))
	{
		return Error.GetValue();
	}

	FString TargetNodeId;
	if (!ExtractRequiredString(Params, TEXT("target_node_id"), TargetNodeId, Error))
	{
		return Error.GetValue();
	}

	FString SourcePin = ExtractOptionalString(Params, TEXT("source_pin"), TEXT(""));
	FString TargetPin = ExtractOptionalString(Params, TEXT("target_pin"), TEXT(""));
	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString ConnectError;
	if (!FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, ConnectError))
	{
		return FMCPToolResult::Error(ConnectError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pins connected")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin.IsEmpty() ? TEXT("(auto exec)") : SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin.IsEmpty() ? TEXT("(auto exec)") : TargetPin);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected '%s' -> '%s'"), *SourceNodeId, *TargetNodeId),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteDisconnectPins(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString SourceNodeId;
	if (!ExtractRequiredString(Params, TEXT("source_node_id"), SourceNodeId, Error))
	{
		return Error.GetValue();
	}

	FString SourcePin;
	if (!ExtractRequiredString(Params, TEXT("source_pin"), SourcePin, Error))
	{
		return Error.GetValue();
	}

	FString TargetNodeId;
	if (!ExtractRequiredString(Params, TEXT("target_node_id"), TargetNodeId, Error))
	{
		return Error.GetValue();
	}

	FString TargetPin;
	if (!ExtractRequiredString(Params, TEXT("target_pin"), TargetPin, Error))
	{
		return Error.GetValue();
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	FString DisconnectError;
	if (!FBlueprintUtils::DisconnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, DisconnectError))
	{
		return FMCPToolResult::Error(DisconnectError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pins disconnected")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("source_node_id"), SourceNodeId);
	ResultData->SetStringField(TEXT("source_pin"), SourcePin);
	ResultData->SetStringField(TEXT("target_node_id"), TargetNodeId);
	ResultData->SetStringField(TEXT("target_pin"), TargetPin);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Disconnected '%s.%s' from '%s.%s'"), *SourceNodeId, *SourcePin, *TargetNodeId, *TargetPin),
		ResultData
	);
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteSetPinValue(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FMCPToolResult> Error;
	FString NodeId;
	if (!ExtractRequiredString(Params, TEXT("node_id"), NodeId, Error))
	{
		return Error.GetValue();
	}

	FString PinName;
	if (!ExtractRequiredString(Params, TEXT("pin_name"), PinName, Error))
	{
		return Error.GetValue();
	}

	// pin_value may be sent as number/bool by AI, so use flexible extraction
	FString PinValue = GetJsonFieldAsString(TSharedPtr<FJsonObject>(Params), TEXT("pin_value"));
	if (PinValue.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: pin_value"));
	}

	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);

	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	if (!Graph)
	{
		return FMCPToolResult::Error(GraphError);
	}

	// Set the pin value
	FString SetError;
	if (!FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinName, PinValue, SetError))
	{
		return FMCPToolResult::Error(SetError);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Pin value set")))
	{
		return CompileError.GetValue();
	}

	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("node_id"), NodeId);
	ResultData->SetStringField(TEXT("pin_name"), PinName);
	ResultData->SetStringField(TEXT("pin_value"), PinValue);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set '%s.%s' = '%s'"), *NodeId, *PinName, *PinValue),
		ResultData
	);
}

// ===== Meta Operations =====

FString FMCPTool_BlueprintModify::ResolveNodeRef(const FString& Ref, const TMap<int32, FString>& CreatedNodeIds)
{
	if (Ref.StartsWith(TEXT("#")))
	{
		FString IndexStr = Ref.Mid(1);
		if (IndexStr.IsNumeric())
		{
			int32 Index = FCString::Atoi(*IndexStr);
			if (const FString* FoundId = CreatedNodeIds.Find(Index))
			{
				return *FoundId;
			}
		}
	}
	return Ref;
}

FMCPToolResult FMCPTool_BlueprintModify::ExecuteBatch(const TSharedRef<FJsonObject>& Params)
{
	// Get operations array
	const TArray<TSharedPtr<FJsonValue>>* OperationsArray;
	if (!Params->TryGetArrayField(TEXT("operations"), OperationsArray))
	{
		return FMCPToolResult::Error(TEXT("'operations' array is required for batch mode"));
	}

	if (OperationsArray->Num() == 0)
	{
		return FMCPToolResult::Error(TEXT("'operations' array is empty"));
	}

	// Load and validate Blueprint once
	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	// Find graph once (top-level, used for node/connection operations)
	FString GraphName = ExtractOptionalString(Params, TEXT("graph_name"), TEXT(""));
	bool bFunctionGraph = ExtractOptionalBool(Params, TEXT("is_function_graph"), false);
	FString GraphError;
	UEdGraph* Graph = FBlueprintUtils::FindGraph(Context.Blueprint, GraphName, bFunctionGraph, GraphError);
	// Graph can be null (e.g. only variable/function ops) - we'll error per-op if needed

	// Track created node IDs for #N references
	TMap<int32, FString> CreatedNodeIds;

	// Results
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;

	for (int32 i = 0; i < OperationsArray->Num(); i++)
	{
		TSharedPtr<FJsonObject> OpResult = MakeShared<FJsonObject>();
		OpResult->SetNumberField(TEXT("index"), i);

		const TSharedPtr<FJsonObject>* OpObj;
		if (!(*OperationsArray)[i]->TryGetObject(OpObj) || !OpObj->IsValid())
		{
			OpResult->SetStringField(TEXT("op"), TEXT("unknown"));
			OpResult->SetBoolField(TEXT("success"), false);
			OpResult->SetStringField(TEXT("error"), TEXT("Invalid operation format"));
			ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
			FailureCount++;
			continue;
		}

		FString Op = (*OpObj)->GetStringField(TEXT("op")).ToLower();
		OpResult->SetStringField(TEXT("op"), Op);

		FString OpError;
		bool bOpSuccess = false;

		if (Op == BlueprintModifyOps::AddVariable)
		{
			FString VarName = (*OpObj)->GetStringField(TEXT("variable_name"));
			FString VarType = (*OpObj)->GetStringField(TEXT("variable_type"));
			FString VarDefaultValue = GetJsonFieldAsString(*OpObj, TEXT("default_value"));
			if (VarName.IsEmpty() || VarType.IsEmpty())
			{
				OpError = TEXT("variable_name and variable_type are required");
			}
			else
			{
				FEdGraphPinType PinType;
				if (!FBlueprintUtils::ParsePinType(VarType, PinType, OpError))
				{
					// OpError already set
				}
				else
				{
					bOpSuccess = FBlueprintUtils::AddVariable(Context.Blueprint, VarName, PinType, OpError, VarDefaultValue);
				}
			}
		}
		else if (Op == BlueprintModifyOps::RemoveVariable)
		{
			FString VarName = (*OpObj)->GetStringField(TEXT("variable_name"));
			if (VarName.IsEmpty())
			{
				OpError = TEXT("variable_name is required");
			}
			else
			{
				bOpSuccess = FBlueprintUtils::RemoveVariable(Context.Blueprint, VarName, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::AddFunction)
		{
			FString FuncName = (*OpObj)->GetStringField(TEXT("function_name"));
			if (FuncName.IsEmpty())
			{
				OpError = TEXT("function_name is required");
			}
			else
			{
				bOpSuccess = FBlueprintUtils::AddFunction(Context.Blueprint, FuncName, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::RemoveFunction)
		{
			FString FuncName = (*OpObj)->GetStringField(TEXT("function_name"));
			if (FuncName.IsEmpty())
			{
				OpError = TEXT("function_name is required");
			}
			else
			{
				bOpSuccess = FBlueprintUtils::RemoveFunction(Context.Blueprint, FuncName, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::AddComponent)
		{
			FString CompClass = (*OpObj)->GetStringField(TEXT("component_class"));
			FString CompName = (*OpObj)->GetStringField(TEXT("component_name"));
			FString CompAttachTo = (*OpObj)->GetStringField(TEXT("attach_to"));
			if (CompClass.IsEmpty())
			{
				OpError = TEXT("component_class is required");
			}
			else
			{
				USimpleConstructionScript* SCS = Context.Blueprint->SimpleConstructionScript;
				if (!SCS)
				{
					OpError = TEXT("Blueprint does not support components");
				}
				else
				{
					// Find component class with prefixes
					UClass* ComponentClass = nullptr;
					TArray<FString> Prefixes = {
						TEXT(""), TEXT("/Script/Engine."), TEXT("/Script/AIModule."),
						TEXT("/Script/NavigationSystem."), TEXT("/Script/PhysicsCore.")
					};
					for (const FString& Prefix : Prefixes)
					{
						ComponentClass = LoadClass<UActorComponent>(nullptr, *(Prefix + CompClass));
						if (ComponentClass) break;
					}

					if (!ComponentClass)
					{
						OpError = FString::Printf(TEXT("Component class not found: %s"), *CompClass);
					}
					else
					{
						USCS_Node* NewNode = SCS->CreateNode(ComponentClass,
							CompName.IsEmpty() ? *ComponentClass->GetName() : *CompName);
						if (NewNode)
						{
							if (!CompAttachTo.IsEmpty())
							{
								USCS_Node* ParentNode = nullptr;
								for (USCS_Node* Node : SCS->GetAllNodes())
								{
									if (Node && (Node->GetVariableName().ToString() == CompAttachTo ||
										(Node->ComponentTemplate && Node->ComponentTemplate->GetName() == CompAttachTo)))
									{
										ParentNode = Node;
										break;
									}
								}
								if (ParentNode)
									ParentNode->AddChildNode(NewNode);
								else
									SCS->AddNode(NewNode);
							}
							else
							{
								SCS->AddNode(NewNode);
							}
							bOpSuccess = true;
							OpResult->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
						}
						else
						{
							OpError = TEXT("Failed to create SCS node");
						}
					}
				}
			}
		}
		else if (Op == BlueprintModifyOps::AddNode)
		{
			if (!Graph)
			{
				OpError = GraphError.IsEmpty() ? TEXT("No valid graph found") : GraphError;
			}
			else
			{
				FString NodeType = (*OpObj)->GetStringField(TEXT("node_type"));
				int32 PosX = (int32)(*OpObj)->GetNumberField(TEXT("pos_x"));
				int32 PosY = (int32)(*OpObj)->GetNumberField(TEXT("pos_y"));

				// Get node params (nested object or inline fields)
				TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
				const TSharedPtr<FJsonObject>* ParamsPtr;
				if ((*OpObj)->TryGetObjectField(TEXT("node_params"), ParamsPtr))
				{
					NodeParams = *ParamsPtr;
				}
				else
				{
					if ((*OpObj)->HasField(TEXT("function")))
						NodeParams->SetStringField(TEXT("function"), (*OpObj)->GetStringField(TEXT("function")));
					if ((*OpObj)->HasField(TEXT("target_class")))
						NodeParams->SetStringField(TEXT("target_class"), (*OpObj)->GetStringField(TEXT("target_class")));
					if ((*OpObj)->HasField(TEXT("event")))
						NodeParams->SetStringField(TEXT("event"), (*OpObj)->GetStringField(TEXT("event")));
					if ((*OpObj)->HasField(TEXT("variable")))
						NodeParams->SetStringField(TEXT("variable"), (*OpObj)->GetStringField(TEXT("variable")));
					if ((*OpObj)->HasField(TEXT("num_outputs")))
						NodeParams->SetNumberField(TEXT("num_outputs"), (*OpObj)->GetNumberField(TEXT("num_outputs")));
				}

				FString NodeId;
				UEdGraphNode* NewNode = FBlueprintUtils::CreateNode(Graph, NodeType, NodeParams, PosX, PosY, NodeId, OpError);
				if (NewNode)
				{
					bOpSuccess = true;
					CreatedNodeIds.Add(i, NodeId);
					OpResult->SetStringField(TEXT("node_id"), NodeId);

					// Apply pin default values if provided
					const TSharedPtr<FJsonObject>* PinValuesPtr;
					if ((*OpObj)->TryGetObjectField(TEXT("pin_values"), PinValuesPtr))
					{
						for (const auto& PinValue : (*PinValuesPtr)->Values)
						{
							FString PinValueStr = JsonValueToString(PinValue.Value);
							if (!PinValueStr.IsEmpty())
							{
								FString PinError;
								FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinValue.Key, PinValueStr, PinError);
							}
						}
					}
				}
			}
		}
		else if (Op == BlueprintModifyOps::DeleteNode)
		{
			if (!Graph)
			{
				OpError = GraphError.IsEmpty() ? TEXT("No valid graph found") : GraphError;
			}
			else
			{
				FString NodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("node_id")), CreatedNodeIds);
				bOpSuccess = FBlueprintUtils::DeleteNode(Graph, NodeId, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::ConnectPins)
		{
			if (!Graph)
			{
				OpError = GraphError.IsEmpty() ? TEXT("No valid graph found") : GraphError;
			}
			else
			{
				FString SourceNodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("source_node_id")), CreatedNodeIds);
				FString TargetNodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("target_node_id")), CreatedNodeIds);
				FString SourcePin = (*OpObj)->GetStringField(TEXT("source_pin"));
				FString TargetPin = (*OpObj)->GetStringField(TEXT("target_pin"));
				bOpSuccess = FBlueprintUtils::ConnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::DisconnectPins)
		{
			if (!Graph)
			{
				OpError = GraphError.IsEmpty() ? TEXT("No valid graph found") : GraphError;
			}
			else
			{
				FString SourceNodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("source_node_id")), CreatedNodeIds);
				FString TargetNodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("target_node_id")), CreatedNodeIds);
				FString SourcePin = (*OpObj)->GetStringField(TEXT("source_pin"));
				FString TargetPin = (*OpObj)->GetStringField(TEXT("target_pin"));
				bOpSuccess = FBlueprintUtils::DisconnectPins(Graph, SourceNodeId, SourcePin, TargetNodeId, TargetPin, OpError);
			}
		}
		else if (Op == BlueprintModifyOps::SetPinValue)
		{
			if (!Graph)
			{
				OpError = GraphError.IsEmpty() ? TEXT("No valid graph found") : GraphError;
			}
			else
			{
				FString NodeId = ResolveNodeRef((*OpObj)->GetStringField(TEXT("node_id")), CreatedNodeIds);
				FString PinName = (*OpObj)->GetStringField(TEXT("pin_name"));
				// pin_value may be sent as number/bool by AI, so use flexible extraction
				FString PinValue = GetJsonFieldAsString(*OpObj, TEXT("pin_value"));
				if (PinValue.IsEmpty())
				{
					OpError = TEXT("pin_value is required");
				}
				else
				{
					bOpSuccess = FBlueprintUtils::SetPinDefaultValue(Graph, NodeId, PinName, PinValue, OpError);
				}
			}
		}
		else
		{
			OpError = FString::Printf(TEXT("Unknown batch op: '%s'"), *Op);
		}

		OpResult->SetBoolField(TEXT("success"), bOpSuccess);
		if (!bOpSuccess && !OpError.IsEmpty())
		{
			OpResult->SetStringField(TEXT("error"), OpError);
			FailureCount++;
		}
		else if (bOpSuccess)
		{
			SuccessCount++;
		}
		else
		{
			FailureCount++;
		}

		ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
	}

	// Compile once at the end
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Batch operations")))
	{
		// Compile failed but operations may have succeeded - include in result
	}

	// Build aggregated result
	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetNumberField(TEXT("total"), OperationsArray->Num());
	ResultData->SetNumberField(TEXT("succeeded"), SuccessCount);
	ResultData->SetNumberField(TEXT("failed"), FailureCount);
	ResultData->SetArrayField(TEXT("results"), ResultsArray);

	FString Message = FString::Printf(TEXT("Batch: %d succeeded, %d failed"), SuccessCount, FailureCount);

	if (FailureCount > 0 && SuccessCount == 0)
	{
		return FMCPToolResult::Error(Message);
	}

	return FMCPToolResult::Success(Message, ResultData);
}
