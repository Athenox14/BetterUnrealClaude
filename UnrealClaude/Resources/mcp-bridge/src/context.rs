use regex::Regex;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Mutex;

struct CategoryConfig {
    files: Vec<&'static str>,
    tool_patterns: Vec<Regex>,
    keywords: Vec<&'static str>,
}

pub struct ContextLoader {
    contexts_dir: PathBuf,
    categories: Vec<(&'static str, CategoryConfig)>,
    cache: Mutex<HashMap<String, String>>,
}

impl ContextLoader {
    pub fn new(contexts_dir: PathBuf) -> Self {
        let categories = vec![
            (
                "animation",
                CategoryConfig {
                    files: vec!["animation.md"],
                    tool_patterns: vec![
                        Regex::new(r"^anim").unwrap(),
                        Regex::new(r"animation").unwrap(),
                        Regex::new(r"state_machine").unwrap(),
                    ],
                    keywords: vec![
                        "animation", "anim", "state machine", "blend", "transition",
                        "animinstance", "montage", "sequence", "blendspace",
                    ],
                },
            ),
            (
                "blueprint",
                CategoryConfig {
                    files: vec!["blueprint.md"],
                    tool_patterns: vec![
                        Regex::new(r"^blueprint").unwrap(),
                        Regex::new(r"^bp_").unwrap(),
                    ],
                    keywords: vec![
                        "blueprint", "graph", "node", "pin", "uk2node", "variable",
                        "function", "event graph",
                    ],
                },
            ),
            (
                "slate",
                CategoryConfig {
                    files: vec!["slate.md"],
                    tool_patterns: vec![
                        Regex::new(r"widget").unwrap(),
                        Regex::new(r"editor.*ui").unwrap(),
                        Regex::new(r"slate").unwrap(),
                    ],
                    keywords: vec![
                        "slate", "widget", "snew", "sassign", "ui", "editor window",
                        "tab", "panel", "sverticalbox", "shorizontalbox",
                    ],
                },
            ),
            (
                "actor",
                CategoryConfig {
                    files: vec!["actor.md"],
                    tool_patterns: vec![
                        Regex::new(r"spawn").unwrap(),
                        Regex::new(r"actor").unwrap(),
                        Regex::new(r"move").unwrap(),
                        Regex::new(r"delete").unwrap(),
                        Regex::new(r"level").unwrap(),
                        Regex::new(r"open_level").unwrap(),
                    ],
                    keywords: vec![
                        "actor", "spawn", "component", "transform", "location",
                        "rotation", "attach", "destroy", "iterate", "level", "map",
                        "open level", "new level", "load map", "switch level",
                        "template map", "save level", "save map", "save as",
                    ],
                },
            ),
            (
                "assets",
                CategoryConfig {
                    files: vec!["assets.md"],
                    tool_patterns: vec![
                        Regex::new(r"asset").unwrap(),
                        Regex::new(r"load").unwrap(),
                        Regex::new(r"reference").unwrap(),
                    ],
                    keywords: vec![
                        "asset", "load", "soft pointer", "tsoftobjectptr", "async",
                        "stream", "reference", "registry", "tobjectptr",
                    ],
                },
            ),
            (
                "replication",
                CategoryConfig {
                    files: vec!["replication.md"],
                    tool_patterns: vec![
                        Regex::new(r"replicate").unwrap(),
                        Regex::new(r"network").unwrap(),
                        Regex::new(r"rpc").unwrap(),
                    ],
                    keywords: vec![
                        "replicate", "replication", "network", "rpc", "server",
                        "client", "multicast", "onrep", "doreplifetime", "authority",
                    ],
                },
            ),
            (
                "enhanced_input",
                CategoryConfig {
                    files: vec!["enhanced_input.md"],
                    tool_patterns: vec![
                        Regex::new(r"enhanced_input").unwrap(),
                        Regex::new(r"input_action").unwrap(),
                        Regex::new(r"mapping_context").unwrap(),
                    ],
                    keywords: vec![
                        "enhanced input", "input action", "mapping context",
                        "inputaction", "inputmappingcontext", "trigger", "modifier",
                        "key binding", "keybinding", "gamepad", "controller",
                        "keyboard mapping", "input mapping", "dead zone", "deadzone",
                        "axis", "chord",
                    ],
                },
            ),
            (
                "character",
                CategoryConfig {
                    files: vec!["character.md"],
                    tool_patterns: vec![
                        Regex::new(r"^character").unwrap(),
                        Regex::new(r"character_data").unwrap(),
                        Regex::new(r"movement_param").unwrap(),
                    ],
                    keywords: vec![
                        "character", "acharacter", "movement", "charactermovement",
                        "walk speed", "jump velocity", "air control", "gravity scale",
                        "capsule", "character data", "stats table", "character config",
                        "health", "stamina", "damage multiplier", "defense",
                        "player character", "npc",
                    ],
                },
            ),
            (
                "material",
                CategoryConfig {
                    files: vec!["material.md"],
                    tool_patterns: vec![
                        Regex::new(r"^material").unwrap(),
                        Regex::new(r"skeletal_mesh_material").unwrap(),
                        Regex::new(r"actor_material").unwrap(),
                    ],
                    keywords: vec![
                        "material", "material instance", "materialinstance", "mic",
                        "mid", "scalar parameter", "vector parameter",
                        "texture parameter", "parent material", "material slot",
                        "roughness", "metallic", "base color", "emissive", "opacity",
                    ],
                },
            ),
            (
                "parallel_workflows",
                CategoryConfig {
                    files: vec!["parallel_workflows.md"],
                    tool_patterns: vec![],
                    keywords: vec![
                        "parallel", "subagent", "swarm", "agent team", "level setup",
                        "build a level", "set up a level", "create a level",
                        "build a scene", "set up scene", "scene setup",
                        "character pipeline", "set up character",
                        "create character pipeline", "multiple agents", "decompose",
                        "parallelize", "concurrent", "batch operations", "bulk create",
                    ],
                },
            ),
        ];

        Self {
            contexts_dir,
            categories,
            cache: Mutex::new(HashMap::new()),
        }
    }

