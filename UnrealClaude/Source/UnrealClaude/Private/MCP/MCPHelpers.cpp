// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPHelpers.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Color.h"
#include "Math/Transform.h"

// ===== FMCPPagination =====

FMCPPagination FMCPPagination::Extract(const TSharedRef<FJsonObject>& Params, int32 DefaultLimit, int32 MaxLimit)
{
	FMCPPagination Result;

	double LimitValue = DefaultLimit;
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	Result.Limit = FMath::Clamp(static_cast<int32>(LimitValue), 1, MaxLimit);

	double OffsetValue = 0;
	Params->TryGetNumberField(TEXT("offset"), OffsetValue);
	Result.Offset = FMath::Max(0, static_cast<int32>(OffsetValue));

	return Result;
}

void FMCPPagination::CalculateIndices(int32 TotalCount, int32& OutStartIndex, int32& OutEndIndex, int32& OutCount, bool& OutHasMore) const
{
	OutStartIndex = FMath::Min(Offset, TotalCount);
	OutEndIndex = FMath::Min(OutStartIndex + Limit, TotalCount);
	OutCount = OutEndIndex - OutStartIndex;
	OutHasMore = OutEndIndex < TotalCount;
}

void FMCPPagination::BuildJsonFields(TSharedPtr<FJsonObject>& ResultData, int32 TotalCount, int32 ReturnedCount) const
{
	if (!ResultData.IsValid())
	{
		return;
	}

	ResultData->SetNumberField(TEXT("count"), ReturnedCount);
	ResultData->SetNumberField(TEXT("total"), TotalCount);
	ResultData->SetNumberField(TEXT("offset"), Offset);
	ResultData->SetNumberField(TEXT("limit"), Limit);

	int32 StartIndex, EndIndex, Count;
	bool bHasMore;
	CalculateIndices(TotalCount, StartIndex, EndIndex, Count, bHasMore);

	ResultData->SetBoolField(TEXT("hasMore"), bHasMore);
	if (bHasMore)
	{
		ResultData->SetNumberField(TEXT("nextOffset"), EndIndex);
	}
}

// ===== FMCPJsonStructs =====

bool FMCPJsonStructs::SetStructFromJson(const FString& StructTypeName, void* StructPtr, const TSharedPtr<FJsonValue>& JsonValue)
{
	if (!StructPtr || !JsonValue.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* JsonObj;
	if (!JsonValue->TryGetObject(JsonObj) || !JsonObj->IsValid())
	{
		return false;
	}

	if (StructTypeName == TEXT("Vector"))
	{
		return SetVectorFromJson(StructPtr, *JsonObj);
	}
	else if (StructTypeName == TEXT("Rotator"))
	{
		return SetRotatorFromJson(StructPtr, *JsonObj);
	}
	else if (StructTypeName == TEXT("LinearColor"))
	{
		return SetLinearColorFromJson(StructPtr, *JsonObj);
	}
	else if (StructTypeName == TEXT("Color"))
	{
		return SetColorFromJson(StructPtr, *JsonObj);
	}
	else if (StructTypeName == TEXT("Transform"))
	{
		return SetTransformFromJson(StructPtr, *JsonObj);
	}

	return false;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::GetStructAsJson(const FString& StructTypeName, const void* StructPtr)
{
	if (!StructPtr)
	{
		return nullptr;
	}

	if (StructTypeName == TEXT("Vector"))
	{
		return VectorToJson(StructPtr);
	}
	else if (StructTypeName == TEXT("Rotator"))
	{
		return RotatorToJson(StructPtr);
	}
	else if (StructTypeName == TEXT("LinearColor"))
	{
		return LinearColorToJson(StructPtr);
	}
	else if (StructTypeName == TEXT("Color"))
	{
		return ColorToJson(StructPtr);
	}
	else if (StructTypeName == TEXT("Transform"))
	{
		return TransformToJson(StructPtr);
	}

	return nullptr;
}

// Private helpers

bool FMCPJsonStructs::SetVectorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj)
{
	FVector* Vec = static_cast<FVector*>(StructPtr);
	JsonObj->TryGetNumberField(TEXT("x"), Vec->X);
	JsonObj->TryGetNumberField(TEXT("y"), Vec->Y);
	JsonObj->TryGetNumberField(TEXT("z"), Vec->Z);
	return true;
}

bool FMCPJsonStructs::SetRotatorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj)
{
	FRotator* Rot = static_cast<FRotator*>(StructPtr);
	JsonObj->TryGetNumberField(TEXT("pitch"), Rot->Pitch);
	JsonObj->TryGetNumberField(TEXT("yaw"), Rot->Yaw);
	JsonObj->TryGetNumberField(TEXT("roll"), Rot->Roll);
	return true;
}

