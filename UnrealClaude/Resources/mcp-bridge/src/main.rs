mod config;
mod context;
mod unreal;

use config::Config;
use context::ContextLoader;
use serde_json::{json, Value};
use std::io::{self, BufRead, Write};
use unreal::UnrealClient;

const SERVER_NAME: &str = "ue5-mcp-server";
const SERVER_VERSION: &str = "1.4.0";

/// Tools that use async execution (task queue) regardless of global setting
const ASYNC_TOOLS: &[&str] = &["execute_script"];

fn log(level: &str, msg: &str, data: Option<&Value>) {
    match data {
        Some(d) => eprintln!("[{}] {} {}", level, msg, d),
        None => eprintln!("[{}] {}", level, msg),
    }
}

#[tokio::main]
async fn main() {
    let config = Config::load();
    let client = UnrealClient::new(&config);
    let contexts_dir = ContextLoader::find_contexts_dir();
    let ctx = ContextLoader::new(contexts_dir);

    let categories = ctx.list_categories();
    log(
        "INFO",
        "UE5 MCP Server started",
        Some(&json!({
            "version": SERVER_VERSION,
            "unrealUrl": &config.unreal_mcp_url,
            "timeoutMs": config.request_timeout_ms,
            "asyncEnabled": config.async_enabled,
            "contextInjection": config.inject_context,
            "contextCategories": categories,
        })),
    );

    let stdin = io::stdin();
    let stdout = io::stdout();

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => break,
        };

        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }

        let request: Value = match serde_json::from_str(trimmed) {
            Ok(v) => v,
            Err(e) => {
                log("ERROR", "Failed to parse JSON-RPC request", Some(&json!({"error": e.to_string()})));
                continue;
            }
        };

        let id = request.get("id").cloned();
        let method = request["method"].as_str().unwrap_or("");

        // Notifications (no id) don't need a response
        if id.is_none() {
            continue;
        }

        let result = match method {
            "initialize" => handle_initialize(&request),
            "tools/list" => handle_list_tools(&client, &ctx).await,
            "tools/call" => handle_call_tool(&client, &ctx, &config, &request).await,
            "ping" => Ok(json!({})),
            _ => Err(json!({
                "code": -32601,
                "message": format!("Method not found: {}", method)
            })),
        };

        let response = match result {
            Ok(res) => json!({
                "jsonrpc": "2.0",
                "id": id,
                "result": res
            }),
            Err(err) => json!({
                "jsonrpc": "2.0",
                "id": id,
                "error": err
            }),
        };

        let mut out = stdout.lock();
        let _ = serde_json::to_writer(&mut out, &response);
        let _ = out.write_all(b"\n");
        let _ = out.flush();
    }
}

fn handle_initialize(_request: &Value) -> Result<Value, Value> {
    Ok(json!({
        "protocolVersion": "2024-11-05",
        "capabilities": {
            "tools": {}
        },
        "serverInfo": {
            "name": SERVER_NAME,
            "version": SERVER_VERSION
        }
    }))
}

