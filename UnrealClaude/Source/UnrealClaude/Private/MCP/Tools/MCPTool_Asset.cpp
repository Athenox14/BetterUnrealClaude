// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_Asset.h"
#include "MCP/MCPParamValidator.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "UObject/PropertyAccessUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Dom/JsonValue.h"

FMCPToolInfo FMCPTool_Asset::GetInfo() const
{
	FMCPToolInfo Info;
	Info.Name = TEXT("asset");
	Info.Description = TEXT("Generic asset operations: set properties, save, search, and query asset relationships");

	Info.Parameters.Add(FMCPToolParameter(TEXT("operation"), TEXT("string"),
		TEXT("Operation: set_asset_property, save_asset, get_asset_info, search, dependencies, referencers"), true));

	Info.Parameters.Add(FMCPToolParameter(TEXT("asset_path"), TEXT("string"),
		TEXT("Asset path (e.g., /Game/Characters/MyMesh)"), false));

	Info.Parameters.Add(FMCPToolParameter(TEXT("property"), TEXT("string"),
		TEXT("Property path (e.g., Materials.0.MaterialInterface, bEnableGravity)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("value"), TEXT("any"),
		TEXT("Value to set (type must match property type)"), false));

	Info.Parameters.Add(FMCPToolParameter(TEXT("save"), TEXT("boolean"),
		TEXT("Actually save to disk (default: true)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("mark_dirty"), TEXT("boolean"),
		TEXT("Mark the asset as dirty (default: true if save is false)"), false));

	Info.Parameters.Add(FMCPToolParameter(TEXT("include_properties"), TEXT("boolean"),
		TEXT("Include editable property list (default: false)"), false));

	Info.Parameters.Add(FMCPToolParameter(TEXT("class_filter"), TEXT("string"),
		TEXT("[search] Filter by class name (e.g., SkeletalMesh, StaticMesh, Material)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("path_filter"), TEXT("string"),
		TEXT("[search] Path prefix to search within (default: /Game/)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("name_pattern"), TEXT("string"),
		TEXT("[search] Substring to match in asset names"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("limit"), TEXT("integer"),
		TEXT("[search/dependencies/referencers] Maximum results (1-1000, default: 25)"), false));
	Info.Parameters.Add(FMCPToolParameter(TEXT("offset"), TEXT("integer"),
		TEXT("[search/dependencies/referencers] Pagination offset (default: 0)"), false));

	Info.Parameters.Add(FMCPToolParameter(TEXT("include_soft"), TEXT("boolean"),
		TEXT("[dependencies/referencers] Include soft references (default: true)"), false));

	Info.Annotations = FMCPToolAnnotations::Modifying();

	return Info;
}

FMCPToolResult FMCPTool_Asset::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("set_asset_property"))
	{
		return ExecuteSetAssetProperty(Params);
	}
	else if (Operation == TEXT("save_asset"))
	{
		return ExecuteSaveAsset(Params);
	}
	else if (Operation == TEXT("get_asset_info"))
	{
		return ExecuteGetAssetInfo(Params);
	}
	else if (Operation == TEXT("search"))
	{
		return ExecuteSearch(Params);
	}
	else if (Operation == TEXT("dependencies"))
	{
		return ExecuteDependencies(Params);
	}
	else if (Operation == TEXT("referencers"))
	{
		return ExecuteReferencers(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: %s. Valid: set_asset_property, save_asset, get_asset_info, search, dependencies, referencers"),
		*Operation));
}

FMCPToolResult FMCPTool_Asset::ExecuteSetAssetProperty(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	FString PropertyPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("property"), PropertyPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidatePropertyPathParam(PropertyPath, Error))
	{
		return Error.GetValue();
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	FString PropertyError;
	if (!SetPropertyFromJson(Asset, PropertyPath, Value, PropertyError))
	{
		return FMCPToolResult::Error(PropertyError);
	}

	Asset->PostEditChange();
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetStringField(TEXT("property"), PropertyPath);
	ResultData->SetBoolField(TEXT("modified"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set property '%s' on asset '%s'"), *PropertyPath, *Asset->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Asset::ExecuteSaveAsset(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}

	bool bSave = true;
	if (Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}

	bool bMarkDirty = !bSave;
	if (Params->HasField(TEXT("mark_dirty")))
	{
		bMarkDirty = Params->GetBoolField(TEXT("mark_dirty"));
	}

	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	bool bWasSaved = false;
	bool bWasMarkedDirty = false;

	if (bMarkDirty)
	{
		Asset->MarkPackageDirty();
		bWasMarkedDirty = true;
	}

	if (bSave)
	{
		UPackage* Package = Asset->GetOutermost();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension()
		);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		FSavePackageResultStruct SaveResult = UPackage::Save(Package, Asset, *PackageFileName, SaveArgs);

		bWasSaved = SaveResult.IsSuccessful();
		if (!bWasSaved)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
		}
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetBoolField(TEXT("saved"), bWasSaved);
	ResultData->SetBoolField(TEXT("marked_dirty"), bWasMarkedDirty);

	FString Message = bWasSaved
		? FString::Printf(TEXT("Saved asset: %s"), *AssetPath)
		: FString::Printf(TEXT("Marked asset dirty: %s"), *AssetPath);

	return FMCPToolResult::Success(Message, ResultData);
}

FMCPToolResult FMCPTool_Asset::ExecuteGetAssetInfo(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;

	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}
	if (!ValidateBlueprintPathParam(AssetPath, Error))
	{
		return Error.GetValue();
	}

	bool bIncludeProperties = false;
	if (Params->HasField(TEXT("include_properties")))
	{
		bIncludeProperties = Params->GetBoolField(TEXT("include_properties"));
	}

	FString LoadError;
	UObject* Asset = LoadAssetByPath(AssetPath, LoadError);
	if (!Asset)
	{
		return FMCPToolResult::Error(LoadError);
	}

	TSharedPtr<FJsonObject> ResultData = BuildAssetInfoJson(Asset);

	if (bIncludeProperties)
	{
		ResultData->SetArrayField(TEXT("properties"), GetAssetProperties(Asset, true));
	}

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Asset info: %s"), *Asset->GetName()),
		ResultData
	);
}

FMCPToolResult FMCPTool_Asset::ExecuteSearch(const TSharedRef<FJsonObject>& Params)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString ClassFilter = ExtractOptionalString(Params, TEXT("class_filter"));
	FString PathFilter = ExtractOptionalString(Params, TEXT("path_filter"), TEXT("/Game/"));
	FString NamePattern = ExtractOptionalString(Params, TEXT("name_pattern"));
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
	}

	if (!ClassFilter.IsEmpty())
	{
		FString ClassPath = ClassFilter;
		if (!ClassPath.StartsWith(TEXT("/")))
		{
			UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassFilter));

			if (!FoundClass)
			{
				FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassFilter));
			}

			if (!FoundClass)
			{
				FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Niagara.%s"), *ClassFilter));
			}

			if (!FoundClass)
			{
				FoundClass = FindObject<UClass>(nullptr, *ClassFilter);
			}

			if (FoundClass)
			{
				ClassPath = FoundClass->GetClassPathName().ToString();
			}
			else
			{
				ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassFilter);
			}
		}

		Filter.ClassPaths.Add(FTopLevelAssetPath(ClassPath));
	}

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	TArray<FAssetData> FilteredAssets;
	if (!NamePattern.IsEmpty())
	{
		for (const FAssetData& Asset : AllAssets)
		{
			if (Asset.AssetName.ToString().Contains(NamePattern, ESearchCase::IgnoreCase))
			{
				FilteredAssets.Add(Asset);
			}
		}
	}
	else
	{
		FilteredAssets = MoveTemp(AllAssets);
	}

	int32 Total = FilteredAssets.Num();
	int32 StartIndex = FMath::Min(Offset, Total);
	int32 EndIndex = FMath::Min(StartIndex + Limit, Total);
	int32 Count = EndIndex - StartIndex;
	bool bHasMore = EndIndex < Total;

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(FilteredAssets[i])));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("assets"), AssetsArray);
	ResultData->SetNumberField(TEXT("count"), Count);
	ResultData->SetNumberField(TEXT("total"), Total);
	ResultData->SetNumberField(TEXT("offset"), StartIndex);
	ResultData->SetNumberField(TEXT("limit"), Limit);
	ResultData->SetBoolField(TEXT("hasMore"), bHasMore);
	if (bHasMore)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), EndIndex);
	}

	FString Message;
	if (Total == 0)
	{
		Message = TEXT("No assets found matching the search criteria");
	}
	else if (Count == Total)
	{
		Message = FString::Printf(TEXT("Found %d asset%s"), Total, Total == 1 ? TEXT("") : TEXT("s"));
	}
	else
	{
		Message = FString::Printf(TEXT("Found %d assets (showing %d-%d of %d total)"),
			Count, StartIndex + 1, EndIndex, Total);
	}

	return FMCPToolResult::Success(Message, ResultData);
}