bool FMCPJsonStructs::SetLinearColorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj)
{
	FLinearColor* Color = static_cast<FLinearColor*>(StructPtr);
	JsonObj->TryGetNumberField(TEXT("r"), Color->R);
	JsonObj->TryGetNumberField(TEXT("g"), Color->G);
	JsonObj->TryGetNumberField(TEXT("b"), Color->B);
	JsonObj->TryGetNumberField(TEXT("a"), Color->A);
	return true;
}

bool FMCPJsonStructs::SetColorFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj)
{
	FColor* Color = static_cast<FColor*>(StructPtr);
	double R, G, B, A = 255;
	JsonObj->TryGetNumberField(TEXT("r"), R);
	JsonObj->TryGetNumberField(TEXT("g"), G);
	JsonObj->TryGetNumberField(TEXT("b"), B);
	JsonObj->TryGetNumberField(TEXT("a"), A);
	Color->R = static_cast<uint8>(FMath::Clamp(R, 0.0, 255.0));
	Color->G = static_cast<uint8>(FMath::Clamp(G, 0.0, 255.0));
	Color->B = static_cast<uint8>(FMath::Clamp(B, 0.0, 255.0));
	Color->A = static_cast<uint8>(FMath::Clamp(A, 0.0, 255.0));
	return true;
}

bool FMCPJsonStructs::SetTransformFromJson(void* StructPtr, const TSharedPtr<FJsonObject>& JsonObj)
{
	FTransform* Transform = static_cast<FTransform*>(StructPtr);

	const TSharedPtr<FJsonObject>* LocationObj;
	if (JsonObj->TryGetObjectField(TEXT("location"), LocationObj))
	{
		FVector Location;
		SetVectorFromJson(&Location, *LocationObj);
		Transform->SetLocation(Location);
	}

	const TSharedPtr<FJsonObject>* RotationObj;
	if (JsonObj->TryGetObjectField(TEXT("rotation"), RotationObj))
	{
		FRotator Rotation;
		SetRotatorFromJson(&Rotation, *RotationObj);
		Transform->SetRotation(FQuat(Rotation));
	}

	const TSharedPtr<FJsonObject>* ScaleObj;
	if (JsonObj->TryGetObjectField(TEXT("scale"), ScaleObj))
	{
		FVector Scale;
		SetVectorFromJson(&Scale, *ScaleObj);
		Transform->SetScale3D(Scale);
	}

	return true;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::VectorToJson(const void* StructPtr)
{
	const FVector* Vec = static_cast<const FVector*>(StructPtr);
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Vec->X);
	Json->SetNumberField(TEXT("y"), Vec->Y);
	Json->SetNumberField(TEXT("z"), Vec->Z);
	return Json;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::RotatorToJson(const void* StructPtr)
{
	const FRotator* Rot = static_cast<const FRotator*>(StructPtr);
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("pitch"), Rot->Pitch);
	Json->SetNumberField(TEXT("yaw"), Rot->Yaw);
	Json->SetNumberField(TEXT("roll"), Rot->Roll);
	return Json;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::LinearColorToJson(const void* StructPtr)
{
	const FLinearColor* Color = static_cast<const FLinearColor*>(StructPtr);
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("r"), Color->R);
	Json->SetNumberField(TEXT("g"), Color->G);
	Json->SetNumberField(TEXT("b"), Color->B);
	Json->SetNumberField(TEXT("a"), Color->A);
	return Json;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::ColorToJson(const void* StructPtr)
{
	const FColor* Color = static_cast<const FColor*>(StructPtr);
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("r"), Color->R);
	Json->SetNumberField(TEXT("g"), Color->G);
	Json->SetNumberField(TEXT("b"), Color->B);
	Json->SetNumberField(TEXT("a"), Color->A);
	return Json;
}

TSharedPtr<FJsonObject> FMCPJsonStructs::TransformToJson(const void* StructPtr)
{
	const FTransform* Transform = static_cast<const FTransform*>(StructPtr);
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	FVector Location = Transform->GetLocation();
	Json->SetObjectField(TEXT("location"), VectorToJson(&Location));

	FRotator Rotation = Transform->GetRotation().Rotator();
	Json->SetObjectField(TEXT("rotation"), RotatorToJson(&Rotation));

	FVector Scale = Transform->GetScale3D();
	Json->SetObjectField(TEXT("scale"), VectorToJson(&Scale));

	return Json;
}
