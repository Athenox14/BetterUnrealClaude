// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"

/**
 * Behavior Tree and Blackboard manipulation utilities
 *
 * Provides static functions for:
 * - Loading/creating Behavior Trees and Blackboard assets
 * - Navigating tree structure via path strings ("root/0/1")
 * - Adding/removing/moving nodes (composites, tasks, decorators, services)
 * - Blackboard key management
 * - Serializing tree structure to JSON
 * - Setting node properties via reflection
 *
 * Path addressing:
 * - "root" = root composite node
 * - "root/0" = first child of root
 * - "root/0/1" = second child of first child of root
 * - "root/0.decorator.0" = first decorator on first child of root
 * - "root/0.service.0" = first service on first child of root
 */
class UNREALCLAUDE_API FBehaviorTreeEditor
{
public:
	// ===== Asset Management =====

	static UBehaviorTree* LoadBehaviorTree(const FString& AssetPath, FString& OutError);
	static UBlackboardData* LoadBlackboard(const FString& AssetPath, FString& OutError);

	static UBehaviorTree* CreateBehaviorTree(
		const FString& PackagePath,
		const FString& AssetName,
		const FString& BlackboardPath,
		FString& OutError);

	static UBlackboardData* CreateBlackboard(
		const FString& PackagePath,
		const FString& AssetName,
		FString& OutError);

	static bool SaveAsset(UObject* Asset, FString& OutError);

	// ===== Blackboard Operations =====

	static TSharedPtr<FJsonObject> SerializeBlackboardInfo(UBlackboardData* Blackboard);

	static bool AddBlackboardKey(
		UBlackboardData* Blackboard,
		const FString& KeyName,
		const FString& KeyType,
		const FString& BaseClass,
		FString& OutError);

	static bool RemoveBlackboardKey(
		UBlackboardData* Blackboard,
		const FString& KeyName,
		FString& OutError);

	static bool RenameBlackboardKey(
		UBlackboardData* Blackboard,
		const FString& OldName,
		const FString& NewName,
		FString& OutError);

	// ===== Tree Serialization =====

	static TSharedPtr<FJsonObject> SerializeTreeStructure(UBehaviorTree* BehaviorTree);

	// ===== Node Operations =====

	static UBTCompositeNode* AddCompositeNode(
		UBehaviorTree* BehaviorTree,
		const FString& ParentPath,
		const FString& NodeClass,
		int32 ChildIndex,
		FString& OutError);

	static UBTTaskNode* AddTaskNode(
		UBehaviorTree* BehaviorTree,
		const FString& ParentPath,
		const FString& NodeClass,
		int32 ChildIndex,
		FString& OutError);

	static UBTDecorator* AddDecorator(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		const FString& DecoratorClass,
		FString& OutError);

	static UBTService* AddService(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		const FString& ServiceClass,
		FString& OutError);

	static bool RemoveNode(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		FString& OutError);

	static bool MoveNode(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		int32 NewIndex,
		FString& OutError);

	static bool SetNodeProperty(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		const FString& PropertyName,
		const FString& PropertyValue,
		FString& OutError);

	static bool SetBlackboardKeySelector(
		UBehaviorTree* BehaviorTree,
		const FString& NodePath,
		const FString& PropertyName,
		const FString& KeyName,
		FString& OutError);

	static bool ConnectToBlackboard(
		UBehaviorTree* BehaviorTree,
		UBlackboardData* Blackboard,
		FString& OutError);

	// ===== Navigation =====

	static UBTNode* ResolveNodePath(UBehaviorTree* BehaviorTree, const FString& Path, FString& OutError);

	static UBTCompositeNode* ResolveCompositeNodePath(UBehaviorTree* BehaviorTree, const FString& Path, FString& OutError);

private:
	// ===== Helpers =====

	static UClass* FindBTNodeClass(const FString& NodeName, UClass* BaseClass, FString& OutError);

	static void RebuildTree(UBehaviorTree* BehaviorTree);

	static TSharedPtr<FJsonObject> SerializeCompositeNode(UBTCompositeNode* Node, int32 Depth = 0);
	static TSharedPtr<FJsonObject> SerializeTaskNode(UBTTaskNode* Node);
	static TSharedPtr<FJsonObject> SerializeNodeBase(UBTNode* Node);

	static TSharedPtr<FJsonObject> SerializeDecoratorInfo(UBTDecorator* Decorator);
	static TSharedPtr<FJsonObject> SerializeServiceInfo(UBTService* Service);

	/**
	 * Map string type name to UBlackboardKeyType subclass
	 * Supports: Bool, Int, Float, String, Name, Vector, Rotator, Object, Class, Enum
	 */
	static UClass* ResolveBlackboardKeyType(const FString& TypeName, FString& OutError);

	static void AssignExecutionIndices(UBTCompositeNode* Node, int32& Index);
};
