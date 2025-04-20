use colored::Colorize;
use std::time::SystemTime;
use tauri::{AppHandle, Emitter};

use crate::{
    commands::CBState,
    connect::AppState,
    constants::{RES_ERROR, RES_FAIL, RES_OK},
    data::ReceivedCTSFrame,
    restore::{ctsync_get_key, ctsync_set_key},
};

pub fn hn_login_response(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            // after this the frontend also need to check if they are part of any group
            _ = app.emit("login_success", "success");
        }
        _ => {
            println!("{}", "Login failed".red());
            _ = app.emit("show-page", "login_failed");
        }
    }
}

pub async fn add_response_handler(
    state: tauri::State<'_, AppState>,
    callback: fn(AppHandle, ReceivedCTSFrame),
) {
    let mut guard = state.response_queue.lock().await;
    if let Some(ref mut response_queue) = *guard {
        // create new id -> create timestamp -> create response processor function
        let timestamp = SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();

        // get the current res_id increament it and use it
        let mut res_guard = state.current_res_index.lock().await;
        if let Some(ref mut res_id) = *res_guard {
            *res_id = res_id.wrapping_add(2);
            response_queue.insert(*res_id, (timestamp, callback));
        }
    }
}

pub fn hn_server_access(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            _ = app.emit("verify-access-status", 1);
        }
        RES_FAIL => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                _ = app.emit("error", error_msg);
            } else {
                _ = app.emit("verify-access-status", 0);
            }
        }
        RES_ERROR => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                _ = app.emit("error", error_msg);
            }
        }
        _ => {}
    }
}

pub fn hn_device_register(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            if let Some(cookie) = frame.inner().kv.get("login_key".as_bytes()) {
                if let Ok(cookie_str) = std::str::from_utf8(cookie) {
                    if let Err(_) = ctsync_set_key(app.clone(), "login_key", cookie_str) {
                        _ = app.emit("error", "failed to save login key");
                        return;
                    }
                }
            }

            if let Some(cookie) = frame.inner().kv.get("device_id".as_bytes()) {
                if let Ok(cookie_str) = std::str::from_utf8(cookie) {
                    if let Err(_) = ctsync_set_key(app.clone(), "device_id", cookie_str) {
                        _ = app.emit("error", "failed to device id");
                        return;
                    }
                }
            }

            _ = app.emit("registration-success", 0);
        }

        RES_FAIL => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(msg) = String::from_utf8(error_msg.clone()) {
                    _ = app.emit("error", msg);
                }
                return;
            }
            _ = app.emit("registraion-status", 0);
        }
        RES_ERROR => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(error_msg_str) = std::str::from_utf8(&error_msg) {
                    _ = app.emit("error", error_msg_str);
                    return;
                }
            }
            _ = app.emit(
                "error",
                "something went wrong registring device, please try again",
            );
        }
        _ => {
            println!("{}", "device_register response is UNKNOWN".red());
        }
    }
}

pub fn hn_new_group(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            _ = app.emit("show-page", "home");
            _ = ctsync_set_key(app, "group", "bingchilling");
        }

        _ => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(error_msg_str) = std::str::from_utf8(&error_msg) {
                    _ = app.emit("error", error_msg_str);
                    return;
                }
            }
        }
    }
}
pub fn hn_gen_group_invite_key(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            if let Some(key) = frame.inner().kv.get("invite_key".as_bytes()) {
                if let Ok(key_str) = std::str::from_utf8(&key) {
                    _ = app.emit("show_invite_key", key_str);
                } else {
                    println!("{}", "failed to convert key to string".red());
                }
            } else {
                println!("{}", "no key named invite_key".red());
            }
        }
        _ => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(error_msg_str) = std::str::from_utf8(&error_msg) {
                    _ = app.emit("error", error_msg_str);
                }
            }
        }
    }
}

pub fn hn_join_group(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            _ = app.emit("join_group_wait", "wait");
        }
        _ => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(error_msg_str) = std::str::from_utf8(&error_msg) {
                    _ = app.emit("notification", error_msg_str);
                }
            }
        }
    }
}

pub fn hn_group_check(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            _ = app.emit("show-page", "home");

            // send event to restore clipboard send and receive state
            match ctsync_get_key(app.clone(), "cb_state") {
                Err(_) => {
                    // if cb_state key not found in store then set both recv and send to true
                    let cb_state = CBState {
                        recv_cb: true,
                        send_cb: true,
                    };
                    _ = app.emit("cb-state", &cb_state);
                }
                Ok(val) => {
                    //  deserialize and  send
                    let cb_state = match serde_json::from_str::<CBState>(&val) {
                        Ok(val_state) => val_state,
                        Err(_) => {
                            let cb_state = CBState {
                                recv_cb: true,
                                send_cb: true,
                            };
                            _ = app.emit("cb-state", &cb_state);
                            return;
                        }
                    };
                    _ = app.emit("cb-state", &cb_state);
                }
            }
        }
        _ => {
            if let Some(error_msg) = frame.inner().kv.get("error".as_bytes()) {
                if let Ok(error_msg_str) = std::str::from_utf8(&error_msg) {
                    _ = app.emit("show-page", "group_join");
                    _ = app.emit("notification", error_msg_str);
                }
            }
        }
    }
}

pub fn hn_accept_device(_app: AppHandle, _frame: ReceivedCTSFrame) { //
}
pub fn hn_get_group_invite_key(app: AppHandle, frame: ReceivedCTSFrame) {
    //

    match frame.inner().status_code {
        RES_OK => {
            if let Some(invite_key) = frame.inner().kv.get("invite_key".as_bytes()) {
                if let Ok(invite_key_str) = std::str::from_utf8(&invite_key) {
                    _ = app.emit("show_invite_key", invite_key_str);
                }
            }
        }
        _ => {}
    }
}
pub fn hn_get_online_devices(_app: AppHandle, _frame: ReceivedCTSFrame) { //
}
pub fn hn_kick_device(_app: AppHandle, _frame: ReceivedCTSFrame) { //
}
pub fn hn_clipboard_sent(_app: AppHandle, _frame: ReceivedCTSFrame) { //
}
