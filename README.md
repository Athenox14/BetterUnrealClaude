# Better UnrealClaude

> **Performance-optimized fork of [UnrealClaude](https://github.com/Natfii/UnrealClaude)** by Natfii
> Created by [Athenox Development](https://athenox.dev)

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?style=flat&logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win64-0078D6?style=flat&logo=windows&logoColor=white)
![Rust](https://img.shields.io/badge/MCP%20Bridge-Rust-DEA584?style=flat&logo=rust&logoColor=white)
![Claude Code](https://img.shields.io/badge/Claude%20Code-Integration-D97757?style=flat&logo=anthropic&logoColor=white)
![MCP](https://img.shields.io/badge/MCP-21%20Tools-8A2BE2?style=flat)
![License](https://img.shields.io/badge/License-MIT-green?style=flat)
[![Hosted by OxalisHeberg](https://img.shields.io/badge/Hosted%20by-OxalisHeberg-FF6600?style=flat&logo=server)](https://oxalisheberg.fr)
[![Sponsor on GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?style=for-the-badge&logo=github)](https://github.com/sponsors/Athenox14)

**Enhanced Claude Code CLI integration for Unreal Engine 5.7** with dynamic Blueprint node discovery, optimized token usage, and advanced workflow validation.

> **Windows Only** - This plugin uses Windows-specific process APIs.

## What's New in Better UnrealClaude

This fork adds **major performance and reliability improvements** to the original UnrealClaude plugin:

### Key Enhancements

- **Dynamic Blueprint Node Discovery** - Zero hardcoding! Auto-discovers all Blueprint nodes (functions, events, math ops) via reflection
- **Native Rust MCP Bridge** - No Node.js required. Single binary, instant startup
- **73% Token Reduction** - Optimized MCP tool descriptions and context
- **Smart Batch Validation** - Pre-validates node references before execution, catching errors early
- **Behavior Tree Support** - Full BT/Blackboard creation and modification tools
- **3-Step Blueprint Workflow** - Documented best practices for reliable node creation
- **Intelligent Error Messages** - Failed pin connections now suggest available pins
- **Caching System** - 30s TTL cache for Blueprint metadata queries (+40% speed)
- **Batching system** - Improving speed dramatically (-60% tool usage)

---

## Overview

Better UnrealClaude integrates the [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) directly into the Unreal Engine 5.7 Editor. Instead of using the API directly, this plugin shells out to the `claude` command-line tool, leveraging your existing Claude Code authentication and capabilities.

**Core Features:**
- **21 MCP Tools** - Model Context Protocol server for comprehensive editor control
- **Dynamic UE 5.7 Context System** - Accurate API documentation on demand
- **Blueprint Editing** - Create and modify Blueprints with validated workflows
- **Behavior Trees** - Full BT/Blackboard support with composite/task/decorator nodes
- **Animation Blueprints** - State machine editing (states, transitions, conditions)
- **Level Management** - Open, create, and manage levels programmatically
- **Asset Management** - Search, dependencies, and referencer queries
- **Async Task Queue** - Long-running operations with progress tracking
- **Script Execution** - Write, compile (via Live Coding), and execute scripts
- **Session Persistence** - Conversation history across editor sessions
- **Project-Aware** - Auto-gathers project context and viewport info
- **Claude Code Auth** - No separate API key management

---

## Prerequisites

### 1. Install Claude Code CLI

```bash
npm install -g @anthropic-ai/claude-code
```

### 2. Authenticate Claude Code

```bash
claude auth login
```

This will open a browser window to authenticate with your Anthropic account (Claude Pro/Max subscription) or set up API access.

### 3. Verify Installation

```bash
claude --version
claude -p "Hello, can you see me?"
```

---

## Installation

(Check the Editor category in the plugin browser. You might need to scroll down if search doesn't pick it up)

### Option A: Download Release (Recommended)

1. Download the latest `.zip` from [GitHub Releases](https://github.com/Athenox14/BetterUnrealClaude/releases)
2. Extract the `UnrealClaude` folder to your project's `Plugins` directory:
   ```
   YourProject/
   ├── Content/
   ├── Source/
   └── Plugins/
       └── UnrealClaude/
           ├── Source/
           ├── Resources/
           │   └── mcp-bridge/
           │       ├── ue5-mcp-server.exe   # Pre-compiled MCP bridge
           │       └── contexts/            # UE5 API documentation
           ├── Config/
           └── UnrealClaude.uplugin
   ```
3. Launch the editor — the plugin compiles and loads automatically

### Option B: Engine Plugin (All Projects)

Copy to your engine's plugins folder:
```
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\UnrealClaude\
```

### Option C: Building from Source

If you need to rebuild (different UE version, modifications, etc.):

**C++ plugin:**
```bash
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64
```

**MCP bridge (Rust):**
```bash
cd UnrealClaude/Resources/mcp-bridge
cargo build --release
copy target\release\ue5-mcp-server.exe .
```

---

## Usage

### Opening the Claude Panel

**Menu → Tools → Claude Assistant**

### Example Prompts

```
How do I create a custom Actor Component in C++?

What's the best way to implement a health system using GAS?

Explain World Partition and how to set up streaming for an open world.

Write a BlueprintCallable function that spawns particles at a location.

How do I properly use TObjectPtr<> vs raw pointers in UE5.7?

Create a Behavior Tree for an AI enemy with patrol and chase states.

Add a Delay node to this Blueprint function and connect it to a print statement.
```

## Features

### Session Persistence

Conversations are automatically saved by Claude Code, you can resume it with `/resume`.

### Project Context

Better UnrealClaude automatically gathers information about your project:
- Source modules and their dependencies
- Enabled plugins
- Project settings
- Recent assets
- Custom CLAUDE.md instructions

---

## MCP Server & Tools

The plugin includes a Model Context Protocol (MCP) server with **21 specialized tools** that expose editor functionality to Claude. The server runs on port 3000 by default and starts automatically when the editor loads.

### Blueprint Tools

**blueprint_query** (Read-only)
- `list` - List all Blueprints with filters
- `inspect` - Get detailed Blueprint info (variables, functions, graphs)
- `get_graph` - Get graph structure and nodes
- **`search`** - Dynamically discover Blueprint nodes
- **`get_node_pins`** - Get exact pin layout for functions
- **`list_libraries`** - List all function libraries
- **`list_library_functions`** - List functions in a library
- **`search_events`** - Dynamically discover Event nodes (BeginPlay, Overlap, etc.)

**blueprint_modify**
- `create` - Create new Blueprint
- `add_variable` / `remove_variable`
- `add_function` / `remove_function`
- `add_component` - Add actor components
- `add_node` - Create Blueprint nodes
- `delete_node` - Remove nodes
- `connect_pins` / `disconnect_pins`
- `set_pin_value` - Set default pin values
- **`batch`** - Multi-operation execution

> **New Workflow:** See [Blueprint Workflow Guide](UnrealClaude/Resources/mcp-bridge/contexts/blueprint_workflow.md) for the recommended 3-step approach to creating Blueprint nodes.

### Behavior Tree Tools

**behavior_tree_modify**
- **Blackboard Operations:**
  - `create_blackboard` - Create new Blackboard Data asset
  - `get_blackboard_info` - Query Blackboard structure
  - `add_key` / `remove_key` / `rename_key` - Manage Blackboard keys
- **Behavior Tree Operations:**
  - `create_behavior_tree` - Create new BT with root node
  - `get_tree_info` - Get complete tree structure
  - `add_composite` - Add Selector/Sequence/Parallel nodes
  - `add_task` - Add task nodes (Wait, MoveTo, custom)
  - `add_decorator` / `add_service` - Attach decorators/services
  - `remove_node` / `move_node` - Tree manipulation
  - `set_node_property` - Configure node properties
  - `connect_to_blackboard` - Link BT to Blackboard
  - `batch` - Multi-operation execution

### Animation Blueprint Tools

**anim_blueprint_modify**
- State machine management
- State creation and configuration
- Transition setup (duration, priority)
- Condition nodes (TimeRemaining, Greater, And, Or, GetVariable)
- Batch operations

### Actor & Level Tools

**actor_query** / **actor_modify**
- Spawn, move, delete, inspect actors
- Set properties with reflection
- Component management

**level_management**
- Open levels
- Create from templates
- List available templates

**get_level_actors**
- Query actors by class/tag/name
- Pagination support

### Asset Tools

**asset_search**
- Search by path, class, tag
- Advanced filtering
- Pagination (default 10, max 1000)

**asset_relations**
- Get dependencies (hard/soft/recursive)
- Find referencers
- Asset graph traversal

**asset_modify**
- Create, delete, rename, duplicate assets
- Batch operations

### Material Tools

**material_modify**
- Material instance creation
- Parameter setting (scalar, vector, texture)
- Parent material queries

### Character & Input Tools

**character_management**
- Character configuration
- Movement settings
- Data asset operations
- Stats table queries

**enhanced_input**
- Input action creation
- Mapping context management
- Trigger and modifier setup

### Utility Tools

**editor_utils**
- Console command execution
- Output log queries
- Viewport capture
- Editor state info

**execute_script**
- Python/Blueprint script execution
- Live coding integration
- Result capture

**task_queue** (Async)
- Background task execution
- Progress tracking
- Long-running operations

---

## Dynamic UE 5.7 Context System

The MCP bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand. Use `unreal_get_ue_context` to query by category or search by keywords.

**Available Categories:**
- `animation` - Animation Blueprints, state machines, blend spaces
- `blueprint` - Blueprint editing, nodes, pins, graphs
- `slate` - UI/Slate widget development
- `actor` - Actor lifecycle, components, transforms
- `assets` - Asset management, references, loading
- `replication` - Network replication, RPCs
- `enhanced_input` - Enhanced Input system
- `material` - Material editing and instances
- `character` - Character movement and configuration
- `parallel_workflows` - Best practices for async operations

Context status is shown in `unreal_status` output.

---

## 🔍 Dynamic Node Discovery (New!)

Better UnrealClaude uses **zero hardcoding** for Blueprint nodes. All nodes are discovered dynamically via UE5 reflection:

**Functions & Math Ops:**
```json
{
  "operation": "search",
  "keyword": "Delay"
}
// Returns: KismetSystemLibrary::Delay with exact pins
```

**Events:**
```json
{
  "operation": "search_events",
  "keyword": "Overlap",
  "base_class": "Actor"
}
// Returns: ActorBeginOverlap, ActorEndOverlap, etc.
```

**Pin Layout:**
```json
{
  "operation": "get_node_pins",
  "function_reference": "KismetSystemLibrary::Delay"
}
// Returns: [execute (in), then (out), Duration (in), ReturnValue (out)]
```

This means:
- **Always up-to-date** - New UE5 functions automatically available
- **Project-aware** - Discovers custom Blueprint functions
- **No maintenance** - No need to update hardcoded lists

---

## 📋 3-Step Blueprint Workflow

To create Blueprint nodes reliably, follow this validated workflow:

### Step 1: Discover
Use `search` or `get_node_pins` to find exact function names and pin layouts:
```json
{"operation": "search", "keyword": "Delay"}
```

### Step 2: Create
Create nodes one by one, capturing the returned IDs:
```json
{"operation": "add_node", "node_type": "CallFunction", ...}
// Returns: {"node_id": "CallFunction_Delay_1"}
```

### Step 3: Connect
Connect using **exact IDs and pin names** from previous steps:
```json
{
  "operation": "connect_pins",
  "source_node_id": "CallFunction_Delay_1",
  "source_pin": "then",
  "target_node_id": "CallFunction_Print_2",
  "target_pin": "execute"
}
```

**Full documentation:** [Blueprint Workflow Guide](UnrealClaude/Resources/mcp-bridge/contexts/blueprint_workflow.md)

---

## Configuration

### Custom System Prompts

You can extend the built-in UE5.7 context by creating a `CLAUDE.md` file in your project root:

```markdown
# My Project Context

## Architecture
- This is a multiplayer survival game
- Using Dedicated Server model
- GAS for all abilities

## Coding Standards
- Always use UPROPERTY for Blueprint access
- Prefix interfaces with I (IInteractable)
- Use GameplayTags for ability identification
```

### Allowed Tools

By default, the plugin runs Claude with these tools: `Read`, `Write`, `Edit`, `Grep`, `Glob`, `Bash`. You can modify this in `ClaudeSubsystem.cpp`:

```cpp
Config.AllowedTools = { TEXT("Read"), TEXT("Grep"), TEXT("Glob") }; // Read-only
```

---

## How It Works

1. User enters a prompt in the editor widget
2. Plugin builds context from UE5.7 knowledge + project information
3. Executes: `claude -p --skip-permissions --append-system-prompt "..." "your prompt"`
4. Claude Code runs with your project as the working directory
5. MCP server provides 21 specialized tools for editor manipulation
6. Response is captured and displayed in the chat panel
7. Conversation is persisted for future sessions

### Command Line Equivalent

```bash
cd "C:\YourProject"
claude -p --skip-permissions \
  --allowedTools "Read,Write,Edit,Grep,Glob,Bash" \
  --append-system-prompt "You are an expert Unreal Engine 5.7 developer..." \
  "How do I create a custom GameMode?"
```

---

## Troubleshooting

### "Claude CLI not found"

1. Verify Claude is installed: `claude --version`
2. Check it's in your PATH: `where claude`
3. Restart Unreal Editor after installation

### "Authentication required"

Run `claude auth login` in a terminal to authenticate.

### Responses are slow

Claude Code executes in your project directory and may read files for context. Large projects may have slower initial responses. The caching system helps with repeated queries.

### Plugin doesn't compile

Ensure you're on Unreal Engine 5.7 for Windows. This plugin uses Windows-specific APIs.

### MCP Server not starting

Check if port 3000 is available. The MCP server logs to `LogUnrealClaude`.

### MCP tools not available / Blueprint tools not working

If Claude says the MCP tools are in its instructions but not in its function list:

1. **Verify the MCP bridge binary exists**: Check that `ue5-mcp-server.exe` is present at:
   ```
   YourProject/Plugins/UnrealClaude/Resources/mcp-bridge/ue5-mcp-server.exe
   ```
   If missing, download the latest release zip which includes the pre-compiled binary.

2. **Verify the HTTP server is running**: With the editor open, test:
   ```bash
   curl http://localhost:3000/mcp/status
   ```
   You should see a JSON response with project info.

3. **Check the Output Log**: Look for `LogUnrealClaude` messages:
   - `MCP Server started on http://localhost:3000` - Server is running
   - `Registered X MCP tools` - Tools are loaded

### Blueprint batch operations failing

If you see errors like "Node #5 not found":
- The plugin now **pre-validates** batch operations
- Remember: `#N` refers to the **N-th node created**, not the N-th operation
- See [Blueprint Workflow Guide](UnrealClaude/Resources/mcp-bridge/contexts/blueprint_workflow.md)

### Pin connection errors

When connections fail, the error now lists **available pins**:
```
Pin 'ReturnValue' not found on 'VariableGet_Delay_2'.
Available pins: [Delay (output)]
```

Use the suggested pins or query with `get_node_pins` first.

---

---

## Contributing

Feel free to fork for your own needs! Possible areas for improvement:

- [ ] Mac/Linux support
- [ ] Additional MCP tools
- [ ] Niagara system editing

---

## License

MIT License - See [LICENSE](UnrealClaude/LICENSE) file.

---

## Sponsors

[![GitHub Sponsors](https://img.shields.io/github/sponsors/Athenox14?style=for-the-badge&logo=github&color=ea4aaa)](https://github.com/sponsors/Athenox14)

**Support this project and get your name/logo here!**

<!-- sponsors --><!-- sponsors -->

---

**Hosted by:**

<a href="https://oxalisheberg.fr">
  <img src="https://img.shields.io/badge/OxalisHeberg-Fast%20%26%20Reliable%20French%20Hosting-FF6600?style=for-the-badge&logo=server" alt="OxalisHeberg">
</a>

---

## Credits

**Better UnrealClaude Fork:**
- Created by **[Athenox Development](https://athenox.dev)**
- GitHub: [github.com/Athenox14/BetterUnrealClaude](https://github.com/Athenox14/BetterUnrealClaude)
- Discord: [discord.gg/4efJMvCVX9](https://discord.gg/4efJMvCVX9)

**Original UnrealClaude:**
- Built by [Natfii](https://github.com/Natfii)
- GitHub: [github.com/Natfii/UnrealClaude](https://github.com/Natfii/UnrealClaude)

**Powered by:**
- Unreal Engine 5.7 by Epic Games
- [Claude Code](https://claude.ai/code) by Anthropic
- Model Context Protocol (MCP) by Anthropic