// Copyright Natali Caggiano. All Rights Reserved.

#include "BehaviorTreeEditor.h"
#include "UnrealClaudeModule.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_PlayAnimation.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_ForceSuccess.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "BehaviorTree/Decorators/BTDecorator_TimeLimit.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BehaviorTree/BehaviorTreeTypes.h"

// ===== Asset Management =====

UBehaviorTree* FBehaviorTreeEditor::LoadBehaviorTree(const FString& AssetPath, FString& OutError)
{
	FString CleanPath = AssetPath;
	if (CleanPath.EndsWith(TEXT(".uasset")))
	{
		CleanPath = CleanPath.LeftChop(7);
	}

	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *CleanPath);
	if (!BT)
	{
		// Try with full path
		BT = LoadObject<UBehaviorTree>(nullptr, *FString::Printf(TEXT("%s.%s"),
			*CleanPath, *FPaths::GetBaseFilename(CleanPath)));
	}

	if (!BT)
	{
		OutError = FString::Printf(TEXT("Failed to load Behavior Tree: '%s'"), *AssetPath);
	}
	return BT;
}

UBlackboardData* FBehaviorTreeEditor::LoadBlackboard(const FString& AssetPath, FString& OutError)
{
	FString CleanPath = AssetPath;
	if (CleanPath.EndsWith(TEXT(".uasset")))
	{
		CleanPath = CleanPath.LeftChop(7);
	}

	UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *CleanPath);
	if (!BB)
	{
		BB = LoadObject<UBlackboardData>(nullptr, *FString::Printf(TEXT("%s.%s"),
			*CleanPath, *FPaths::GetBaseFilename(CleanPath)));
	}

	if (!BB)
	{
		OutError = FString::Printf(TEXT("Failed to load Blackboard: '%s'"), *AssetPath);
	}
	return BB;
}

UBehaviorTree* FBehaviorTreeEditor::CreateBehaviorTree(
	const FString& PackagePath,
	const FString& AssetName,
	const FString& BlackboardPath,
	FString& OutError)
{
	// Create package
	FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: '%s'"), *FullPath);
		return nullptr;
	}

	// Create BehaviorTree asset
	UBehaviorTree* BT = NewObject<UBehaviorTree>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!BT)
	{
		OutError = TEXT("Failed to create Behavior Tree object");
		return nullptr;
	}

	// Create default root node (Selector)
	UBTComposite_Selector* RootSelector = NewObject<UBTComposite_Selector>(BT);
	BT->RootNode = RootSelector;

	// Connect to Blackboard if specified
	if (!BlackboardPath.IsEmpty())
	{
		UBlackboardData* BB = LoadBlackboard(BlackboardPath, OutError);
		if (BB)
		{
			BT->BlackboardAsset = BB;
		}
		// Don't fail creation if BB not found, just warn
		if (!BB)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Blackboard '%s' not found, BT created without BB"), *BlackboardPath);
			OutError.Empty(); // Clear error, creation still succeeds
		}
	}

	// Mark dirty and notify
	BT->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(BT);

	UE_LOG(LogUnrealClaude, Log, TEXT("Created Behavior Tree: %s"), *FullPath);
	return BT;
}

UBlackboardData* FBehaviorTreeEditor::CreateBlackboard(
	const FString& PackagePath,
	const FString& AssetName,
	FString& OutError)
{
	FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: '%s'"), *FullPath);
		return nullptr;
	}

	UBlackboardData* BB = NewObject<UBlackboardData>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!BB)
	{
		OutError = TEXT("Failed to create Blackboard object");
		return nullptr;
	}

	BB->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(BB);

	UE_LOG(LogUnrealClaude, Log, TEXT("Created Blackboard: %s"), *FullPath);
	return BB;
}

bool FBehaviorTreeEditor::SaveAsset(UObject* Asset, FString& OutError)
{
	if (!Asset)
	{
		OutError = TEXT("Cannot save null asset");
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		OutError = TEXT("Asset has no package");
		return false;
	}

	FString PackageFilename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);

	if (!bSaved)
	{
		OutError = FString::Printf(TEXT("Failed to save package: '%s'"), *Package->GetName());
		return false;
	}

	return true;
}

// ===== Blackboard Operations =====

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeBlackboardInfo(UBlackboardData* Blackboard)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!Blackboard)
	{
		return Result;
	}

	Result->SetStringField(TEXT("name"), Blackboard->GetName());
	Result->SetStringField(TEXT("path"), Blackboard->GetPathName());

	// Serialize keys
	TArray<TSharedPtr<FJsonValue>> KeysArray;
	for (const FBlackboardEntry& Entry : Blackboard->Keys)
	{
		TSharedPtr<FJsonObject> KeyJson = MakeShared<FJsonObject>();
		KeyJson->SetStringField(TEXT("name"), Entry.EntryName.ToString());
		KeyJson->SetStringField(TEXT("type"), Entry.KeyType ? Entry.KeyType->GetClass()->GetName() : TEXT("None"));
		KeyJson->SetBoolField(TEXT("instance_synced"), Entry.bInstanceSynced);
		KeysArray.Add(MakeShared<FJsonValueObject>(KeyJson));
	}
	Result->SetArrayField(TEXT("keys"), KeysArray);
	Result->SetNumberField(TEXT("key_count"), Blackboard->Keys.Num());

	// Parent blackboard
	if (Blackboard->Parent)
	{
		Result->SetStringField(TEXT("parent"), Blackboard->Parent->GetPathName());
	}

	return Result;
}

