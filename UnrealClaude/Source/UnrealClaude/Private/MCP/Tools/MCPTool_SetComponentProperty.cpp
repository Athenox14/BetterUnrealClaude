// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SetComponentProperty.h"
#include "MCP/MCPParamValidator.h"
#include "MCP/MCPBlueprintLoadContext.h"
#include "BlueprintUtils.h"
#include "UnrealClaudeModule.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

FMCPToolResult FMCPTool_SetComponentProperty::Execute(const TSharedRef<FJsonObject>& Params)
{
	// Extract parameters
	TOptional<FMCPToolResult> ParamError;

	FString ComponentName;
	if (!ExtractRequiredString(Params, TEXT("component_name"), ComponentName, ParamError))
	{
		return ParamError.GetValue();
	}

	FString PropertyPath;
	if (!ExtractRequiredString(Params, TEXT("property"), PropertyPath, ParamError))
	{
		return ParamError.GetValue();
	}

	if (!Params->HasField(TEXT("value")))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: value"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	// Load and validate Blueprint
	FMCPBlueprintLoadContext Context;
	if (auto LoadError = Context.LoadAndValidate(Params))
	{
		return LoadError.GetValue();
	}

	// Get SimpleConstructionScript
	USimpleConstructionScript* SCS = Context.Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return FMCPToolResult::Error(TEXT("Blueprint does not have a SimpleConstructionScript (not an Actor Blueprint?)"));
	}

	// Find the component by name
	UActorComponent* ComponentTemplate = nullptr;
	FString FoundComponentName;

	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (!Node || !Node->ComponentTemplate)
		{
			continue;
		}

		FString VarName = Node->GetVariableName().ToString();
		FString TemplateName = Node->ComponentTemplate->GetName();
		FString ClassName = Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("");

		// Match by variable name, template name, or class name
		if (VarName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
			VarName.Contains(ComponentName) ||
			TemplateName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
			TemplateName.Contains(ComponentName) ||
			ClassName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
			ClassName.Contains(ComponentName))
		{
			ComponentTemplate = Node->ComponentTemplate;
			FoundComponentName = VarName;
			break;
		}
	}

	// Also check inherited components from the CDO
	if (!ComponentTemplate)
	{
		UClass* GeneratedClass = Context.Blueprint->GeneratedClass;
		if (GeneratedClass)
		{
			UObject* CDO = GeneratedClass->GetDefaultObject();
			if (CDO)
			{
				AActor* ActorCDO = Cast<AActor>(CDO);
				if (ActorCDO)
				{
					for (UActorComponent* Comp : ActorCDO->GetComponents())
					{
						if (!Comp) continue;
						FString CompName = Comp->GetName();
						FString CompClassName = Comp->GetClass()->GetName();

						if (CompName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
							CompName.Contains(ComponentName) ||
							CompClassName.Equals(ComponentName, ESearchCase::IgnoreCase) ||
							CompClassName.Contains(ComponentName))
						{
							ComponentTemplate = Comp;
							FoundComponentName = CompName;
							break;
						}
					}
				}
			}
		}
	}

	if (!ComponentTemplate)
	{
		// Build helpful error message listing available components
		TArray<FString> AvailableComponents;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate)
			{
				AvailableComponents.Add(FString::Printf(TEXT("%s (%s)"),
					*Node->GetVariableName().ToString(),
					*Node->ComponentTemplate->GetClass()->GetName()));
			}
		}

		FString AvailableList = FString::Join(AvailableComponents, TEXT(", "));
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Component '%s' not found. Available: [%s]"),
			*ComponentName, *AvailableList));
	}

	// Set the property using reflection
	FString ErrorMessage;
	if (!SetPropertyFromJson(ComponentTemplate, PropertyPath, Value, ErrorMessage))
	{
		return FMCPToolResult::Error(ErrorMessage);
	}

	// Compile and finalize
	if (auto CompileError = Context.CompileAndFinalize(TEXT("Component property set")))
	{
		return CompileError.GetValue();
	}

	// Build result
	TSharedPtr<FJsonObject> ResultData = Context.BuildResultJson();
	ResultData->SetStringField(TEXT("component"), FoundComponentName);
	ResultData->SetStringField(TEXT("property"), PropertyPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Set '%s' on component '%s'"), *PropertyPath, *FoundComponentName),
		ResultData
	);
}

bool FMCPTool_SetComponentProperty::NavigateToProperty(
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

		OutProperty = OutObject->GetClass()->FindPropertyByName(FName(*PartName));

		if (!OutProperty)
		{
			OutError = FString::Printf(TEXT("Property not found: %s on %s"), *PartName, *OutObject->GetClass()->GetName());
			return false;
		}

		if (!bIsLastPart)
		{
			// Navigate into nested object
			FObjectProperty* ObjProp = CastField<FObjectProperty>(OutProperty);
			if (!ObjProp)
			{
				OutError = FString::Printf(TEXT("Cannot navigate into non-object property: %s"), *PartName);
				return false;
			}

			UObject* NestedObj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(OutObject));
			if (!NestedObj)
			{
				OutError = FString::Printf(TEXT("Nested object is null: %s"), *PartName);
				return false;
			}

			OutObject = NestedObj;
			OutProperty = nullptr;
		}
	}

	return OutProperty != nullptr;
}

