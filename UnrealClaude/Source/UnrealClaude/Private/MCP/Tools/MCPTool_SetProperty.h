// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Set a property on an actor or Blueprint component template
 *
 * Targets:
 * - 'actor' (default): Modify an actor in the current scene
 * - 'blueprint_component': Modify a component template in a Blueprint (CDO level)
 */
class FMCPTool_SetProperty : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("set_property");
		Info.Description = TEXT(
			"Set property values on actors or Blueprint component templates.\n\n"
			"Targets:\n"
			"- 'actor' (default): Modify scene actor. Use dot notation for components "
			"(e.g., 'LightComponent.Intensity', 'StaticMeshComponent.RelativeScale3D').\n"
			"- 'blueprint_component': Modify component defaults in Blueprint CDO. "
			"Changes affect all future instances.\n\n"
			"Value types: strings, numbers, booleans, objects (FVector, FRotator, FLinearColor), hex colors.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("target"), TEXT("string"),
				TEXT("Target: 'actor' (default) or 'blueprint_component'"), false, TEXT("actor")),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"),
				TEXT("Actor name (required for 'actor' target)"), false),
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Blueprint path (required for 'blueprint_component' target)"), false),
			FMCPToolParameter(TEXT("component_name"), TEXT("string"),
				TEXT("Component name (required for 'blueprint_component' target)"), false),
			FMCPToolParameter(TEXT("property"), TEXT("string"),
				TEXT("Property path (e.g., 'MaxWalkSpeed', 'LightComponent.Intensity')"), true),
			FMCPToolParameter(TEXT("value"), TEXT("any"),
				TEXT("Value to set (type depends on property)"), true)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Execute on a scene actor */
	FMCPToolResult ExecuteOnActor(const TSharedRef<FJsonObject>& Params);

	/** Execute on a Blueprint component template */
	FMCPToolResult ExecuteOnBlueprintComponent(const TSharedRef<FJsonObject>& Params);

	/** Navigate through a property path to find the target object and property */
	bool NavigateToProperty(
		UObject* StartObject,
		const TArray<FString>& PathParts,
		UObject*& OutObject,
		FProperty*& OutProperty,
		FString& OutError);

	/** Try to navigate into a component on an actor */
	bool TryNavigateToComponent(
		UObject*& CurrentObject,
		const FString& PartName,
		bool bIsLastPart,
		FString& OutError);

	/** Navigate into a nested object property */
	bool NavigateIntoNestedObject(
		UObject*& CurrentObject,
		FProperty* Property,
		const FString& PartName,
		FString& OutError);

	/** Set a numeric property value from JSON */
	bool SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Set a struct property value from JSON (FVector, FRotator, FLinearColor) */
	bool SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Helper to set a property value from JSON */
	bool SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError);
};