bool FBehaviorTreeEditor::AddBlackboardKey(
	UBlackboardData* Blackboard,
	const FString& KeyName,
	const FString& KeyType,
	const FString& BaseClass,
	FString& OutError)
{
	if (!Blackboard)
	{
		OutError = TEXT("Blackboard is null");
		return false;
	}

	if (KeyName.IsEmpty())
	{
		OutError = TEXT("Key name cannot be empty");
		return false;
	}

	// Check if key already exists
	for (const FBlackboardEntry& Entry : Blackboard->Keys)
	{
		if (Entry.EntryName.ToString().Equals(KeyName, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("Key '%s' already exists in Blackboard"), *KeyName);
			return false;
		}
	}

	// Resolve key type
	UClass* KeyTypeClass = ResolveBlackboardKeyType(KeyType, OutError);
	if (!KeyTypeClass)
	{
		return false;
	}

	// Create new entry
	FBlackboardEntry NewEntry;
	NewEntry.EntryName = FName(*KeyName);
	NewEntry.KeyType = NewObject<UBlackboardKeyType>(Blackboard, KeyTypeClass);

	// For Object type, set the base class
	if (KeyTypeClass == UBlackboardKeyType_Object::StaticClass() && !BaseClass.IsEmpty())
	{
		UBlackboardKeyType_Object* ObjKeyType = Cast<UBlackboardKeyType_Object>(NewEntry.KeyType);
		if (ObjKeyType)
		{
			UClass* ObjClass = FindObject<UClass>(nullptr, *BaseClass);
			if (!ObjClass)
			{
				ObjClass = LoadClass<UObject>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *BaseClass));
			}
			if (ObjClass)
			{
				ObjKeyType->BaseClass = ObjClass;
			}
		}
	}

	Blackboard->Keys.Add(NewEntry);
	Blackboard->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Added Blackboard key '%s' (type: %s)"), *KeyName, *KeyType);
	return true;
}

bool FBehaviorTreeEditor::RemoveBlackboardKey(
	UBlackboardData* Blackboard,
	const FString& KeyName,
	FString& OutError)
{
	if (!Blackboard)
	{
		OutError = TEXT("Blackboard is null");
		return false;
	}

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Blackboard->Keys.Num(); i++)
	{
		if (Blackboard->Keys[i].EntryName.ToString().Equals(KeyName, ESearchCase::IgnoreCase))
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Key '%s' not found in Blackboard"), *KeyName);
		return false;
	}

	Blackboard->Keys.RemoveAt(FoundIndex);
	Blackboard->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Removed Blackboard key '%s'"), *KeyName);
	return true;
}

bool FBehaviorTreeEditor::RenameBlackboardKey(
	UBlackboardData* Blackboard,
	const FString& OldName,
	const FString& NewName,
	FString& OutError)
{
	if (!Blackboard)
	{
		OutError = TEXT("Blackboard is null");
		return false;
	}

	if (NewName.IsEmpty())
	{
		OutError = TEXT("New key name cannot be empty");
		return false;
	}

	// Check new name doesn't exist
	for (const FBlackboardEntry& Entry : Blackboard->Keys)
	{
		if (Entry.EntryName.ToString().Equals(NewName, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("Key '%s' already exists in Blackboard"), *NewName);
			return false;
		}
	}

	// Find and rename
	for (FBlackboardEntry& Entry : Blackboard->Keys)
	{
		if (Entry.EntryName.ToString().Equals(OldName, ESearchCase::IgnoreCase))
		{
			Entry.EntryName = FName(*NewName);
			Blackboard->MarkPackageDirty();
			UE_LOG(LogUnrealClaude, Log, TEXT("Renamed Blackboard key '%s' -> '%s'"), *OldName, *NewName);
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Key '%s' not found in Blackboard"), *OldName);
	return false;
}

// ===== Tree Serialization =====

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeTreeStructure(UBehaviorTree* BehaviorTree)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!BehaviorTree)
	{
		return Result;
	}

	Result->SetStringField(TEXT("name"), BehaviorTree->GetName());
	Result->SetStringField(TEXT("path"), BehaviorTree->GetPathName());

	if (BehaviorTree->BlackboardAsset)
	{
		Result->SetStringField(TEXT("blackboard"), BehaviorTree->BlackboardAsset->GetPathName());
	}

	if (BehaviorTree->RootNode)
	{
		Result->SetObjectField(TEXT("root"), SerializeCompositeNode(BehaviorTree->RootNode));
	}

	return Result;
}

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeCompositeNode(UBTCompositeNode* Node, int32 Depth)
{
	if (!Node)
	{
		return MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonObject> NodeJson = SerializeNodeBase(Node);
	NodeJson->SetStringField(TEXT("node_category"), TEXT("composite"));

	// Serialize children
	TArray<TSharedPtr<FJsonValue>> ChildrenArray;
	for (int32 i = 0; i < Node->Children.Num(); i++)
	{
		const FBTCompositeChild& Child = Node->Children[i];
		TSharedPtr<FJsonObject> ChildJson = MakeShared<FJsonObject>();
		ChildJson->SetNumberField(TEXT("index"), i);

		FString ChildPath = FString::Printf(TEXT("root"));
		// Build path (simplified for top level)

		if (Child.ChildComposite)
		{
			ChildJson->SetObjectField(TEXT("node"), SerializeCompositeNode(Child.ChildComposite, Depth + 1));
		}
		else if (Child.ChildTask)
		{
			ChildJson->SetObjectField(TEXT("node"), SerializeTaskNode(Child.ChildTask));
		}

		// Decorators
		TArray<TSharedPtr<FJsonValue>> DecoArray;
		for (UBTDecorator* Deco : Child.Decorators)
		{
			if (Deco)
			{
				DecoArray.Add(MakeShared<FJsonValueObject>(SerializeDecoratorInfo(Deco)));
			}
		}
		if (DecoArray.Num() > 0)
		{
			ChildJson->SetArrayField(TEXT("decorators"), DecoArray);
		}

		ChildrenArray.Add(MakeShared<FJsonValueObject>(ChildJson));
	}
	NodeJson->SetArrayField(TEXT("children"), ChildrenArray);

	// Services on this composite
	TArray<TSharedPtr<FJsonValue>> ServicesArray;
	for (UBTService* Service : Node->Services)
	{
		if (Service)
		{
			ServicesArray.Add(MakeShared<FJsonValueObject>(SerializeServiceInfo(Service)));
		}
	}
	if (ServicesArray.Num() > 0)
	{
		NodeJson->SetArrayField(TEXT("services"), ServicesArray);
	}

	return NodeJson;
}

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeTaskNode(UBTTaskNode* Node)
{
	if (!Node)
	{
		return MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonObject> NodeJson = SerializeNodeBase(Node);
	NodeJson->SetStringField(TEXT("node_category"), TEXT("task"));

	// Services on tasks
	TArray<TSharedPtr<FJsonValue>> ServicesArray;
	for (UBTService* Service : Node->Services)
	{
		if (Service)
		{
			ServicesArray.Add(MakeShared<FJsonValueObject>(SerializeServiceInfo(Service)));
		}
	}
	if (ServicesArray.Num() > 0)
	{
		NodeJson->SetArrayField(TEXT("services"), ServicesArray);
	}

	return NodeJson;
}

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeNodeBase(UBTNode* Node)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Node)
	{
		return Json;
	}

	Json->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	Json->SetStringField(TEXT("node_name"), Node->GetNodeName());
	Json->SetNumberField(TEXT("execution_index"), Node->GetExecutionIndex());

	// Extract instance description if available
	FString Description = Node->GetStaticDescription();
	if (!Description.IsEmpty())
	{
		Json->SetStringField(TEXT("description"), Description);
	}

	return Json;
}

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeDecoratorInfo(UBTDecorator* Decorator)
{
	TSharedPtr<FJsonObject> Json = SerializeNodeBase(Decorator);
	Json->SetStringField(TEXT("node_category"), TEXT("decorator"));
	Json->SetBoolField(TEXT("inverse_condition"), Decorator->IsInversed());
	return Json;
}

