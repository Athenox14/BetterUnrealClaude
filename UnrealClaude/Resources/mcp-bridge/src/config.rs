use std::env;
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone)]
pub struct Config {
    pub unreal_mcp_url: String,
    pub request_timeout_ms: u64,
    pub inject_context: bool,
    pub async_enabled: bool,
    pub async_timeout_ms: u64,
    pub poll_interval_ms: u64,
}

impl Config {
    pub fn load() -> Self {
        let port = get_config_port();
        let default_url = format!("http://localhost:{}", port);

        Self {
            unreal_mcp_url: env::var("UNREAL_MCP_URL").unwrap_or(default_url),
            request_timeout_ms: env::var("MCP_REQUEST_TIMEOUT_MS")
                .ok()
                .and_then(|v| v.parse().ok())
                .unwrap_or(30_000),
            inject_context: env::var("INJECT_CONTEXT")
                .map(|v| v == "true")
                .unwrap_or(false),
            async_enabled: env::var("MCP_ASYNC_ENABLED")
                .map(|v| v == "true")
                .unwrap_or(false),
            async_timeout_ms: env::var("MCP_ASYNC_TIMEOUT_MS")
                .ok()
                .and_then(|v| v.parse().ok())
                .unwrap_or(300_000),
            poll_interval_ms: env::var("MCP_POLL_INTERVAL_MS")
                .ok()
                .and_then(|v| v.parse().ok())
                .unwrap_or(2_000),
        }
    }
}

/// Read port from plugin config.json (shared with C++ side)
fn get_config_port() -> u16 {
    // config.json is at plugin root: ../../config.json relative to Resources/mcp-bridge/
    let exe_dir = env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|p| p.to_path_buf()));

    let candidates: Vec<PathBuf> = vec![
        // When running from the mcp-bridge directory
        PathBuf::from("../../config.json"),
        // When running from the binary location
        exe_dir
            .as_ref()
            .map(|d| d.join("../../config.json"))
            .unwrap_or_default(),
        // Fallback: relative to working directory
        PathBuf::from("config.json"),
    ];

    for path in candidates {
        if let Ok(content) = fs::read_to_string(&path) {
            if let Ok(json) = serde_json::from_str::<serde_json::Value>(&content) {
                if let Some(port) = json.get("server_port").and_then(|v| v.as_u64()) {
                    if port > 0 && port <= 65535 {
                        return port as u16;
                    }
                }
            }
        }
    }

    3000
}
