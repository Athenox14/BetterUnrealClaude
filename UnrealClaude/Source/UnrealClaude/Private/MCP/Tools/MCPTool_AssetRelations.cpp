// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_AssetRelations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FMCPToolResult FMCPTool_AssetRelations::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract operation
	FString Operation;
	TOptional<FMCPToolResult> Error;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, Error))
	{
		return Error.GetValue();
	}
	Operation = Operation.ToLower();

	bool bIsDependencies = (Operation == TEXT("dependencies"));
	bool bIsReferencers = (Operation == TEXT("referencers"));
	if (!bIsDependencies && !bIsReferencers)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Unknown operation: '%s'. Valid: 'dependencies', 'referencers'"), *Operation));
	}

	// Extract required asset_path
	FString AssetPath;
	if (!ExtractRequiredString(Params, TEXT("asset_path"), AssetPath, Error))
	{
		return Error.GetValue();
	}

	// Extract optional parameters
	bool bIncludeSoft = ExtractOptionalBool(Params, TEXT("include_soft"), true);
	int32 Limit = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("limit"), 25), 1, 1000);
	int32 Offset = FMath::Max(0, ExtractOptionalNumber<int32>(Params, TEXT("offset"), 0));

	// Get AssetRegistry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Normalize path
	FString PackagePath = AssetPath;
	if (PackagePath.Contains(TEXT(".")))
	{
		PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
	}

	// Verify asset exists
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

	// Build query flags
	UE::AssetRegistry::FDependencyQuery QueryFlags;
	if (!bIncludeSoft)
	{
		QueryFlags = UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard);
	}

	// Query relationships
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

	// Filter out engine/script packages
	TArray<FName> Filtered;
	for (const FName& Path : RelatedAssets)
	{
		FString PathStr = Path.ToString();
		if (!PathStr.StartsWith(TEXT("/Script/")) && !PathStr.StartsWith(TEXT("/Engine/")))
		{
			Filtered.Add(Path);
		}
	}

	// Pagination
	int32 Total = Filtered.Num();
	int32 StartIndex = FMath::Min(Offset, Total);
	int32 EndIndex = FMath::Min(StartIndex + Limit, Total);
	int32 Count = EndIndex - StartIndex;
	bool bHasMore = EndIndex < Total;

	// Build result array
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

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("asset_path"), AssetPath);
	ResultData->SetStringField(TEXT("direction"), Operation);
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

	// Build message
	FString TypeLabel = bIsDependencies ? TEXT("dependenc") : TEXT("referencer");
	FString Plural = bIsDependencies ? (Total == 1 ? TEXT("y") : TEXT("ies")) : (Total == 1 ? TEXT("") : TEXT("s"));
	FString Message;
	if (Total == 0 && bIsReferencers)
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