TSharedPtr<FJsonObject> FBehaviorTreeEditor::SerializeServiceInfo(UBTService* Service)
{
	TSharedPtr<FJsonObject> Json = SerializeNodeBase(Service);
	Json->SetStringField(TEXT("node_category"), TEXT("service"));
	return Json;
}

// ===== Node Operations =====

UBTCompositeNode* FBehaviorTreeEditor::AddCompositeNode(
	UBehaviorTree* BehaviorTree,
	const FString& ParentPath,
	const FString& NodeClass,
	int32 ChildIndex,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return nullptr;
	}

	UBTCompositeNode* ParentNode = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
	if (!ParentNode)
	{
		return nullptr;
	}

	// Find the composite class
	UClass* CompositeClass = FindBTNodeClass(NodeClass, UBTCompositeNode::StaticClass(), OutError);
	if (!CompositeClass)
	{
		return nullptr;
	}

	// Create the node
	UBTCompositeNode* NewNode = NewObject<UBTCompositeNode>(BehaviorTree, CompositeClass);
	if (!NewNode)
	{
		OutError = FString::Printf(TEXT("Failed to create composite node of class '%s'"), *NodeClass);
		return nullptr;
	}

	// Add as child
	FBTCompositeChild NewChild;
	NewChild.ChildComposite = NewNode;
	NewChild.ChildTask = nullptr;

	if (ChildIndex >= 0 && ChildIndex < ParentNode->Children.Num())
	{
		ParentNode->Children.Insert(NewChild, ChildIndex);
	}
	else
	{
		ParentNode->Children.Add(NewChild);
	}

	// Rebuild execution indices
	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Added composite node '%s' to '%s'"), *NodeClass, *ParentPath);
	return NewNode;
}

UBTTaskNode* FBehaviorTreeEditor::AddTaskNode(
	UBehaviorTree* BehaviorTree,
	const FString& ParentPath,
	const FString& NodeClass,
	int32 ChildIndex,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return nullptr;
	}

	UBTCompositeNode* ParentNode = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
	if (!ParentNode)
	{
		return nullptr;
	}

	// Find the task class
	UClass* TaskClass = FindBTNodeClass(NodeClass, UBTTaskNode::StaticClass(), OutError);
	if (!TaskClass)
	{
		return nullptr;
	}

	// Create the node
	UBTTaskNode* NewNode = NewObject<UBTTaskNode>(BehaviorTree, TaskClass);
	if (!NewNode)
	{
		OutError = FString::Printf(TEXT("Failed to create task node of class '%s'"), *NodeClass);
		return nullptr;
	}

	// Add as child
	FBTCompositeChild NewChild;
	NewChild.ChildComposite = nullptr;
	NewChild.ChildTask = NewNode;

	if (ChildIndex >= 0 && ChildIndex < ParentNode->Children.Num())
	{
		ParentNode->Children.Insert(NewChild, ChildIndex);
	}
	else
	{
		ParentNode->Children.Add(NewChild);
	}

	// Rebuild execution indices
	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Added task node '%s' to '%s'"), *NodeClass, *ParentPath);
	return NewNode;
}