FMCPToolResult FMCPTool_Asset::ExecuteDependencies(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteRelations(Params, true);
}

FMCPToolResult FMCPTool_Asset::ExecuteReferencers(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteRelations(Params, false);
}

FMCPToolResult FMCPTool_Asset::ExecuteRelations(const TSharedRef<FJsonObject>& Params, bool bIsDependencies)
{
	FString AssetPath;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}

	bool bIncludeSoft = ExtractOptionalBool(Params, TEXT("include_soft"), true);
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString PackagePath = AssetPath;
	if (PackagePath.Contains(TEXT(".")))
	{
		PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
	}

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
	{
		TArray<FAssetData> AssetsInPackage;
		AssetRegistry.GetAssetsByPackageName(FName(*PackagePath), AssetsInPackage);
		if (AssetsInPackage.Num() == 0)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		}
		AssetData = AssetsInPackage[0];
	}

	UE::AssetRegistry::FDependencyQuery QueryFlags;
	if (!bIncludeSoft)
	{
		QueryFlags = UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard);
	}

	TArray<FName> RelatedAssets;
	if (bIsDependencies)
	{
		AssetRegistry.GetDependencies(FName(*PackagePath), RelatedAssets,
			UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
	}
	else
	{
		AssetRegistry.GetReferencers(FName(*PackagePath), RelatedAssets,
			UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
	}

	TArray<FName> Filtered;
	for (const FName& Path : RelatedAssets)
	{
		FString PathStr = Path.ToString();
		if (!PathStr.StartsWith(TEXT("/Script/")) && !PathStr.StartsWith(TEXT("/Engine/")))
		{
			Filtered.Add(Path);
		}
	}

	int32 Total = Filtered.Num();
	int32 StartIndex = FMath::Min(Offset, Total);
	int32 EndIndex = FMath::Min(StartIndex + Limit, Total);
	int32 Count = EndIndex - StartIndex;
	bool bHasMore = EndIndex < Total;

	FString ResultArrayKey = bIsDependencies ? TEXT("dependencies") : TEXT("referencers");
	TArray<TSharedPtr<FJsonValue>> ResultArray;
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		TSharedPtr<FJsonObject> EntryJson = MakeShared<FJsonObject>();
		EntryJson->SetStringField(TEXT("path"), Filtered[i].ToString());

		TArray<FAssetData> EntryAssets;
		AssetRegistry.GetAssetsByPackageName(Filtered[i], EntryAssets);
		if (EntryAssets.Num() > 0)
		{
			EntryJson->SetStringField(TEXT("class"), EntryAssets[0].AssetClassPath.GetAssetName().ToString());
			EntryJson->SetStringField(TEXT("name"), EntryAssets[0].AssetName.ToString());
		}

		ResultArray.Add(MakeShared<FJsonValueObject>(EntryJson));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetStringField(TEXT("direction"), bIsDependencies ? TEXT("dependencies") : TEXT("referencers"));
	ResultData->SetArrayField(ResultArrayKey, ResultArray);
	ResultData->SetNumberField(TEXT("count"), Count);
	ResultData->SetNumberField(TEXT("total"), Total);
	ResultData->SetNumberField(TEXT("offset"), StartIndex);
	ResultData->SetNumberField(TEXT("limit"), Limit);
	ResultData->SetBoolField(TEXT("hasMore"), bHasMore);
	if (bHasMore)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), EndIndex);
	}
	ResultData->SetBoolField(TEXT("include_soft"), bIncludeSoft);

	FString TypeLabel = bIsDependencies ? TEXT("dependenc") : TEXT("referencer");
	FString Plural = bIsDependencies ? (Total == 1 ? TEXT("y") : TEXT("ies")) : (Total == 1 ? TEXT("") : TEXT("s"));
	FString Message;
	if (Total == 0 && !bIsDependencies)
	{
		Message = FString::Printf(TEXT("No referencers found for '%s' - this asset appears unused"),
			*AssetData.AssetName.ToString());
	}
	else if (Count == Total)
	{
		Message = FString::Printf(TEXT("Found %d %s%s for '%s'"),
			Total, *TypeLabel, *Plural, *AssetData.AssetName.ToString());
	}
	else
	{
		Message = FString::Printf(TEXT("Found %d %s%s (showing %d-%d of %d total) for '%s'"),
			Count, *TypeLabel, *Plural, StartIndex + 1, EndIndex, Total,
			*AssetData.AssetName.ToString());
	}

	return FMCPToolResult::Success(Message, ResultData);
}

