use std::collections::HashMap;
use std::sync::Arc;
use tauri;
use tokio::sync::Mutex;
pub mod commands;
pub mod connect;
pub mod constants;
pub mod data;
pub mod keys;
pub mod response;
pub mod restore;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub async fn run() {
    let state = connect::AppState {
        data_channel: Arc::new(Mutex::new(None)),
        task_handle: Arc::new(Mutex::new(None)),
        cb_monitor_handle: Arc::new(Mutex::new(None)),
        server_pub_key_hash: Arc::new(Mutex::new(None)),
        con_addr: Arc::new(Mutex::new(None)),
        content_hash: Arc::new(Mutex::new(None)),
        response_queue: Arc::new(Mutex::new(Some(HashMap::new()))),
        current_res_index: Arc::new(Mutex::new(Some(1))),
    };

    tauri::Builder::default()
        .plugin(tauri_plugin_store::Builder::new().build())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .manage(state)
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![
            connect::get_server_public_key_hash,
            connect::verify_server_access,
            connect::register_device,
            commands::create_new_group,
            commands::page_to_show,
            commands::generate_invite_key,
            commands::join_group,
            commands::accept_device,
            commands::stop_clipboard_monitor,
            commands::start_clipboard_monitor,
            commands::isconnected,
            commands::try_reconnect,
            commands::get_online_devices,
            commands::get_invite_key,
            commands::kick_device,
            commands::restore_state,
            commands::store_clipboard_recv_status,
            commands::delete_local_account,
            commands::group_check,
            commands::start_clipboard_receive,
            commands::stop_clipboard_receive,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