UBTDecorator* FBehaviorTreeEditor::AddDecorator(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	const FString& DecoratorClass,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return nullptr;
	}

	// We need to find the parent composite and the child index
	// The NodePath points to a child node; we need its parent's Children[] entry
	FString ParentPath, ChildIndexStr;
	int32 ChildIndex = -1;

	if (NodePath == TEXT("root"))
	{
		// Decorators on root need special handling - can't easily add decorators to root
		OutError = TEXT("Cannot add decorators directly to the root node. Add them to child nodes.");
		return nullptr;
	}

	// Parse path to find parent and child index
	int32 LastSlash;
	if (NodePath.FindLastChar(TEXT('/'), LastSlash))
	{
		ParentPath = NodePath.Left(LastSlash);
		FString IndexPart = NodePath.Mid(LastSlash + 1);
		ChildIndex = FCString::Atoi(*IndexPart);
	}
	else
	{
		OutError = FString::Printf(TEXT("Invalid path for decorator: '%s'. Use format 'root/0'"), *NodePath);
		return nullptr;
	}

	UBTCompositeNode* ParentNode = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
	if (!ParentNode)
	{
		return nullptr;
	}

	if (ChildIndex < 0 || ChildIndex >= ParentNode->Children.Num())
	{
		OutError = FString::Printf(TEXT("Child index %d out of range (parent has %d children)"),
			ChildIndex, ParentNode->Children.Num());
		return nullptr;
	}

	// Find decorator class
	UClass* DecoClass = FindBTNodeClass(DecoratorClass, UBTDecorator::StaticClass(), OutError);
	if (!DecoClass)
	{
		return nullptr;
	}

	// Create the decorator
	UBTDecorator* NewDecorator = NewObject<UBTDecorator>(BehaviorTree, DecoClass);
	if (!NewDecorator)
	{
		OutError = FString::Printf(TEXT("Failed to create decorator of class '%s'"), *DecoratorClass);
		return nullptr;
	}

	ParentNode->Children[ChildIndex].Decorators.Add(NewDecorator);

	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Added decorator '%s' to '%s'"), *DecoratorClass, *NodePath);
	return NewDecorator;
}

UBTService* FBehaviorTreeEditor::AddService(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	const FString& ServiceClass,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return nullptr;
	}

	// Find the target node - services can go on composites or tasks
	UBTNode* TargetNode = ResolveNodePath(BehaviorTree, NodePath, OutError);
	if (!TargetNode)
	{
		return nullptr;
	}

	// Find service class
	UClass* SvcClass = FindBTNodeClass(ServiceClass, UBTService::StaticClass(), OutError);
	if (!SvcClass)
	{
		return nullptr;
	}

	// Create the service
	UBTService* NewService = NewObject<UBTService>(BehaviorTree, SvcClass);
	if (!NewService)
	{
		OutError = FString::Printf(TEXT("Failed to create service of class '%s'"), *ServiceClass);
		return nullptr;
	}

	// Add to composite or task
	if (UBTCompositeNode* Composite = Cast<UBTCompositeNode>(TargetNode))
	{
		Composite->Services.Add(NewService);
	}
	else if (UBTTaskNode* Task = Cast<UBTTaskNode>(TargetNode))
	{
		Task->Services.Add(NewService);
	}
	else
	{
		OutError = TEXT("Services can only be added to composite or task nodes");
		return nullptr;
	}

	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Added service '%s' to '%s'"), *ServiceClass, *NodePath);
	return NewService;
}

bool FBehaviorTreeEditor::RemoveNode(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return false;
	}

	if (NodePath == TEXT("root"))
	{
		OutError = TEXT("Cannot remove the root node");
		return false;
	}

	// Check if it's a decorator or service path
	if (NodePath.Contains(TEXT(".decorator.")))
	{
		// Parse: parentPath.decorator.index
		FString BasePath, DecoIndexStr;
		NodePath.Split(TEXT(".decorator."), &BasePath, &DecoIndexStr);
		int32 DecoIndex = FCString::Atoi(*DecoIndexStr);

		// Get the parent composite and child index
		FString ParentPath;
		int32 ChildIndex;
		int32 LastSlash;
		if (BasePath.FindLastChar(TEXT('/'), LastSlash))
		{
			ParentPath = BasePath.Left(LastSlash);
			ChildIndex = FCString::Atoi(*BasePath.Mid(LastSlash + 1));
		}
		else
		{
			OutError = FString::Printf(TEXT("Invalid decorator path: '%s'"), *NodePath);
			return false;
		}

		UBTCompositeNode* Parent = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
		if (!Parent || ChildIndex < 0 || ChildIndex >= Parent->Children.Num())
		{
			return false;
		}

		if (DecoIndex < 0 || DecoIndex >= Parent->Children[ChildIndex].Decorators.Num())
		{
			OutError = FString::Printf(TEXT("Decorator index %d out of range"), DecoIndex);
			return false;
		}

		Parent->Children[ChildIndex].Decorators.RemoveAt(DecoIndex);
		RebuildTree(BehaviorTree);
		BehaviorTree->MarkPackageDirty();
		return true;
	}

	if (NodePath.Contains(TEXT(".service.")))
	{
		// Parse: nodePath.service.index
		FString BasePath, SvcIndexStr;
		NodePath.Split(TEXT(".service."), &BasePath, &SvcIndexStr);
		int32 SvcIndex = FCString::Atoi(*SvcIndexStr);

		UBTNode* Node = ResolveNodePath(BehaviorTree, BasePath, OutError);
		if (!Node) return false;

		if (UBTCompositeNode* Comp = Cast<UBTCompositeNode>(Node))
		{
			if (SvcIndex >= 0 && SvcIndex < Comp->Services.Num())
			{
				Comp->Services.RemoveAt(SvcIndex);
				RebuildTree(BehaviorTree);
				BehaviorTree->MarkPackageDirty();
				return true;
			}
		}
		else if (UBTTaskNode* Task = Cast<UBTTaskNode>(Node))
		{
			if (SvcIndex >= 0 && SvcIndex < Task->Services.Num())
			{
				Task->Services.RemoveAt(SvcIndex);
				RebuildTree(BehaviorTree);
				BehaviorTree->MarkPackageDirty();
				return true;
			}
		}

		OutError = FString::Printf(TEXT("Service index %d out of range"), SvcIndex);
		return false;
	}

	// Regular child node removal
	int32 LastSlash;
	if (!NodePath.FindLastChar(TEXT('/'), LastSlash))
	{
		OutError = FString::Printf(TEXT("Invalid node path: '%s'"), *NodePath);
		return false;
	}

	FString ParentPath = NodePath.Left(LastSlash);
	int32 ChildIndex = FCString::Atoi(*NodePath.Mid(LastSlash + 1));

	UBTCompositeNode* ParentNode = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
	if (!ParentNode)
	{
		return false;
	}

	if (ChildIndex < 0 || ChildIndex >= ParentNode->Children.Num())
	{
		OutError = FString::Printf(TEXT("Child index %d out of range (parent has %d children)"),
			ChildIndex, ParentNode->Children.Num());
		return false;
	}

	ParentNode->Children.RemoveAt(ChildIndex);
	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Removed node at '%s'"), *NodePath);
	return true;
}