async fn handle_list_tools(
    client: &UnrealClient,
    ctx: &ContextLoader,
) -> Result<Value, Value> {
    let status = client.check_connection().await;

    if !status.connected {
        log("INFO", "Unreal not connected", Some(&json!({"reason": status.reason})));
        return Ok(json!({
            "tools": [{
                "name": "unreal_status",
                "description": "Check if Unreal Editor is running with the plugin. Currently: NOT CONNECTED.",
                "inputSchema": { "type": "object", "properties": {} }
            }]
        }));
    }

    let unreal_tools = client.fetch_tools().await;
    let mut mcp_tools: Vec<Value> = Vec::with_capacity(unreal_tools.len() + 2);

    // Status tool
    mcp_tools.push(json!({
        "name": "unreal_status",
        "description": format!(
            "Check Unreal Editor connection status. Currently: CONNECTED to {} ({})",
            status.project_name.as_deref().unwrap_or("Unknown Project"),
            status.engine_version.as_deref().unwrap_or("Unknown")
        ),
        "inputSchema": { "type": "object", "properties": {} },
        "annotations": {
            "readOnlyHint": true,
            "destructiveHint": false,
            "idempotentHint": true,
            "openWorldHint": false
        }
    }));

    // Unreal tools
    for tool in &unreal_tools {
        let schema = convert_to_mcp_schema(&tool.parameters);
        let annotations = tool
            .annotations
            .as_ref()
            .map(|a| {
                json!({
                    "readOnlyHint": a.read_only_hint,
                    "destructiveHint": a.destructive_hint,
                    "idempotentHint": a.idempotent_hint,
                    "openWorldHint": a.open_world_hint
                })
            })
            .unwrap_or(json!({
                "readOnlyHint": false,
                "destructiveHint": true,
                "idempotentHint": false,
                "openWorldHint": false
            }));

        mcp_tools.push(json!({
            "name": format!("unreal_{}", tool.name),
            "description": format!("[Unreal Editor] {}", tool.description),
            "inputSchema": schema,
            "annotations": annotations
        }));
    }

    // Context tool
    let categories = ctx.list_categories();
    mcp_tools.push(json!({
        "name": "unreal_get_ue_context",
        "description": format!(
            "Get Unreal Engine 5.7 API context/documentation. Categories: {}. Can also search by query keywords.",
            categories.join(", ")
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "category": {
                    "type": "string",
                    "description": format!("Specific category to load: {}", categories.join(", "))
                },
                "query": {
                    "type": "string",
                    "description": "Search query to find relevant context"
                }
            }
        },
        "annotations": {
            "readOnlyHint": true,
            "destructiveHint": false,
            "idempotentHint": true,
            "openWorldHint": false
        }
    }));

    log("INFO", "Tools listed", Some(&json!({"count": mcp_tools.len()})));
    Ok(json!({ "tools": mcp_tools }))
}

async fn handle_call_tool(
    client: &UnrealClient,
    ctx: &ContextLoader,
    config: &Config,
    request: &Value,
) -> Result<Value, Value> {
    let params = &request["params"];
    let name = params["name"].as_str().unwrap_or("");
    let args = params.get("arguments").cloned().unwrap_or(json!({}));

    // Handle UE context request
    if name == "unreal_get_ue_context" {
        return handle_context_request(ctx, &args);
    }

    // Handle status check
    if name == "unreal_status" {
        return handle_status_request(client, ctx).await;
    }

    // Strip "unreal_" prefix
    let tool_name = match name.strip_prefix("unreal_") {
        Some(t) => t,
        None => {
            return Ok(tool_error(&format!("Unknown tool: {}", name)));
        }
    };

    // Determine if this tool should use async
    let is_task_tool = tool_name.starts_with("task_");
    let use_async =
        !is_task_tool && (config.async_enabled || ASYNC_TOOLS.contains(&tool_name));

    let result = if use_async {
        client.execute_tool_async(tool_name, &args).await
    } else {
        client.execute_tool(tool_name, &args).await
    };

    let mut response_text = if result.success {
        let mut text = result.message.clone();
        if let Some(data) = &result.data {
            text.push_str("\n\n");
            text.push_str(&data.to_string());
        }
        text
    } else {
        format!("Error: {}", result.message)
    };

    // Context injection
    if config.inject_context && result.success {
        if let Some(context) = ctx.get_context_for_tool(tool_name) {
            response_text.push_str("\n\n---\n\n## Relevant UE 5.7 API Context\n\n");
            response_text.push_str(&context);
        }
    }

    Ok(json!({
        "content": [{ "type": "text", "text": response_text }],
        "isError": !result.success
    }))
}

