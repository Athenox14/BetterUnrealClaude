// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../MCPToolBase.h"

/**
 * MCP Tool: Set a property on a Blueprint component (CDO level)
 *
 * Unlike set_property which operates on actors in the scene,
 * this tool operates on component templates defined in the Blueprint Editor.
 * Changes affect the Class Default Object and all future instances.
 */
class FMCPTool_SetComponentProperty : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("set_component_property");
		Info.Description = TEXT(
			"Set a property on a Blueprint's component template (CDO level).\n\n"
			"Unlike 'set_property' which modifies actors already in the scene, this tool modifies "
			"the component defaults in the Blueprint itself. Changes affect all future instances.\n\n"
			"Examples:\n"
			"- Set CharacterMovement MaxWalkSpeed to 600\n"
			"- Set CapsuleComponent CapsuleHalfHeight to 96\n"
			"- Set Mesh SkeletalMesh to a specific asset\n"
			"- Set bOrientRotationToMovement on CharacterMovement\n\n"
			"The component_name can match by variable name or component class name.\n\n"
			"Returns: Confirmation of property change on the Blueprint component."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("blueprint_path"), TEXT("string"),
				TEXT("Path to the Blueprint asset"), true),
			FMCPToolParameter(TEXT("component_name"), TEXT("string"),
				TEXT("Name of the component (e.g., 'CharacterMovement', 'Mesh', 'CapsuleComponent')"), true),
			FMCPToolParameter(TEXT("property"), TEXT("string"),
				TEXT("Property path to set (e.g., 'MaxWalkSpeed', 'bOrientRotationToMovement')"), true),
			FMCPToolParameter(TEXT("value"), TEXT("any"),
				TEXT("Value to set (type depends on property)"), true)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	/** Navigate through a property path to find the target property on the component */
	bool NavigateToProperty(
		UObject* StartObject,
		const TArray<FString>& PathParts,
		UObject*& OutObject,
		FProperty*& OutProperty,
		FString& OutError);

	/** Set a numeric property value from JSON */
	bool SetNumericPropertyValue(FNumericProperty* NumProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Set a struct property value from JSON */
	bool SetStructPropertyValue(FStructProperty* StructProp, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);

	/** Set a property value from JSON using reflection */
	bool SetPropertyFromJson(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value, FString& OutError);
};