bool FBehaviorTreeEditor::MoveNode(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	int32 NewIndex,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return false;
	}

	int32 LastSlash;
	if (!NodePath.FindLastChar(TEXT('/'), LastSlash))
	{
		OutError = FString::Printf(TEXT("Invalid node path: '%s'"), *NodePath);
		return false;
	}

	FString ParentPath = NodePath.Left(LastSlash);
	int32 OldIndex = FCString::Atoi(*NodePath.Mid(LastSlash + 1));

	UBTCompositeNode* ParentNode = ResolveCompositeNodePath(BehaviorTree, ParentPath, OutError);
	if (!ParentNode)
	{
		return false;
	}

	if (OldIndex < 0 || OldIndex >= ParentNode->Children.Num())
	{
		OutError = FString::Printf(TEXT("Source index %d out of range"), OldIndex);
		return false;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, ParentNode->Children.Num() - 1);

	if (OldIndex == NewIndex)
	{
		return true; // Nothing to do
	}

	// Move by removing and reinserting
	FBTCompositeChild Child = ParentNode->Children[OldIndex];
	ParentNode->Children.RemoveAt(OldIndex);
	ParentNode->Children.Insert(Child, NewIndex);

	RebuildTree(BehaviorTree);
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Moved node from index %d to %d"), OldIndex, NewIndex);
	return true;
}

bool FBehaviorTreeEditor::SetNodeProperty(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	const FString& PropertyName,
	const FString& PropertyValue,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return false;
	}

	UBTNode* Node = ResolveNodePath(BehaviorTree, NodePath, OutError);
	if (!Node)
	{
		return false;
	}

	// Use UE reflection to set the property
	FProperty* Property = Node->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property '%s' not found on node class '%s'"),
			*PropertyName, *Node->GetClass()->GetName());
		return false;
	}

	// Import text value into the property
	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Node);
	if (!Property->ImportText_Direct(*PropertyValue, PropertyAddr, Node, PPF_None))
	{
		OutError = FString::Printf(TEXT("Failed to set property '%s' to value '%s'"),
			*PropertyName, *PropertyValue);
		return false;
	}

	BehaviorTree->MarkPackageDirty();
	UE_LOG(LogUnrealClaude, Log, TEXT("Set property '%s' = '%s' on node at '%s'"),
		*PropertyName, *PropertyValue, *NodePath);
	return true;
}