fn handle_context_request(ctx: &ContextLoader, args: &Value) -> Result<Value, Value> {
    let category = args["category"].as_str();
    let query = args["query"].as_str();

    if let Some(cat) = category {
        if let Some(content) = ctx.load_category(cat) {
            return Ok(json!({
                "content": [{ "type": "text", "text": format!("# UE 5.7 Context: {}\n\n{}", cat, content) }]
            }));
        }
        return Ok(tool_error(&format!(
            "Unknown category: {}. Available: {}",
            cat,
            ctx.list_categories().join(", ")
        )));
    }

    if let Some(q) = query {
        if let Some((categories, content)) = ctx.get_context_for_query(q) {
            return Ok(json!({
                "content": [{ "type": "text", "text": format!("# UE 5.7 Context: {}\n\n{}", categories.join(", "), content) }]
            }));
        }
        return Ok(json!({
            "content": [{ "type": "text", "text": format!("No context found for query: \"{}\". Try categories: {}", q, ctx.list_categories().join(", ")) }]
        }));
    }

    // List categories
    let list: Vec<String> = ctx
        .list_categories()
        .iter()
        .map(|cat| {
            let kw = ctx.get_category_keywords(cat);
            format!("- **{}**: Keywords: {}...", cat, kw.join(", "))
        })
        .collect();

    Ok(json!({
        "content": [{ "type": "text", "text": format!(
            "# Available UE 5.7 Context Categories\n\n{}\n\nUse `category` param for specific context or `query` to search by keywords.",
            list.join("\n")
        )}]
    }))
}

async fn handle_status_request(
    client: &UnrealClient,
    ctx: &ContextLoader,
) -> Result<Value, Value> {
    let status = client.check_connection().await;

    if !status.connected {
        return Ok(json!({
            "content": [{ "type": "text", "text": serde_json::to_string_pretty(&json!({
                "connected": false,
                "reason": status.reason,
                "message": "Unreal Editor is not running or the plugin is not enabled."
            })).unwrap() }],
            "isError": true
        }));
    }

    let tools = client.fetch_tools().await;
    let mut categories: std::collections::HashMap<&str, usize> = std::collections::HashMap::new();
    for tool in &tools {
        let cat = if tool.name.starts_with("blueprint_") {
            "blueprint"
        } else if tool.name.starts_with("anim_blueprint") {
            "animation"
        } else if tool.name.starts_with("asset_") {
            "asset"
        } else if tool.name.starts_with("task_") {
            "task_queue"
        } else if tool.name.contains("actor")
            || tool.name.contains("spawn")
            || tool.name.contains("move")
            || tool.name.contains("level")
        {
            "actor"
        } else {
            "utility"
        };
        *categories.entry(cat).or_insert(0) += 1;
    }

    let context_categories = ctx.list_categories();

    Ok(json!({
        "content": [{ "type": "text", "text": serde_json::to_string_pretty(&json!({
            "connected": true,
            "project": status.project_name,
            "engine": status.engine_version,
            "context_system": format!("OK ({} categories: {})", context_categories.len(), context_categories.join(", ")),
            "tool_summary": categories,
            "total_tools": tools.len(),
            "message": "Unreal Editor connected. All tools operational."
        })).unwrap() }]
    }))
}

fn convert_to_mcp_schema(params: &[unreal::UnrealParam]) -> Value {
    let mut properties = serde_json::Map::new();
    let mut required: Vec<Value> = Vec::new();

    for param in params {
        let mut prop = serde_json::Map::new();
        prop.insert(
            "type".into(),
            json!(match param.param_type.as_str() {
                "number" => "number",
                "boolean" => "boolean",
                "array" => "array",
                "object" => "object",
                _ => "string",
            }),
        );
        prop.insert("description".into(), json!(param.description));
        if let Some(ref default) = param.default {
            prop.insert("default".into(), json!(default));
        }
        properties.insert(param.name.clone(), Value::Object(prop));
        if param.required {
            required.push(json!(param.name));
        }
    }

    let mut schema = json!({ "type": "object", "properties": properties });
    if !required.is_empty() {
        schema["required"] = json!(required);
    }
    schema
}

fn tool_error(msg: &str) -> Value {
    json!({
        "content": [{ "type": "text", "text": msg }],
        "isError": true
    })
}