bool FMCPTool_SetComponentProperty::SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
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

bool FMCPTool_SetComponentProperty::SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)
{
	const FName StructName = StructProp->Struct->GetFName();

	const bool bIsVector = (StructProp->Struct == TBaseStructure<FVector>::Get())
		|| (StructName == FName("Vector"));
	const bool bIsRotator = (StructProp->Struct == TBaseStructure<FRotator>::Get())
		|| (StructName == FName("Rotator"));
	const bool bIsLinearColor = (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		|| (StructName == FName("LinearColor"));
	const bool bIsFColor = (StructProp->Struct == TBaseStructure<FColor>::Get())
		|| (StructName == FName("Color"));

	// Try string format first (hex colors, UE text format)
	FString StringValue;
	if (Value->TryGetString(StringValue))
	{
		const TCHAR* ImportResult = StructProp->ImportText_Direct(*StringValue, ValuePtr, nullptr, 0);
		if (ImportResult != nullptr)
		{
			return true;
		}
	}

	// Try object format
	const TSharedPtr<FJsonObject>* ObjVal;
	if (!Value->TryGetObject(ObjVal))
	{
		return false;
	}

	if (bIsVector)
	{
		FVector Vec;
		(*ObjVal)->TryGetNumberField(TEXT("x"), Vec.X);
		(*ObjVal)->TryGetNumberField(TEXT("y"), Vec.Y);
		(*ObjVal)->TryGetNumberField(TEXT("z"), Vec.Z);
		*reinterpret_cast<FVector*>(ValuePtr) = Vec;
		return true;
	}

	if (bIsRotator)
	{
		FRotator Rot;
		(*ObjVal)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*ObjVal)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*ObjVal)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		*reinterpret_cast<FRotator*>(ValuePtr) = Rot;
		return true;
	}

	if (bIsLinearColor)
	{
		FLinearColor Color;
		(*ObjVal)->TryGetNumberField(TEXT("r"), Color.R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), Color.G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), Color.B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("a"), Color.A))
		{
			Color.A = 1.0f;
		}
		if (Color.R > 1.5f || Color.G > 1.5f || Color.B > 1.5f)
		{
			Color.R /= 255.0f;
			Color.G /= 255.0f;
			Color.B /= 255.0f;
			if (Color.A > 1.5f) Color.A /= 255.0f;
		}
		*reinterpret_cast<FLinearColor*>(ValuePtr) = Color;
		return true;
	}

	if (bIsFColor)
	{
		double R = 0, G = 0, B = 0, A = 255;
		(*ObjVal)->TryGetNumberField(TEXT("r"), R);
		(*ObjVal)->TryGetNumberField(TEXT("g"), G);
		(*ObjVal)->TryGetNumberField(TEXT("b"), B);
		if (!(*ObjVal)->TryGetNumberField(TEXT("a"), A)) A = 255.0;
		FColor Color;
		Color.R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R), 0, 255));
		Color.G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G), 0, 255));
		Color.B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B), 0, 255));
		Color.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(A), 0, 255));
		*reinterpret_cast<FColor*>(ValuePtr) = Color;
		return true;
	}

	// Generic fallback using ImportText
	{
		FString TextRepresentation = TEXT("(");
		bool bFirst = true;
		for (const auto& Pair : (*ObjVal)->Values)
		{
			if (!bFirst) TextRepresentation += TEXT(",");
			TextRepresentation += Pair.Key.ToUpper() + TEXT("=");
			double NumVal;
			FString StrVal;
			if (Pair.Value->TryGetNumber(NumVal))
				TextRepresentation += FString::SanitizeFloat(NumVal);
			else if (Pair.Value->TryGetString(StrVal))
				TextRepresentation += StrVal;
			bFirst = false;
		}
		TextRepresentation += TEXT(")");
		const TCHAR* ImportResult = StructProp->ImportText_Direct(*TextRepresentation, ValuePtr, nullptr, 0);
		if (ImportResult != nullptr)
		{
			return true;
		}
	}

	return false;
}

bool FMCPTool_SetComponentProperty::SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError)
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

	// Numeric
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (SetNumericPropertyValue(NumProp, ValuePtr, Value))
		{
			return true;
		}
	}
	// Bool
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool BoolVal = false;
		if (Value->TryGetBool(BoolVal))
		{
			BoolProp->SetPropertyValue(ValuePtr, BoolVal);
			return true;
		}
	}
	// String
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			StrProp->SetPropertyValue(ValuePtr, StrVal);
			return true;
		}
	}
	// Name
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString StrVal;
		if (Value->TryGetString(StrVal))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
			return true;
		}
	}
	// Struct
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (SetStructPropertyValue(StructProp, ValuePtr, Value))
		{
			return true;
		}
		OutError = FString::Printf(TEXT("Failed to set struct property '%s' (type: F%s)"),
			*PropertyPath, *StructProp->Struct->GetName());
		return false;
	}

	OutError = FString::Printf(TEXT("Unsupported property type '%s' for: %s"),
		*Property->GetCPPType(), *PropertyPath);
	return false;
}