bool FBehaviorTreeEditor::SetBlackboardKeySelector(
	UBehaviorTree* BehaviorTree,
	const FString& NodePath,
	const FString& PropertyName,
	const FString& KeyName,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return false;
	}

	UBTNode* Node = ResolveNodePath(BehaviorTree, NodePath, OutError);
	if (!Node)
	{
		return false;
	}

	// Find FBlackboardKeySelector property
	FStructProperty* StructProp = nullptr;
	for (TFieldIterator<FStructProperty> It(Node->GetClass()); It; ++It)
	{
		if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
		{
			if (It->Struct && It->Struct->GetName() == TEXT("BlackboardKeySelector"))
			{
				StructProp = *It;
				break;
			}
		}
	}

	if (!StructProp)
	{
		OutError = FString::Printf(TEXT("BlackboardKeySelector property '%s' not found on node"), *PropertyName);
		return false;
	}

	// Set the key name and resolve the key ID
	FBlackboardKeySelector* Selector = StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
	if (!Selector)
	{
		OutError = TEXT("Failed to access BlackboardKeySelector");
		return false;
	}

	Selector->SelectedKeyName = FName(*KeyName);

	// Resolve the SelectedKeyID from the Blackboard asset so the node is actually valid at runtime
	UBlackboardData* BB = BehaviorTree->BlackboardAsset;
	if (BB)
	{
		// Search through all keys (including parent blackboard keys)
		bool bKeyFound = false;
		const UBlackboardData* CurrentBB = BB;
		while (CurrentBB)
		{
			for (int32 KeyIdx = 0; KeyIdx < CurrentBB->Keys.Num(); KeyIdx++)
			{
				if (CurrentBB->Keys[KeyIdx].EntryName.ToString().Equals(KeyName, ESearchCase::IgnoreCase))
				{
					// Calculate absolute key ID (parent keys come first)
					int32 AbsoluteKeyID = KeyIdx;
					const UBlackboardData* ParentBB = CurrentBB->Parent;
					while (ParentBB)
					{
						AbsoluteKeyID += ParentBB->Keys.Num();
						ParentBB = ParentBB->Parent;
					}

					Selector->SelectedKeyID = static_cast<uint8>(AbsoluteKeyID);

					// Also set the allowed key type if the selector has type filters
					if (CurrentBB->Keys[KeyIdx].KeyType)
					{
						Selector->AllowedTypes.Empty();
						Selector->AllowedTypes.Add(CurrentBB->Keys[KeyIdx].KeyType->GetClass());
					}

					bKeyFound = true;
					break;
				}
			}
			if (bKeyFound) break;
			CurrentBB = CurrentBB->Parent;
		}

		if (!bKeyFound)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("Blackboard key '%s' not found in BB '%s'. SelectedKeyName set but SelectedKeyID will be invalid."),
				*KeyName, *BB->GetName());
		}

		// Call InitializeFromAsset on the node to let it resolve internal references
		Node->InitializeFromAsset(*BehaviorTree);
	}
	else
	{
		UE_LOG(LogUnrealClaude, Warning, TEXT("No Blackboard asset on BT - SelectedKeyName set but SelectedKeyID cannot be resolved"));
	}

	BehaviorTree->MarkPackageDirty();
	UE_LOG(LogUnrealClaude, Log, TEXT("Set BlackboardKeySelector '%s' to key '%s' (KeyID: %d)"),
		*PropertyName, *KeyName, Selector->SelectedKeyID);
	return true;
}

bool FBehaviorTreeEditor::ConnectToBlackboard(
	UBehaviorTree* BehaviorTree,
	UBlackboardData* Blackboard,
	FString& OutError)
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return false;
	}
	if (!Blackboard)
	{
		OutError = TEXT("Blackboard is null");
		return false;
	}

	BehaviorTree->BlackboardAsset = Blackboard;
	BehaviorTree->MarkPackageDirty();

	UE_LOG(LogUnrealClaude, Log, TEXT("Connected BT '%s' to Blackboard '%s'"),
		*BehaviorTree->GetName(), *Blackboard->GetName());
	return true;
}

// ===== Navigation =====

UBTNode* FBehaviorTreeEditor::ResolveNodePath(UBehaviorTree* BehaviorTree, const FString& Path, FString& OutError)
{
	if (!BehaviorTree || !BehaviorTree->RootNode)
	{
		OutError = TEXT("Behavior Tree or root node is null");
		return nullptr;
	}

	if (Path.IsEmpty() || Path.Equals(TEXT("root"), ESearchCase::IgnoreCase))
	{
		return BehaviorTree->RootNode;
	}

	// Remove "root/" prefix
	FString WorkingPath = Path;
	if (WorkingPath.StartsWith(TEXT("root/"), ESearchCase::IgnoreCase))
	{
		WorkingPath = WorkingPath.Mid(5);
	}
	else if (WorkingPath.StartsWith(TEXT("root"), ESearchCase::IgnoreCase))
	{
		return BehaviorTree->RootNode;
	}

	// Navigate through children
	UBTCompositeNode* CurrentComposite = BehaviorTree->RootNode;

	TArray<FString> PathParts;
	WorkingPath.ParseIntoArray(PathParts, TEXT("/"));

	for (int32 PartIdx = 0; PartIdx < PathParts.Num(); PartIdx++)
	{
		FString Part = PathParts[PartIdx];

		// Check for decorator/service suffix
		if (Part.Contains(TEXT(".decorator.")))
		{
			FString IndexStr, DecoIndexStr;
			Part.Split(TEXT(".decorator."), &IndexStr, &DecoIndexStr);
			int32 ChildIdx = FCString::Atoi(*IndexStr);
			int32 DecoIdx = FCString::Atoi(*DecoIndexStr);

			if (!CurrentComposite || ChildIdx < 0 || ChildIdx >= CurrentComposite->Children.Num())
			{
				OutError = FString::Printf(TEXT("Invalid child index %d at path '%s'"), ChildIdx, *Path);
				return nullptr;
			}

			if (DecoIdx < 0 || DecoIdx >= CurrentComposite->Children[ChildIdx].Decorators.Num())
			{
				OutError = FString::Printf(TEXT("Decorator index %d out of range at path '%s'"), DecoIdx, *Path);
				return nullptr;
			}

			return CurrentComposite->Children[ChildIdx].Decorators[DecoIdx];
		}

		if (Part.Contains(TEXT(".service.")))
		{
			FString IndexStr, SvcIndexStr;
			Part.Split(TEXT(".service."), &IndexStr, &SvcIndexStr);
			int32 ChildIdx = FCString::Atoi(*IndexStr);
			int32 SvcIdx = FCString::Atoi(*SvcIndexStr);

			if (!CurrentComposite || ChildIdx < 0 || ChildIdx >= CurrentComposite->Children.Num())
			{
				OutError = FString::Printf(TEXT("Invalid child index %d at path '%s'"), ChildIdx, *Path);
				return nullptr;
			}

			// Service on composite child
			const FBTCompositeChild& Child = CurrentComposite->Children[ChildIdx];
			if (Child.ChildComposite && SvcIdx < Child.ChildComposite->Services.Num())
			{
				return Child.ChildComposite->Services[SvcIdx];
			}
			if (Child.ChildTask && SvcIdx < Child.ChildTask->Services.Num())
			{
				return Child.ChildTask->Services[SvcIdx];
			}

			OutError = FString::Printf(TEXT("Service index %d out of range at path '%s'"), SvcIdx, *Path);
			return nullptr;
		}

		// Regular child index
		int32 ChildIndex = FCString::Atoi(*Part);
		if (!CurrentComposite)
		{
			OutError = FString::Printf(TEXT("Cannot navigate further at path '%s' - current node is not a composite"),
				*Path);
			return nullptr;
		}

		if (ChildIndex < 0 || ChildIndex >= CurrentComposite->Children.Num())
		{
			OutError = FString::Printf(TEXT("Child index %d out of range at '%s' (has %d children)"),
				ChildIndex, *Path, CurrentComposite->Children.Num());
			return nullptr;
		}

		const FBTCompositeChild& Child = CurrentComposite->Children[ChildIndex];

		// If this is the last part, return the child node
		if (PartIdx == PathParts.Num() - 1)
		{
			if (Child.ChildComposite)
			{
				return Child.ChildComposite;
			}
			return Child.ChildTask;
		}

		// Need to continue navigating - must be a composite
		if (!Child.ChildComposite)
		{
			OutError = FString::Printf(TEXT("Child at index %d is a task node, cannot navigate further (path: '%s')"),
				ChildIndex, *Path);
			return nullptr;
		}

		CurrentComposite = Child.ChildComposite;
	}

	return CurrentComposite;
}

