use openssl::hash::MessageDigest;
use openssl::sign::Verifier;
use openssl::symm::{Cipher, Crypter, Mode};
use serde::Deserialize;
use std::collections::HashMap;
use std::time::SystemTime;
use tauri_plugin_clipboard_manager::ClipboardExt;

use openssl::encrypt::Decrypter;
use openssl::pkey::PKey;
use openssl::rsa::{Padding, Rsa};
use serde_json::json;
use tauri::{AppHandle, Emitter};

use crate::commands::CBState;
use crate::constants::{RESPONSE_VALID_WINTHIN_SEC, RES_FAIL, RES_OK};
use crate::data::ReceivedCTSFrame;
use crate::restore::{ctsync_get_key, ctsync_set_key};

use std::sync::Arc;
use tokio::sync::Mutex;

pub async fn data_processor(
    response_queue: &Arc<Mutex<Option<HashMap<u32, (u64, fn(AppHandle, ReceivedCTSFrame))>>>>,
    app: AppHandle,
    data: Vec<u8>,
    content_hash: Arc<Mutex<Option<Vec<u8>>>>,
) {
    let frame: ReceivedCTSFrame;
    match ReceivedCTSFrame::new(data) {
        Ok(f) => {
            frame = f;

            // response originated from client will have odd index
            if frame.inner().res_index % 2 == 1 {
                let mut guard = response_queue.lock().await;
                if let Some(ref mut res_q) = *guard {
                    let recv_index = &frame.inner().res_index.clone();
                    if let Some(item) = res_q.get(recv_index) {
                        let current_time = SystemTime::now()
                            .duration_since(std::time::UNIX_EPOCH)
                            .unwrap()
                            .as_secs();
                        let timestamp = item.0;

                        if (timestamp + RESPONSE_VALID_WINTHIN_SEC as u64) < current_time {
                            return; // too old response
                        }

                        let callback_fn = item.1;
                        callback_fn(app, frame);
                        // free response index
                        res_q.remove(recv_index);
                    } else {
                        println!("no item found in the response HashMap that has same index as this response index");
                    }
                } else {
                    println!("failed to aquire lock for response_queue");
                }
            } else {
                // handles response that has been originated from server, i.e responses that are not result of an explcit request from the device
                server_action_handler(app, frame, content_hash).await;
            }
        }

        Err(_) => {
            println!("error desirializing received data");
        }
    }
}

// handles response that has been originated from server, i.e responses that are not result of an explcit request from the device
async fn server_action_handler(
    app: AppHandle,
    frame: ReceivedCTSFrame,
    content_hash: Arc<Mutex<Option<Vec<u8>>>>,
) {
    // TODO: could notify user about errors in notification or someting
    if let Some(action_b) = frame.inner().kv.get("action".as_bytes()) {
        let action = match String::from_utf8(action_b.to_vec()) {
            Err(_) => {
                return;
            }
            Ok(val) => val,
        };

        if action == "ONLINE_DEVICES" {
            svr_online_devices(app, frame);
            // 'GROUP_JOIN_REQUEST' is for when someone has request a join request and your (this device) need to accept/or not
        } else if action == "GROUP_JOIN_REQUEST" {
            svr_group_join_req(app, frame);
            // 'GROUP_JOIN_RESPONSE' is for when other device accepts or reject your (this device) request to join a group
        } else if action == "GROUP_JOIN_RESPONSE" {
            svr_group_join_res(app, frame);
        } else if action == "RECV_CLIPBOARD" {
            svr_recv_clipboad(app, frame, content_hash).await;
        } else if action == "KICKED" {
            svr_kicked(app, frame);
        }
    }
}

fn rsa_decrypt_data(data: Vec<u8>, private_key: &str) -> Option<Vec<u8>> {
    let rsa = Rsa::private_key_from_pem(private_key.as_bytes()).ok()?;
    let sign_key = PKey::from_rsa(rsa).ok()?;
    let mut decrypter = Decrypter::new(&sign_key).ok()?;
    decrypter.set_rsa_padding(Padding::PKCS1).ok()?;
    let buffer_len = decrypter.decrypt_len(&data).unwrap();
    let mut decrypted = vec![0; buffer_len];
    let decrypted_len = decrypter.decrypt(&data, &mut decrypted).ok()?;
    decrypted.truncate(decrypted_len);
    return Some(decrypted);
}

fn aes_decrypt(ciphertext: &[u8], key: &[u8], iv: &[u8]) -> Result<Vec<u8>, &'static str> {
    let cipher = Cipher::aes_256_cbc();
    let mut decrypter = Crypter::new(cipher, Mode::Decrypt, key, Some(iv))
        .map_err(|_| "Decryption failed, failed to create decryptor")?;

    let mut decrypted_text = vec![0; ciphertext.len() + cipher.block_size()];
    let mut count = decrypter
        .update(ciphertext, &mut decrypted_text)
        .map_err(|_| "Decryption failed, faild to update the decryptor")?;
    count += decrypter
        .finalize(&mut decrypted_text[count..])
        .map_err(|_| "Decryption failed, failed to finalize the decryptor")?;
    decrypted_text.truncate(count);

    Ok(decrypted_text)
}

#[derive(Debug, Deserialize)]
struct DeviceInfo {
    device_id: String,
    sign_public_key: String,
}