UObject* FMCPTool_Asset::LoadAssetByPath(const FString& AssetPath, FString& OutError)
{
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		if (!AssetPath.EndsWith(TEXT("_C")))
		{
			Asset = LoadObject<UObject>(nullptr, *(AssetPath + TEXT("_C")));
		}
	}

	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath);
	}

	return Asset;
}

bool FMCPTool_Asset::NavigateToProperty(
	UObject* StartObject,
	const TArray<FString>& PathParts,
	UObject*& OutObject,
	FProperty*& OutProperty,
	FString& OutError)
{
	OutObject = StartObject;
	OutProperty = nullptr;

	for (int32 i = 0; i < PathParts.Num(); ++i)
	{
		const FString& PartName = PathParts[i];
		const bool bIsLastPart = (i == PathParts.Num() - 1);

		int32 ArrayIndex = INDEX_NONE;
		FString PropertyName = PartName;

		if (PartName.IsNumeric())
		{
			ArrayIndex = FCString::Atoi(*PartName);
			if (!OutProperty)
			{
				OutError = FString::Printf(TEXT("Cannot index without preceding array property"));
				return false;
			}

			FArrayProperty* ArrayProp = CastField<FArrayProperty>(OutProperty);
			if (!ArrayProp)
			{
				OutError = FString::Printf(TEXT("Property is not an array, cannot use index"));
				return false;
			}

			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(OutObject));
			if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
			{
				OutError = FString::Printf(TEXT("Array index %d out of bounds (size: %d)"), ArrayIndex, ArrayHelper.Num());
				return false;
			}

			FProperty* InnerProp = ArrayProp->Inner;
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(InnerProp))
			{
				void* ElementPtr = ArrayHelper.GetRawPtr(ArrayIndex);
				UObject* ElementObj = ObjProp->GetObjectPropertyValue(ElementPtr);
				if (ElementObj && !bIsLastPart)
				{
					OutObject = ElementObj;
					OutProperty = nullptr;
					continue;
				}
			}

			if (bIsLastPart)
			{
				OutProperty = InnerProp;
				return true;
			}
			continue;
		}

		OutProperty = OutObject->GetClass()->FindPropertyByName(FName(*PropertyName));

		if (!OutProperty)
		{
			OutError = FString::Printf(TEXT("Property not found: %s on %s"), *PropertyName, *OutObject->GetClass()->GetName());
			return false;
		}

		if (!bIsLastPart)
		{
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(OutProperty))
			{
				continue;
			}

			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(OutProperty))
			{
				UObject* NestedObj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(OutObject));
				if (!NestedObj)
				{
					OutError = FString::Printf(TEXT("Nested object is null: %s"), *PropertyName);
					return false;
				}
				OutObject = NestedObj;
				OutProperty = nullptr;
			}
			else if (FStructProperty* StructProp = CastField<FStructProperty>(OutProperty))
			{
				OutError = FString::Printf(TEXT("Cannot navigate into struct property: %s. Set the entire struct instead."), *PropertyName);
				return false;
			}
			else
			{
				OutError = FString::Printf(TEXT("Cannot navigate into property type: %s"), *PropertyName);
				return false;
			}
		}
	}

	return OutProperty != nullptr;
}