    fn load_file(&self, filename: &str) -> Option<String> {
        {
            let cache = self.cache.lock().unwrap();
            if let Some(content) = cache.get(filename) {
                return Some(content.clone());
            }
        }

        let path = self.contexts_dir.join(filename);
        match fs::read_to_string(&path) {
            Ok(content) => {
                let mut cache = self.cache.lock().unwrap();
                cache.insert(filename.to_string(), content.clone());
                Some(content)
            }
            Err(_) => {
                eprintln!("[ContextLoader] Context file not found: {}", path.display());
                None
            }
        }
    }

    pub fn list_categories(&self) -> Vec<&str> {
        self.categories.iter().map(|(name, _)| *name).collect()
    }

    pub fn get_category_keywords(&self, category: &str) -> Vec<&str> {
        self.categories
            .iter()
            .find(|(name, _)| *name == category)
            .map(|(_, config)| config.keywords.iter().take(5).copied().collect())
            .unwrap_or_default()
    }

    pub fn load_category(&self, category: &str) -> Option<String> {
        let config = self
            .categories
            .iter()
            .find(|(name, _)| *name == category)?;

        let contents: Vec<String> = config
            .1
            .files
            .iter()
            .filter_map(|f| self.load_file(f))
            .collect();

        if contents.is_empty() {
            None
        } else {
            Some(contents.join("\n\n---\n\n"))
        }
    }

    /// Get context for a tool call (automatic injection by tool name)
    pub fn get_context_for_tool(&self, tool_name: &str) -> Option<String> {
        let lower = tool_name.to_lowercase();

        for (name, config) in &self.categories {
            for pattern in &config.tool_patterns {
                if pattern.is_match(&lower) {
                    return self.load_category(name);
                }
            }
        }
        None
    }

    /// Get context by query keywords
    pub fn get_context_for_query(&self, query: &str) -> Option<(Vec<String>, String)> {
        let lower = query.to_lowercase();
        let mut matched: Vec<String> = Vec::new();

        for (name, config) in &self.categories {
            for keyword in &config.keywords {
                if lower.contains(&keyword.to_lowercase()) {
                    if !matched.contains(&name.to_string()) {
                        matched.push(name.to_string());
                    }
                    break;
                }
            }
        }

        if matched.is_empty() {
            return None;
        }

        let contents: Vec<String> = matched
            .iter()
            .filter_map(|cat| self.load_category(cat))
            .collect();

        Some((matched, contents.join("\n\n---\n\n")))
    }

    /// Resolve the contexts directory from the executable location
    pub fn find_contexts_dir() -> PathBuf {
        // Try relative to executable
        if let Ok(exe) = std::env::current_exe() {
            let dir = exe.parent().unwrap_or(Path::new("."));
            let candidate = dir.join("contexts");
            if candidate.is_dir() {
                return candidate;
            }
            // When running via cargo run, exe is in target/debug/
            let candidate = dir.join("../../contexts");
            if candidate.is_dir() {
                return candidate;
            }
        }

        // Fallback: current directory
        PathBuf::from("contexts")
    }
}
