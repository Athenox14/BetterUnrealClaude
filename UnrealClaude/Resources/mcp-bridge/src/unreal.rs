use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::time::Duration;

use crate::config::Config;

/// Unreal Engine HTTP client
#[derive(Clone)]
pub struct UnrealClient {
    client: Client,
    base_url: String,
    timeout: Duration,
    async_timeout: Duration,
    poll_interval: Duration,
    #[allow(dead_code)]
    pub async_enabled: bool,
}

#[derive(Debug, Deserialize)]
pub struct UnrealTool {
    pub name: String,
    pub description: String,
    #[serde(default)]
    pub parameters: Vec<UnrealParam>,
    pub annotations: Option<UnrealAnnotations>,
}

#[derive(Debug, Deserialize)]
pub struct UnrealParam {
    pub name: String,
    #[serde(rename = "type")]
    pub param_type: String,
    pub description: String,
    #[serde(default)]
    pub required: bool,
    pub default: Option<String>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct UnrealAnnotations {
    #[serde(default)]
    pub read_only_hint: bool,
    #[serde(default = "default_true")]
    pub destructive_hint: bool,
    #[serde(default)]
    pub idempotent_hint: bool,
    #[serde(default)]
    pub open_world_hint: bool,
}

fn default_true() -> bool {
    true
}

impl Default for UnrealAnnotations {
    fn default() -> Self {
        Self {
            read_only_hint: false,
            destructive_hint: true,
            idempotent_hint: false,
            open_world_hint: false,
        }
    }
}

#[derive(Debug, Deserialize)]
pub struct ConnectionStatus {
    pub connected: bool,
    #[serde(default)]
    pub reason: Option<String>,
    #[serde(rename = "projectName")]
    pub project_name: Option<String>,
    #[serde(rename = "engineVersion")]
    pub engine_version: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct ToolResult {
    pub success: bool,
    pub message: String,
    pub data: Option<Value>,
}

impl UnrealClient {
    pub fn new(config: &Config) -> Self {
        let client = Client::builder()
            .timeout(Duration::from_millis(config.request_timeout_ms))
            .build()
            .expect("Failed to build HTTP client");

        Self {
            client,
            base_url: config.unreal_mcp_url.clone(),
            timeout: Duration::from_millis(config.request_timeout_ms),
            async_timeout: Duration::from_millis(config.async_timeout_ms),
            poll_interval: Duration::from_millis(config.poll_interval_ms),
            async_enabled: config.async_enabled,
        }
    }

    pub async fn check_connection(&self) -> ConnectionStatus {
        match self
            .client
            .get(format!("{}/mcp/status", self.base_url))
            .timeout(self.timeout)
            .send()
            .await
        {
            Ok(resp) if resp.status().is_success() => {
                let mut status: ConnectionStatus = resp.json().await.unwrap_or(ConnectionStatus {
                    connected: false,
                    reason: Some("Failed to parse response".into()),
                    project_name: None,
                    engine_version: None,
                });
                status.connected = true;
                status
            }
            Ok(resp) => ConnectionStatus {
                connected: false,
                reason: Some(format!("HTTP {}", resp.status())),
                project_name: None,
                engine_version: None,
            },
            Err(e) => ConnectionStatus {
                connected: false,
                reason: Some(if e.is_timeout() {
                    "timeout".into()
                } else {
                    e.to_string()
                }),
                project_name: None,
                engine_version: None,
            },
        }
    }

    pub async fn fetch_tools(&self) -> Vec<UnrealTool> {
        let resp = self
            .client
            .get(format!("{}/mcp/tools", self.base_url))
            .timeout(self.timeout)
            .send()
            .await;

        match resp {
            Ok(r) if r.status().is_success() => {
                #[derive(Deserialize)]
                struct ToolsResponse {
                    #[serde(default)]
                    tools: Vec<UnrealTool>,
                }
                r.json::<ToolsResponse>()
                    .await
                    .map(|t| t.tools)
                    .unwrap_or_default()
            }
            _ => Vec::new(),
        }
    }

    pub async fn execute_tool(&self, tool_name: &str, args: &Value) -> ToolResult {
        let url = format!("{}/mcp/tool/{}", self.base_url, tool_name);
        match self
            .client
            .post(&url)
            .json(args)
            .timeout(self.timeout)
            .send()
            .await
        {
            Ok(resp) => resp.json::<ToolResult>().await.unwrap_or(ToolResult {
                success: false,
                message: "Failed to parse tool response".into(),
                data: None,
            }),
            Err(e) => ToolResult {
                success: false,
                message: format!(
                    "Failed to execute tool: {}",
                    if e.is_timeout() {
                        format!("Request timeout after {}ms", self.timeout.as_millis())
                    } else {
                        e.to_string()
                    }
                ),
                data: None,
            },
        }
    }

    /// Execute a tool via async task queue (submit → poll → result).
    /// Falls back to sync if task_submit fails.
    pub async fn execute_tool_async(&self, tool_name: &str, args: &Value) -> ToolResult {
        // Step 1: Submit task
        let submit_body = serde_json::json!({
            "tool_name": tool_name,
            "params": args,
            "timeout_ms": self.async_timeout.as_millis() as u64,
        });

        let task_id = match self
            .client
            .post(format!("{}/mcp/tool/task_submit", self.base_url))
            .json(&submit_body)
            .timeout(self.timeout)
            .send()
            .await
        {
            Ok(resp) => {
                let data: Value = resp.json().await.unwrap_or_default();
                match data["data"]["task_id"].as_str() {
                    Some(id) if data["success"].as_bool() == Some(true) => id.to_string(),
                    _ => return self.execute_tool(tool_name, args).await,
                }
            }
            Err(_) => return self.execute_tool(tool_name, args).await,
        };

        // Step 2: Poll for completion
        let deadline = tokio::time::Instant::now() + self.async_timeout;

        while tokio::time::Instant::now() < deadline {
            tokio::time::sleep(self.poll_interval).await;

            let status_body = serde_json::json!({ "task_id": &task_id });
            let status_resp = self
                .client
                .post(format!("{}/mcp/tool/task_status", self.base_url))
                .json(&status_body)
                .timeout(self.timeout)
                .send()
                .await;

            if let Ok(resp) = status_resp {
                let data: Value = resp.json().await.unwrap_or_default();
                let status = data["data"]["status"]
                    .as_str()
                    .or(data["status"].as_str())
                    .unwrap_or("");

                if matches!(status, "completed" | "failed" | "cancelled") {
                    // Step 3: Retrieve result
                    let result_body = serde_json::json!({ "task_id": &task_id });
                    return match self
                        .client
                        .post(format!("{}/mcp/tool/task_result", self.base_url))
                        .json(&result_body)
                        .timeout(self.timeout)
                        .send()
                        .await
                    {
                        Ok(r) => r.json::<ToolResult>().await.unwrap_or(ToolResult {
                            success: false,
                            message: format!(
                                "Task {} but failed to retrieve result",
                                status
                            ),
                            data: None,
                        }),
                        Err(e) => ToolResult {
                            success: false,
                            message: format!(
                                "Task {} but failed to retrieve result: {}",
                                status, e
                            ),
                            data: None,
                        },
                    };
                }
            }
        }

        ToolResult {
            success: false,
            message: format!(
                "Task timed out after {}ms (task_id: {})",
                self.async_timeout.as_millis(),
                task_id
            ),
            data: None,
        }
    }
}