bool FMCPTool_Asset::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	if (!Object || !Value.IsValid())
	{
		OutError = TEXT("Invalid object or value");
		return false;
	}

	TArray<FString> PathParts;
	PropertyPath.ParseIntoArray(PathParts, TEXT("."), true);

	UObject* TargetObject = nullptr;
	FProperty* Property = nullptr;

	if (!NavigateToProperty(Object, PathParts, TargetObject, Property, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Property not found: %s"), *PropertyPath);
		}
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return SetObjectPropertyValue(ObjProp, ValuePtr, Value, OutError);
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (SetNumericPropertyValue(NumProp, ValuePtr, Value))
		{
			return true;
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			return true;
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (SetStructPropertyValue(StructProp, ValuePtr, Value))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Unsupported property type for: %s"), *PropertyPath);
	return false;
}

bool FMCPTool_Asset::SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	if (NumProp->IsFloatingPoint())
	{
		double DoubleVal = 0.0;
		if (Value->TryGetNumber(DoubleVal))
		{
			NumProp->SetFloatingPointPropertyValue(ValuePtr, DoubleVal);
			return true;
		}
	}
	else if (NumProp->IsInteger())
	{
		int64 IntVal = 0;
		if (Value->TryGetNumber(IntVal))
		{
			NumProp->SetIntPropertyValue(ValuePtr, IntVal);
			return true;
		}
	}
	return false;
}