// the key used to sign data
fn get_sign_public_key(app: AppHandle, device_id: &str) -> Option<String> {
    let devices_str = ctsync_get_key(app, "devices").ok()?;
    let devices: Vec<DeviceInfo> = serde_json::from_str(&devices_str).ok()?;

    devices
        .into_iter()
        .find(|device| device.device_id == device_id)
        .map(|device| device.sign_public_key)
}

pub async fn update_clipboard(
    app: AppHandle,
    frame: ReceivedCTSFrame,
    content_hash: Arc<Mutex<Option<Vec<u8>>>>,
) -> Option<()> {
    // check if user want to receive
    match ctsync_get_key(app.clone(), "cb_state") {
        Err(_) => {}
        Ok(cb_state_str) => {
            let cb_state = match serde_json::from_str::<CBState>(&cb_state_str) {
                Err(_) => return None,
                Ok(val) => val,
            };
            if !cb_state.recv_cb {
                // user don't want to receive
                return None;
            }
        }
    }

    let encd_data = frame.get_key("encd_data".as_bytes()).ok()?;
    let encd_data_signature = frame.get_key("encd_data_signature".as_bytes()).ok()?;
    let encd_key = frame.get_key("encd_key".as_bytes()).ok()?;
    let encd_iv = frame.get_key("encd_iv".as_bytes()).ok()?;
    let device_id = frame.get_key("device_id".as_bytes()).ok()?;
    let private_key = ctsync_get_key(app.clone(), "pem_private_key").ok()?;

    let device_id_str = String::from_utf8(device_id.clone()).ok()?;

    let sign_public_key = match get_sign_public_key(app.clone(), &device_id_str) {
        Some(key) => key,
        None => return None,
    };

    let rsa = Rsa::public_key_from_pem(sign_public_key.as_bytes()).ok()?;
    let sign_key = PKey::from_rsa(rsa).ok()?;
    let mut verifier = Verifier::new(MessageDigest::sha256(), &sign_key).ok()?;
    verifier.update(&encd_data).ok()?;

    if let Ok(flag) = verifier.verify(&encd_data_signature) {
        if flag {
            let iv = rsa_decrypt_data(encd_iv, &private_key)?;
            let key = rsa_decrypt_data(encd_key, &private_key)?;

            // decode main content
            match aes_decrypt(&encd_data, &key, &iv) {
                Ok(decrypted) => {
                    let mut guard = content_hash.lock().await;
                    let text_data = String::from_utf8_lossy(&decrypted);
                    let hash =
                        openssl::hash::hash(openssl::hash::MessageDigest::sha256(), &decrypted)
                            .ok()?
                            .to_vec();
                    // store received content hash so that in state to keep track of received
                    *guard = Some(hash);
                    app.clipboard().write_text(text_data).ok()?;
                    return Some(());
                }

                Err(_) => {
                    return None;
                }
            }
        } else {
            return None;
        }
    } else {
        return None;
    }
}

async fn svr_recv_clipboad(
    app: AppHandle,
    frame: ReceivedCTSFrame,
    content_hash: Arc<Mutex<Option<Vec<u8>>>>,
) {
    match frame.inner().status_code {
        RES_OK => _ = update_clipboard(app.clone(), frame, content_hash).await,
        RES_FAIL => {}
        _ => {}
    }
}

fn svr_group_join_res(app: AppHandle, frame: ReceivedCTSFrame) {
    match frame.inner().status_code {
        RES_OK => {
            _ = app.emit("show-page", "home");
        }
        RES_FAIL => {
            _ = app.emit("group_join_req_rejected", "reject");
        }
        _ => {}
    }
}

fn svr_online_devices(app: AppHandle, frame: ReceivedCTSFrame) {
    if let Some(data_b) = frame.inner().kv.get("data".as_bytes()) {
        let data = match String::from_utf8(data_b.to_vec()) {
            Err(_) => {
                return;
            }
            Ok(val) => val,
        };
        // store every group devices information in local storage
        _ = ctsync_set_key(app.clone(), "devices", &data);
        _ = app.emit("online_devices", data);
    }
}

fn get_group_join_req_json_data(frame: ReceivedCTSFrame) -> Option<serde_json::Value> {
    let device_alias = frame.inner().kv.get("device_alias".as_bytes())?.to_vec();
    let device_id = frame.inner().kv.get("device_id".as_bytes())?.to_vec();
    let pub_key_hash = frame.inner().kv.get("pub_key_hash".as_bytes())?.to_vec();

    let device_alias = String::from_utf8(device_alias).ok()?;
    let device_id = String::from_utf8(device_id).ok()?;
    let pub_key_hash = String::from_utf8(pub_key_hash).ok()?;

    Some(json!({
        "device_alias": device_alias,
        "device_id": device_id,
        "pub_key_hash": pub_key_hash,
    }))
}

fn svr_group_join_req(app: AppHandle, frame: ReceivedCTSFrame) {
    if let Some(json_data) = get_group_join_req_json_data(frame) {
        _ = app.emit("group_join_request", json_data);
    }
}

fn svr_kicked(app: AppHandle, _frame: ReceivedCTSFrame) {
    _ = app.emit("show-page", "group_join");
    _ = app.emit("notification", "You have been removed from the group.");
}
