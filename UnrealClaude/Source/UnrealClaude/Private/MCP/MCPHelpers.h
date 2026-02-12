// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Pagination helper for MCP tools
 * Eliminates duplication of limit/offset/hasMore logic across 8+ tools
 */
struct UNREALCLAUDE_API FMCPPagination
{
	int32 Limit = 25;
	int32 Offset = 0;

	/** Extract pagination params from JSON request */
	static FMCPPagination Extract(const TSharedRef<FJsonObject>& Params, int32 DefaultLimit = 25, int32 MaxLimit = 1000);

	/** Calculate pagination indices for an array */
	void CalculateIndices(int32 TotalCount, int32& OutStartIndex, int32& OutEndIndex, int32& OutCount, bool& OutHasMore) const;

	/** Build pagination fields in JSON result */
	void BuildJsonFields(TSharedPtr<FJsonObject>& ResultData, int32 TotalCount, int32 ReturnedCount) const;
};

/**
 * JSON-to-Struct serialization helper
 * Handles common UE struct types (FVector, FRotator, FLinearColor, FTransform, FColor)
 */
class UNREALCLAUDE_API FMCPJsonStructs
{
public:
	/** Convert JSON value to UE struct, returns true if successful */
	static bool SetStructFromJson(const FString& StructTypeName, void* StructPtr, const TSharedPtr<FJsonValue>& JsonValue);

	/** Convert UE struct to JSON object */
	static TSharedPtr<FJsonObject> GetStructAsJson(const FString& StructTypeName, const void* StructPtr);

private:
	static bool SetVectorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj);
	static bool SetRotatorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj);
	static bool SetLinearColorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj);
	static bool SetColorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj);
	static bool SetTransformFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj);

	static TSharedPtr<FJsonObject> VectorToJson(const void* StructPtr);
	static TSharedPtr<FJsonObject> RotatorToJson(const void* StructPtr);
	static TSharedPtr<FJsonObject> LinearColorToJson(const void* StructPtr);
	static TSharedPtr<FJsonObject> ColorToJson(const void* StructPtr);
	static TSharedPtr<FJsonObject> TransformToJson(const void* StructPtr);
};
