# Blueprint Node Creation Workflow

**Best Practice: 3-Step Approach for Reliable Blueprint Modification**

When creating Blueprint nodes programmatically via the MCP tools, follow this structured workflow to avoid errors and ensure correct connections:

## 🎯 The 3-Step Workflow

### Step 1: Discover (search_nodes)

**Before creating any nodes**, use `blueprint_query` with operation `search` or `get_node_pins` to discover:

- **Available functions and their exact names**
- **Pin names and types** (input/output, exec/data)
- **Target classes** for function calls
- **Function references** in `ClassName::FunctionName` format

**Example:**
```json
{
  "operation": "search",
  "keyword": "Delay"
}
// Returns: KismetSystemLibrary::Delay with exact pin layout
```

```json
{
  "operation": "get_node_pins",
  "function_reference": "KismetSystemLibrary::Delay"
}
// Returns: pins [execute (input), then (output), Duration (input), ReturnValue (output)]
```

### Step 2: Create (add_node)

**Create nodes one by one** and **capture the returned node IDs**:

```json
{
  "operation": "add_node",
  "graph_name": "MyFunction",
  "is_function_graph": true,
  "node_type": "CallFunction",
  "node_params": {
    "function": "KismetSystemLibrary::Delay",
    "target_class": "KismetSystemLibrary"
  },
  "pos_x": 300,
  "pos_y": 0
}
// Returns: { "node_id": "CallFunction_KismetSystemLibrary::Delay_1" }
```

**Important:**
- Store the returned `node_id` values
- Do NOT guess node IDs
- Nodes are auto-named: `{NodeType}_{Context}_{Counter}`

### Step 3: Connect (connect_pins)

**Connect using the real node IDs and exact pin names** from previous steps:

```json
{
  "operation": "connect_pins",
  "graph_name": "MyFunction",
  "is_function_graph": true,
  "source_node_id": "VariableGet_RespawnDelay_1",
  "source_pin": "Respawn Delay",  // Exact pin name from Step 1
  "target_node_id": "CallFunction_KismetSystemLibrary::Delay_2",
  "target_pin": "Duration"  // Exact pin name from Step 1
}
```

---

## ⚠️ Common Mistakes

### ❌ DON'T: Guess Function Names
```json
// BAD - function name is wrong
"function": "GetActorTransform"  // Not in static libraries!
```

✅ **DO: Search first**
```json
// GOOD - search for the function
operation: "search", keyword: "GetActorTransform"
// Discover: AActor::K2_GetActorLocation is correct
```

### ❌ DON'T: Guess Pin Names
```json
// BAD - pin name might be wrong
"source_pin": "ReturnValue"  // VariableGet doesn't have this!
```

✅ **DO: Get exact pin layout**
```json
// GOOD - query pins first
operation: "get_node_pins", function_reference: "KismetSystemLibrary::Delay"
// Returns exact pins: ["execute", "then", "Duration", "ReturnValue"]
```

### ❌ DON'T: Use #N references incorrectly in batch
```json
// BAD - #0 refers to first NODE created, not first OPERATION
{
  "operations": [
    {"op": "add_function", "function_name": "MyFunc"},  // Op 0 (no node)
    {"op": "add_node", "node_type": "Delay"},           // Op 1 (node #0)
    {"op": "connect_pins", "source_node_id": "#1"}      // ERROR: should be #0
  ]
}
```

✅ **DO: Count nodes, not operations**
```json
// GOOD - #0 refers to first created node
{
  "operations": [
    {"op": "add_function", "function_name": "MyFunc"},  // Op 0 (no node)
    {"op": "add_node", "node_type": "Delay"},           // Op 1 (creates node #0)
    {"op": "connect_pins", "source_node_id": "#0"}      // CORRECT
  ]
}
```

---

## 📦 Batch Operations Best Practices

When using `operation: "batch"`, temporary node references `#N` work as follows:

- `#0` = **first node created** (not first operation)
- `#1` = **second node created**
- `#N` = **(N+1)-th node created**

**Validation:** The plugin now pre-validates all `#N` references before execution and will error if you reference a node that won't be created.

**Example Error:**
```
Invalid node reference '#3' at operation 5: only 3 node(s) will be created. Use #0 to #2.
```

---

## 🔍 Pin Discovery on Error

If a connection fails, the error message now includes **available pins**:

**Old Error:**
```
Pin 'ReturnValue' not found on target node 'VariableGet_Respawn Delay_2'
```

**New Error:**
```
Pin 'ReturnValue' not found on target node 'VariableGet_Respawn Delay_2'.
Available pins: [Respawn Delay (output)]
```

This helps you quickly identify the correct pin name to use.

---

## 🎓 Complete Example

**Task:** Create a respawn delay system

**Step 1: Discover**
```json
// Search for Delay function
{"operation": "search", "keyword": "Delay"}
// Found: KismetSystemLibrary::Delay

// Get exact pins
{"operation": "get_node_pins", "function_reference": "KismetSystemLibrary::Delay"}
// Pins: execute (in), then (out), Duration (in), ReturnValue (out)
```

**Step 2: Create Nodes**
```json
// Create VariableGet
{"operation": "add_node", "node_type": "VariableGet", "node_params": {"variable": "RespawnDelay"}}
// Returns: "VariableGet_RespawnDelay_1"

// Create Delay
{"operation": "add_node", "node_type": "CallFunction", "node_params": {"function": "KismetSystemLibrary::Delay"}}
// Returns: "CallFunction_KismetSystemLibrary::Delay_2"
```

**Step 3: Connect**
```json
// Connect variable to delay duration
{
  "operation": "connect_pins",
  "source_node_id": "VariableGet_RespawnDelay_1",
  "source_pin": "RespawnDelay",  // Exact name from variable
  "target_node_id": "CallFunction_KismetSystemLibrary::Delay_2",
  "target_pin": "Duration"  // Exact name from Step 1
}
```

---

## 🛠️ Helper Tools

**FindNodeForKeyword (C++)**
```cpp
TOptional<FNodeSearchResult> Result = FBlueprintNodeSearcher::FindNodeForKeyword("Delay");
// Quick single-node lookup
```

**Improved Error Messages**
- Invalid `#N` references are caught before execution
- Pin not found errors list all available pins
- Node creation counter is separate from operation index

---

## 📋 Summary

✅ **Always search first** - Don't guess function or pin names
✅ **Capture node IDs** - Use the exact IDs returned from creation
✅ **Use exact pin names** - Query pins before connecting
✅ **Validate batch refs** - Remember `#N` counts nodes, not operations
✅ **Read error messages** - They now include available pins/nodes

Following this 3-step workflow eliminates most Blueprint modification errors!