UBTCompositeNode* FBehaviorTreeEditor::ResolveCompositeNodePath(UBehaviorTree* BehaviorTree, const FString& Path, FString& OutError)
{
	UBTNode* Node = ResolveNodePath(BehaviorTree, Path, OutError);
	if (!Node)
	{
		return nullptr;
	}

	UBTCompositeNode* Composite = Cast<UBTCompositeNode>(Node);
	if (!Composite)
	{
		OutError = FString::Printf(TEXT("Node at path '%s' is not a composite node (class: %s)"),
			*Path, *Node->GetClass()->GetName());
		return nullptr;
	}

	return Composite;
}

// ===== Helpers =====

UClass* FBehaviorTreeEditor::FindBTNodeClass(const FString& NodeName, UClass* BaseClass, FString& OutError)
{
	if (NodeName.IsEmpty())
	{
		OutError = TEXT("Node class name is empty");
		return nullptr;
	}

	// Well-known names mapping
	struct FKnownNode { const TCHAR* ShortName; UClass* (*GetClass)(); };

	// Composites
	static const FKnownNode KnownComposites[] = {
		{ TEXT("Selector"),        []() -> UClass* { return UBTComposite_Selector::StaticClass(); } },
		{ TEXT("Sequence"),        []() -> UClass* { return UBTComposite_Sequence::StaticClass(); } },
		{ TEXT("SimpleParallel"),  []() -> UClass* { return UBTComposite_SimpleParallel::StaticClass(); } },
		{ TEXT("Parallel"),        []() -> UClass* { return UBTComposite_SimpleParallel::StaticClass(); } },
	};

	// Tasks
	static const FKnownNode KnownTasks[] = {
		{ TEXT("Wait"),            []() -> UClass* { return UBTTask_Wait::StaticClass(); } },
		{ TEXT("MoveTo"),          []() -> UClass* { return UBTTask_MoveTo::StaticClass(); } },
		{ TEXT("RunBehavior"),     []() -> UClass* { return UBTTask_RunBehavior::StaticClass(); } },
		{ TEXT("PlayAnimation"),   []() -> UClass* { return UBTTask_PlayAnimation::StaticClass(); } },
	};

	// Decorators
	static const FKnownNode KnownDecorators[] = {
		{ TEXT("Blackboard"),      []() -> UClass* { return UBTDecorator_Blackboard::StaticClass(); } },
		{ TEXT("ForceSuccess"),    []() -> UClass* { return UBTDecorator_ForceSuccess::StaticClass(); } },
		{ TEXT("Loop"),            []() -> UClass* { return UBTDecorator_Loop::StaticClass(); } },
		{ TEXT("TimeLimit"),       []() -> UClass* { return UBTDecorator_TimeLimit::StaticClass(); } },
		{ TEXT("Cooldown"),        []() -> UClass* { return UBTDecorator_Cooldown::StaticClass(); } },
	};

	// Services
	static const FKnownNode KnownServices[] = {
		{ TEXT("BlackboardBase"),   []() -> UClass* { return UBTService_BlackboardBase::StaticClass(); } },
		{ TEXT("DefaultFocus"),     []() -> UClass* { return UBTService_DefaultFocus::StaticClass(); } },
	};

	// Check well-known names based on base class
	auto SearchKnown = [&](const FKnownNode* List, int32 Count) -> UClass*
	{
		for (int32 i = 0; i < Count; i++)
		{
			if (NodeName.Equals(List[i].ShortName, ESearchCase::IgnoreCase))
			{
				return List[i].GetClass();
			}
		}
		return nullptr;
	};

	UClass* Found = nullptr;

	if (BaseClass == UBTCompositeNode::StaticClass() || !BaseClass)
	{
		Found = SearchKnown(KnownComposites, UE_ARRAY_COUNT(KnownComposites));
		if (Found) return Found;
	}
	if (BaseClass == UBTTaskNode::StaticClass() || !BaseClass)
	{
		Found = SearchKnown(KnownTasks, UE_ARRAY_COUNT(KnownTasks));
		if (Found) return Found;
	}
	if (BaseClass == UBTDecorator::StaticClass() || !BaseClass)
	{
		Found = SearchKnown(KnownDecorators, UE_ARRAY_COUNT(KnownDecorators));
		if (Found) return Found;
	}
	if (BaseClass == UBTService::StaticClass() || !BaseClass)
	{
		Found = SearchKnown(KnownServices, UE_ARRAY_COUNT(KnownServices));
		if (Found) return Found;
	}

	// Try with common prefixes
	TArray<FString> Candidates;
	Candidates.Add(NodeName);
	Candidates.Add(FString::Printf(TEXT("BTComposite_%s"), *NodeName));
	Candidates.Add(FString::Printf(TEXT("BTTask_%s"), *NodeName));
	Candidates.Add(FString::Printf(TEXT("BTDecorator_%s"), *NodeName));
	Candidates.Add(FString::Printf(TEXT("BTService_%s"), *NodeName));
	Candidates.Add(FString::Printf(TEXT("U%s"), *NodeName));

	// Search all loaded classes
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		if (!TestClass || TestClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		if (BaseClass && !TestClass->IsChildOf(BaseClass))
		{
			continue;
		}

		// Must at least be a BT node
		if (!TestClass->IsChildOf(UBTNode::StaticClass()))
		{
			continue;
		}

		FString TestName = TestClass->GetName();
		for (const FString& Candidate : Candidates)
		{
			if (TestName.Equals(Candidate, ESearchCase::IgnoreCase))
			{
				return TestClass;
			}
		}
	}

	OutError = FString::Printf(TEXT("BT node class '%s' not found. For composites use: Selector, Sequence, SimpleParallel. "
		"For tasks use: Wait, MoveTo, RunBehavior. For decorators use: Blackboard, ForceSuccess, Loop, TimeLimit, Cooldown."),
		*NodeName);
	return nullptr;
}

