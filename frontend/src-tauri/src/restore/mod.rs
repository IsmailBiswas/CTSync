use serde_json::json;
use serde_json::Value;
use tauri::AppHandle;
use tauri_plugin_store::StoreExt;

use crate::constants::CTSYNC_STORE_NAME;

pub fn ctsync_get_key(app: AppHandle, key: &str) -> Result<String, String> {
    let store = app.store(CTSYNC_STORE_NAME).map_err(|e| e.to_string())?;
    let value = store
        .get(key)
        .ok_or_else(|| "failed to retrieve stored value from disk".to_string())?;

    // Parse the JSON object and extract the "value" field
    let json_value: Value = serde_json::from_str(&value.to_string()).map_err(|e| e.to_string())?;
    let inner_value = json_value
        .get("value")
        .and_then(|v| v.as_str())
        .ok_or_else(|| "missing 'value' field or not a string".to_string())?;

    Ok(inner_value.to_string())
}

pub fn ctsync_set_key(app: AppHandle, key: &str, value: &str) -> Result<(), String> {
    let store = app.store(CTSYNC_STORE_NAME).map_err(|e| e.to_string())?;
    store.set(key, json!({ "value": value }));
    Ok(())
}

pub fn ctsync_clear_store(app: AppHandle) -> Result<(), String> {
    let store = app.store(CTSYNC_STORE_NAME).map_err(|e| e.to_string())?;
    store.clear();
    Ok(())
}