bool FMCPTool_Asset::SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	const TSharedPtr<FJsonObject>* ObjVal;
	if (!Value->TryGetObject(ObjVal))
	{
		return false;
	}

	if (StructProp->Struct == TBaseStructure<FVector>::Get())
	{
		FVector Vec;
		(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
		(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
		(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		*reinterpret_cast<FVector*>(ValuePtr) = Vec;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FRotator>::Get())
	{
		FRotator Rot;
		(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
		return true;
	}

	if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
	{
		FLinearColor Color;
		(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A);
		*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
		return true;
	}

	return false;
}

bool FMCPTool_Asset::SetObjectPropertyValue(FObjectProperty* ObjProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	FString ObjectPath;
	if (!Value->TryGetString(ObjectPath))
	{
		OutError = TEXT("Object property value must be a string path");
		return false;
	}

	if (ObjectPath.IsEmpty() || ObjectPath.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
		return true;
	}

	UObject* ReferencedObject = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!ReferencedObject)
	{
		OutError = FString::Printf(TEXT("Failed to load object: %s"), *ObjectPath);
		return false;
	}

	if (!ReferencedObject->IsA(ObjProp->PropertyClass))
	{
		OutError = FString::Printf(TEXT("Object type mismatch. Expected %s, got %s"),
			*ObjProp->PropertyClass->GetName(), *ReferencedObject->GetClass()->GetName());
		return false;
	}

	ObjProp->SetObjectPropertyValue(ValuePtr, ReferencedObject);
	return true;
}

TSharedPtr<FJsonObject> FMCPTool_Asset::BuildAssetInfoJson(UObject* Asset)
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();

	Info->SetStringField(TEXT("name"), Asset->GetName());
	Info->SetStringField(TEXT("path"), Asset->GetPathName());
	Info->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
	Info->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());

	UPackage* Package = Asset->GetOutermost();
	Info->SetBoolField(TEXT("is_dirty"), Package->IsDirty());

	if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Asset))
	{
		TArray<TSharedPtr<FJsonValue>> MaterialsArr;
		const TArray<FSkeletalMaterial>& Materials = SkelMesh->GetMaterials();
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			TSharedPtr<FJsonObject> MatObj = MakeShared<FJsonObject>();
			MatObj->SetNumberField(TEXT("slot"), i);
			MatObj->SetStringField(TEXT("slot_name"), Materials[i].MaterialSlotName.ToString());
			MatObj->SetStringField(TEXT("material"),
				Materials[i].MaterialInterface ? Materials[i].MaterialInterface->GetPathName() : TEXT("None"));
			MaterialsArr.Add(MakeShared<FJsonValueObject>(MatObj));
		}
		Info->SetArrayField(TEXT("materials"), MaterialsArr);
	}

	return Info;
}

TArray<TSharedPtr<FJsonValue>> FMCPTool_Asset::GetAssetProperties(UObject* Asset, bool bEditableOnly)
{
	TArray<TSharedPtr<FJsonValue>> PropsArray;

	for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		if (bEditableOnly && !Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());

		FString TypeStr;
		if (CastField<FNumericProperty>(Property))
		{
			TypeStr = Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>()
				? TEXT("float") : TEXT("integer");
		}
		else if (CastField<FBoolProperty>(Property))
		{
			TypeStr = TEXT("bool");
		}
		else if (CastField<FStrProperty>(Property))
		{
			TypeStr = TEXT("string");
		}
		else if (CastField<FNameProperty>(Property))
		{
			TypeStr = TEXT("name");
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			TypeStr = FString::Printf(TEXT("struct:%s"), *StructProp->Struct->GetName());
		}
		else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			TypeStr = FString::Printf(TEXT("object:%s"), *ObjProp->PropertyClass->GetName());
		}
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			TypeStr = TEXT("array");
		}
		else
		{
			TypeStr = TEXT("other");
		}

		PropObj->SetStringField(TEXT("type"), TypeStr);
		PropObj->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	return PropsArray;
}

TSharedPtr<FJsonObject> FMCPTool_Asset::AssetDataToJson(const FAssetData& AssetData) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	Json->SetStringField(TEXT("path"), AssetData.GetObjectPathString());

	Json->SetStringField(TEXT("name"), AssetData.AssetName.ToString());

	Json->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	Json->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());

	return Json;
}