void FBehaviorTreeEditor::RebuildTree(UBehaviorTree* BehaviorTree)
{
	if (!BehaviorTree || !BehaviorTree->RootNode)
	{
		return;
	}

	int32 Index = 0;
	AssignExecutionIndices(BehaviorTree->RootNode, Index);
}

void FBehaviorTreeEditor::AssignExecutionIndices(UBTCompositeNode* Node, int32& Index)
{
	if (!Node)
	{
		return;
	}

	Node->ForceInstancing(false);

	for (int32 i = 0; i < Node->Children.Num(); i++)
	{
		const FBTCompositeChild& Child = Node->Children[i];

		// Decorators
		for (UBTDecorator* Deco : Child.Decorators)
		{
			if (Deco)
			{
				Index++;
			}
		}

		// The child node itself
		if (Child.ChildComposite)
		{
			Index++;
			// Services on composite
			for (UBTService* Svc : Child.ChildComposite->Services)
			{
				if (Svc)
				{
					Index++;
				}
			}
			AssignExecutionIndices(Child.ChildComposite, Index);
		}
		else if (Child.ChildTask)
		{
			Index++;
			// Services on task
			for (UBTService* Svc : Child.ChildTask->Services)
			{
				if (Svc)
				{
					Index++;
				}
			}
		}
	}
}

UClass* FBehaviorTreeEditor::ResolveBlackboardKeyType(const FString& TypeName, FString& OutError)
{
	struct FKeyTypeMapping { const TCHAR* Name; UClass* (*GetClass)(); };
	static const FKeyTypeMapping Mappings[] = {
		{ TEXT("Bool"),     []() -> UClass* { return UBlackboardKeyType_Bool::StaticClass(); } },
		{ TEXT("Int"),      []() -> UClass* { return UBlackboardKeyType_Int::StaticClass(); } },
		{ TEXT("Float"),    []() -> UClass* { return UBlackboardKeyType_Float::StaticClass(); } },
		{ TEXT("String"),   []() -> UClass* { return UBlackboardKeyType_String::StaticClass(); } },
		{ TEXT("Name"),     []() -> UClass* { return UBlackboardKeyType_Name::StaticClass(); } },
		{ TEXT("Vector"),   []() -> UClass* { return UBlackboardKeyType_Vector::StaticClass(); } },
		{ TEXT("Rotator"),  []() -> UClass* { return UBlackboardKeyType_Rotator::StaticClass(); } },
		{ TEXT("Object"),   []() -> UClass* { return UBlackboardKeyType_Object::StaticClass(); } },
		{ TEXT("Class"),    []() -> UClass* { return UBlackboardKeyType_Class::StaticClass(); } },
		{ TEXT("Enum"),     []() -> UClass* { return UBlackboardKeyType_Enum::StaticClass(); } },
	};

	for (const FKeyTypeMapping& Mapping : Mappings)
	{
		if (TypeName.Equals(Mapping.Name, ESearchCase::IgnoreCase))
		{
			return Mapping.GetClass();
		}
	}

	OutError = FString::Printf(TEXT("Unknown Blackboard key type: '%s'. Valid types: Bool, Int, Float, String, Name, Vector, Rotator, Object, Class, Enum"),
		*TypeName);
	return nullptr;
}
