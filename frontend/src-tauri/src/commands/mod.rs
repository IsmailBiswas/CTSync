use crate::connect::terminate_con;
use crate::data::{FrameAppState, OwnedCTSFrame};
use crate::response::{
    hn_accept_device, hn_clipboard_sent, hn_gen_group_invite_key, hn_get_group_invite_key,
    hn_get_online_devices, hn_group_check, hn_join_group, hn_kick_device, hn_login_response,
    hn_new_group,
};
use crate::restore::{ctsync_clear_store, ctsync_get_key, ctsync_set_key};
use core::str;
use openssl::base64;
use openssl::encrypt::Encrypter;
use openssl::hash::{hash, MessageDigest};
use openssl::pkey::PKey;
use openssl::rand::rand_bytes;
use openssl::rsa::Padding;
use openssl::rsa::Rsa;
use openssl::sign::Signer;
use openssl::symm::{Cipher, Crypter, Mode};
use rand::distributions::Alphanumeric;
use rand::Rng;
use serde::{Deserialize, Serialize};
use std::{fs, io};
use tauri::AppHandle;
use tauri::Manager;
use tauri_plugin_clipboard_manager::ClipboardExt;
use tokio::sync::oneshot;
use tokio::time::{sleep, Duration};

use crate::{
    connect::{setup_connection, AppState, ApplicationData, Cookie},
    constants::{APPLICATION_STATE, COOKIE_FILE_LOCATION, INVITE_KEY_SIZE},
};
use tauri::Emitter;

#[tauri::command]
pub async fn create_new_group(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);
    frame.add_kv("action".as_bytes(), "CREATE_GROUP".as_bytes());
    match frame.send(hn_new_group).await {
        Ok(_) => {
            return Ok(());
        }
        Err(_) => {
            return Err("failed make group creation request".to_string());
        }
    }
}

// remove
pub fn read_application_state_from_disk(app: AppHandle) -> io::Result<ApplicationData> {
    let app_dir = app
        .path()
        .app_local_data_dir()
        .map_err(|e| io::Error::new(io::ErrorKind::NotFound, e))?;

    let file_loc = app_dir.join(APPLICATION_STATE);
    let contents = fs::read_to_string(file_loc)?;
    let server_info: ApplicationData = serde_json::from_str(&contents)
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;

    Ok(server_info)
}
pub async fn read_cookie_from_disk(app: AppHandle) -> io::Result<Cookie> {
    let app_dir = app.path().app_local_data_dir().unwrap();

    let file_loc = app_dir.join(COOKIE_FILE_LOCATION);
    let contents = fs::read_to_string(file_loc)?;
    let cookie: Cookie = serde_json::from_str(&contents)
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;
    Ok(cookie)
}

pub async fn login(
    state: tauri::State<'_, AppState>,
    login_key: &str,
    device_id: &str,
) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);
    frame.add_kv("action".as_bytes(), "LOGIN".as_bytes());
    frame.add_kv("device_id".as_bytes(), device_id.as_bytes());
    frame.add_kv("login_key".as_bytes(), login_key.as_bytes());
    frame
        .send(hn_login_response)
        .await
        .map_err(|_| "failed to make login request")?;
    Ok(())
}

#[tauri::command]
pub async fn try_reconnect(
    state: tauri::State<'_, AppState>,
    window: tauri::Window,
    app: AppHandle,
) -> Result<(), String> {
    //check if all four credential are present

    // return if server_addr or server_port is not present.
    let server_addr = ctsync_get_key(app.clone(), "server_address")?;
    let server_port = ctsync_get_key(app.clone(), "server_port")?;
    let server_port: u16 = server_port
        .parse()
        .map_err(|_| "failed to convert port number to number type.".to_string())?;

    // if login_key is not present then just establish TCP connection
    let login_key = match ctsync_get_key(app.clone(), "login_key") {
        Err(_) => {
            setup_connection(
                app.clone(),
                state.clone(),
                window.clone(),
                &server_addr,
                server_port,
            )
            .await?;
            return Ok(());
        }
        Ok(login_key) => login_key,
    };

    // if device_id is not present then just establish TCP connection
    let device_id = match ctsync_get_key(app.clone(), "device_id") {
        Err(_) => {
            setup_connection(
                app.clone(),
                state.clone(),
                window.clone(),
                &server_addr,
                server_port,
            )
            .await?;
            return Ok(());
        }
        Ok(device_id) => device_id,
    };

    setup_connection(
        app.clone(),
        state.clone(),
        window.clone(),
        &server_addr,
        server_port,
    )
    .await?;

    if let Err(_) = login(state.clone(), &login_key, &device_id).await {
        _ = app.emit("show-page", "login_failed");
    }
    Ok(())
}

#[tauri::command]
pub async fn page_to_show(state: tauri::State<'_, AppState>, app: AppHandle) -> Result<(), String> {
    // show new page if no server_address found
    if let Err(_) = ctsync_get_key(app.clone(), "server_address") {
        _ = app.emit("show-page", "new");
        return Ok(());
    };

    if let Ok(server_port) = ctsync_get_key(app.clone(), "server_port") {
        if let Err(_) = server_port.parse::<u16>() {
            // show new page if port number can not be converted to u16
            _ = app.emit("show-page", "new");
            return Ok(());
        }
    } else {
        // show new page if server port number is not found
        _ = app.emit("show-page", "new");
        return Ok(());
    };

    let login_key = match ctsync_get_key(app.clone(), "login_key") {
        Err(_) => {
            _ = app.emit("show-page", "new");
            return Ok(());
        }
        Ok(key) => key,
    };

    let device_id = match ctsync_get_key(app.clone(), "device_id") {
        Err(_) => {
            _ = app.emit("show-page", "new");
            return Ok(());
        }
        Ok(key) => key,
    };

    if let Err(_) = login(state, &login_key, &device_id).await {
        _ = app.emit("show-page", "login_failed");
    }

    Ok(())
}

fn generate_random_string(length: u16) -> String {
    let mut rng = rand::thread_rng();
    (0..length)
        .map(|_| rng.sample(Alphanumeric))
        .map(char::from)
        .collect()
}

// TODO rename to generate_group_invite_key
#[tauri::command]
pub async fn generate_invite_key(
    state: tauri::State<'_, AppState>,
    app: AppHandle,
    exp_hour: i32,
) -> Result<(), String> {
    let key = generate_random_string(INVITE_KEY_SIZE);
    let exp_hour_s = exp_hour.to_string();
    let sign_private_key = match ctsync_get_key(app, "pem_sign_private_key") {
        Ok(k) => k,
        Err(_) => {
            return Err("failed to retrive sing private key from disk".to_string());
        }
    };

    let keypair = PKey::private_key_from_pem(sign_private_key.as_bytes()).unwrap();

    // Sign the data
    let mut signer = Signer::new(MessageDigest::sha256(), &keypair).unwrap();
    signer.update(key.as_bytes()).unwrap();
    signer.update(exp_hour_s.as_bytes()).unwrap();
    let signature = signer.sign_to_vec().unwrap();
    let signature_string = base64::encode_block(&signature);

    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);
    frame.add_kv("action".as_bytes(), "STORE_INVITE_KEY".as_bytes());
    frame.add_kv("key".as_bytes(), key.as_bytes());
    frame.add_kv("exp".as_bytes(), exp_hour_s.as_bytes());
    frame.add_kv("signature".as_bytes(), signature_string.as_bytes());

    if let Err(_) = frame.send(hn_gen_group_invite_key).await {
        return Err("failed to make group invite key generation request".to_string());
    }

    Ok(())
}

#[tauri::command]
pub async fn join_group(
    state: tauri::State<'_, AppState>,
    invite_key: String,
) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "JOIN_GROUP".as_bytes());
    frame.add_kv("key".as_bytes(), invite_key.as_bytes());

    if let Err(_) = frame.send(hn_join_group).await {
        return Err("failed to make group join request".to_string());
    }
    Ok(())
}
#[tauri::command]
pub async fn group_check(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "GROUP_CHECK".as_bytes());

    if let Err(_) = frame.send(hn_group_check).await {
        return Err("failed to make group check request".to_string());
    }
    Ok(())
}

// Approve a device and add it to the same group as the device calling this function.
#[tauri::command]
pub async fn accept_device(
    state: tauri::State<'_, AppState>,
    device_id: String,
) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "ACCEPT_DEVICE".as_bytes());
    frame.add_kv("device_id".as_bytes(), device_id.as_bytes());

    if let Err(_) = frame.send(hn_accept_device).await {
        return Err("failed to make ruquest to accept the device".to_string());
    }
    Ok(())
}

#[tauri::command]
pub async fn stop_clipboard_monitor(
    app: AppHandle,
    state: tauri::State<'_, AppState>,
) -> Result<(), String> {
    println!("stop_clipboard_monitor function got called");
    {
        let mut guard = state.cb_monitor_handle.lock().await;
        *guard = None;
    }
    store_clipboard_send_state(app, false)?;
    Ok(())
}

#[tauri::command]
pub async fn stop_clipboard_receive(app: AppHandle) -> Result<(), String> {
    store_clipboard_recv_state(app, false)?;
    Ok(())
}

#[tauri::command]
pub async fn start_clipboard_receive(app: AppHandle) -> Result<(), String> {
    store_clipboard_recv_state(app, true)?;
    Ok(())
}

#[tauri::command]
pub fn store_clipboard_recv_status(value: bool, app: AppHandle) -> Result<(), String> {
    if let Err(_) = ctsync_set_key(app.clone(), "recv_data", &value.to_string()) {
        return Err("failed to store clipboard receive status".to_string());
    }
    // TODO: create a new action to notify server to not send data
    Ok(())
}

pub fn sha256_from_base64(base64_data: &str) -> Result<String, String> {
    // Calculate SHA256 hash using OpenSSL
    let hash_result = hash(MessageDigest::sha256(), base64_data.as_bytes())
        .map_err(|_| "was unable to create hash of the clipboard content")?;

    // Convert hash bytes to hex string
    let hash_hex: String = hash_result.iter().map(|b| format!("{:02x}", b)).collect();

    Ok(hash_hex)
}

fn aes_encrypt(data: &[u8]) -> Result<(Vec<u8>, Vec<u8>, Vec<u8>), &'static str> {
    let cipher = Cipher::aes_256_cbc();

    // Generate random key and IV(initialization vector)
    let mut key = vec![0; cipher.key_len()];
    let mut iv = vec![0; cipher.iv_len().unwrap()];
    rand_bytes(&mut key).map_err(|_| "Encryption failed")?;
    rand_bytes(&mut iv).map_err(|_| "Encryption failed")?;

    let mut encrypter =
        Crypter::new(cipher, Mode::Encrypt, &key, Some(&iv)).map_err(|_| "Encryption failed")?;

    let mut ciphertext = vec![0; data.len() + cipher.block_size()];
    let mut count = encrypter
        .update(data, &mut ciphertext)
        .map_err(|_| "Encryption failed")?;
    count += encrypter
        .finalize(&mut ciphertext[count..])
        .map_err(|_| "Encryption failed")?;
    ciphertext.truncate(count);

    Ok((ciphertext, key, iv))
}

struct SendableEncData {
    encd_data: Vec<u8>,
    encd_key: Vec<u8>,
    encd_iv: Vec<u8>,
    signature: Vec<u8>,
}

fn gen_sendable_cb_data(
    content: &str,
    enc_key: &str,
    private_key: &str,
) -> Option<SendableEncData> {
    // encrypt the content with newly created AES key
    match aes_encrypt(content.as_bytes()) {
        Ok((ciphereddata, key, iv)) => {
            // create the encryptor
            let rsa = Rsa::public_key_from_pem(enc_key.as_bytes()).ok()?;
            let public_key = PKey::from_rsa(rsa).ok()?;
            let mut encrypter = Encrypter::new(&public_key).ok()?;
            encrypter.set_rsa_padding(Padding::PKCS1).ok()?;

            let key_len = encrypter.encrypt_len(&key).ok()?;
            let iv_len = encrypter.encrypt_len(&iv).ok()?;

            let mut encd_key = vec![0; key_len];
            let mut encd_iv = vec![0; iv_len];

            let encrypted_len = encrypter.encrypt(&key, &mut encd_key).ok()?;
            encd_key.truncate(encrypted_len);

            let encrypted_len = encrypter.encrypt(&iv, &mut encd_iv).ok()?;
            encd_iv.truncate(encrypted_len);

            // create signer using sender private key
            let sign_rsa = Rsa::private_key_from_pem(private_key.as_bytes()).ok()?;
            let rsa_private_key = PKey::from_rsa(sign_rsa).ok()?;
            let mut signer = Signer::new(MessageDigest::sha256(), &rsa_private_key).ok()?;

            // create signature
            signer.update(&ciphereddata).ok()?;
            let signature = signer.sign_to_vec().ok()?;

            let data = SendableEncData {
                encd_data: ciphereddata,
                encd_key,
                encd_iv,
                signature,
            };

            Some(data)
        }
        Err(_) => None,
    }
}

#[derive(Debug, Deserialize)]
struct DeviceInfo {
    device_id: String,
    public_key: String,
    is_online: bool,
}

async fn send_clipboard_content(
    content: &str,
    devices_json_str: &str,
    frame_appstate: FrameAppState,
    sign_private_key: &str,
    this_device_id: &str,
) {
    let devices = match serde_json::from_str::<Vec<DeviceInfo>>(devices_json_str) {
        Err(_) => return,
        Ok(val) => val,
    };

    for device in devices {
        if device.is_online {
            if device.device_id == this_device_id {
                continue;
            }
            if let Some(data) = gen_sendable_cb_data(&content, &device.public_key, sign_private_key)
            {
                let mut frame = OwnedCTSFrame::new(frame_appstate.clone());
                frame.add_kv("action".as_bytes(), "CLIPBOARD".as_bytes());
                frame.add_kv("encd_data".as_bytes(), &data.encd_data);
                frame.add_kv("encd_data_signature".as_bytes(), &data.signature);
                frame.add_kv("encd_key".as_bytes(), &data.encd_key);
                frame.add_kv("encd_iv".as_bytes(), &data.encd_iv);
                frame.add_kv("device_id".as_bytes(), device.device_id.as_bytes());
                _ = frame.send(hn_clipboard_sent).await;
            }
        }
    }
}
#[derive(Debug, Deserialize, Serialize)]
pub struct CBState {
    pub recv_cb: bool,
    pub send_cb: bool,
}

fn store_clipboard_send_state(app: AppHandle, send: bool) -> Result<(), String> {
    match ctsync_get_key(app.clone(), "cb_state") {
        Err(_) => {
            //creae new entry
            let cb_state = CBState {
                recv_cb: true,
                send_cb: true,
            };
            let json_string = serde_json::to_string(&cb_state)
                .map_err(|_| "failed to convert object to json string".to_string())?;
            ctsync_set_key(app.clone(), "cb_state", &json_string)
                .map_err(|_| "failed to store clipboard status".to_string())?;
        }
        Ok(str_val) => {
            // update current value
            let mut cb_state = match serde_json::from_str::<CBState>(&str_val) {
                Err(_) => {
                    return Err(
                        "failed up convert clipboard state from string to serde object".to_string(),
                    )
                }
                Ok(val) => val,
            };
            cb_state.send_cb = send;
            let json_string = serde_json::to_string(&cb_state)
                .map_err(|_| "failed to convert object to json string".to_string())?;
            ctsync_set_key(app.clone(), "cb_state", &json_string)
                .map_err(|_| "failed to store clipboard status".to_string())?;
        }
    }
    Ok(())
}

fn store_clipboard_recv_state(app: AppHandle, recv: bool) -> Result<(), String> {
    match ctsync_get_key(app.clone(), "cb_state") {
        Err(_) => {
            //creae new entry
            let cb_state = CBState {
                recv_cb: true,
                send_cb: true,
            };
            let json_string = serde_json::to_string(&cb_state)
                .map_err(|_| "failed to convert object to json string".to_string())?;
            ctsync_set_key(app.clone(), "cb_state", &json_string)
                .map_err(|_| "failed to store clipboard status".to_string())?;
        }
        Ok(str_val) => {
            // update current value
            let mut cb_state = match serde_json::from_str::<CBState>(&str_val) {
                Err(_) => {
                    return Err(
                        "failed up convert clipboard state from string to serde object".to_string(),
                    )
                }
                Ok(val) => val,
            };
            cb_state.recv_cb = recv;
            let json_string = serde_json::to_string(&cb_state)
                .map_err(|_| "failed to convert object to json string".to_string())?;
            ctsync_set_key(app.clone(), "cb_state", &json_string)
                .map_err(|_| "failed to store clipboard status".to_string())?;
        }
    }
    Ok(())
}

#[tauri::command]
pub async fn start_clipboard_monitor(
    app: AppHandle,
    state: tauri::State<'_, AppState>,
) -> Result<(), String> {
    let (tx, rx) = oneshot::channel::<()>();
    {
        let mut guard = state.cb_monitor_handle.lock().await;
        *guard = Some(tx);
    }

    // store the state to local storage
    store_clipboard_send_state(app.clone(), true)?;

    // Clone just the data_channel before moving into the task
    let content_hash = state.content_hash.clone();
    let frame_appstate: FrameAppState = (&*state).into();

    tokio::spawn({
        let frame_appstate = frame_appstate.clone();
        async move {
            tokio::select! {
                _ = async {
                    match rx.await{
                        Ok(_) =>{}
                        Err(_) =>{}
                    }
                } =>{println!("stopped monitoring clipboard")}
                _ = async {
                    loop {
                        let sapp = app.clone();
                        if let Some(content_now) = app.clipboard().read_text().ok() {
                        let hash =
                            openssl::hash::hash(openssl::hash::MessageDigest::sha256(), &content_now.clone().into_bytes())
                                .unwrap()
                                .to_vec();

                            // let new_content_hash = sha256_from_base64(&new_content_base64).unwrap();

                            let mut guard = content_hash.lock().await;
                            match &*guard {
                                Some(value) => {
                                    if *value != hash {
                                        if let (Ok(devices_json_str), Ok(sign_private_key), Ok(this_device_id)) = (
                                            ctsync_get_key(sapp.clone(), "devices"),
                                            ctsync_get_key(sapp.clone(), "pem_sign_private_key"),
                                            ctsync_get_key(sapp, "device_id"),
                                            ) {
                                            send_clipboard_content(
                                                &content_now,
                                                &devices_json_str,
                                                frame_appstate.clone(),
                                                &sign_private_key,
                                                &this_device_id,
                                            ).await;
                                        }
                                        *guard = Some(hash);
                                    }
                                },
                                None => {
                                    *guard = Some(hash);
                                },
                            }
                        }
                        sleep(Duration::new(1, 0)).await;
                    }
                }=>{
                }
            }
        }
    });
    Ok(())
}

#[tauri::command]
pub async fn isconnected(state: tauri::State<'_, AppState>) -> Result<(), ()> {
    let guard = state.task_handle.lock().await;
    if let Some(_) = *guard {
        Ok(())
    } else {
        Err(())
    }
}

#[tauri::command]
pub async fn get_invite_key(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "GET_INVITE_KEY".as_bytes());

    if let Err(_) = frame.send(hn_get_group_invite_key).await {
        return Err("failed to make request to retrive group join invite key ".to_string());
    }

    Ok(())
}

#[tauri::command]
pub async fn get_online_devices(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "ONLINE_DEVICES".as_bytes());

    if let Err(_) = frame.send(hn_get_online_devices).await {
        return Err("failed to make request".to_string());
    }
    Ok(())
}

#[tauri::command]
pub async fn kick_device(
    state: tauri::State<'_, AppState>,
    device_id: String,
) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "KICK_DEVICE".as_bytes());
    frame.add_kv("device_id".as_bytes(), device_id.as_bytes());

    if let Err(_) = frame.send(hn_kick_device).await {
        return Err("failed to make request".to_string());
    }

    Ok(())
}

// #[tauri::command]
// pub async fn leave_group(state: tauri::State<'_, AppState>) -> Result<(), String> {
//     let frame_appstate: FrameAppState = (&*state).into();
//     let mut frame = OwnedCTSFrame::new(frame_appstate);

//     frame.add_kv("action".as_bytes(), "KICK_DEVICE".as_bytes());
//     frame.add_kv("device_id".as_bytes(), device_id.as_bytes());

//     if let Err(_) = frame.send(hn_kick_device).await {
//         return Err("failed to make request".to_string());
//     }

//     Ok(())
// }

#[tauri::command]
pub fn restore_state(app: AppHandle) -> Result<(), String> {
    _ = app.emit(
        "app-state",
        "TODO, return JSON payload to restore app state",
    );
    Ok(())
}

// delete local account information, doesnt delete aynthing from server
#[tauri::command]
pub async fn delete_local_account(
    state: tauri::State<'_, AppState>,
    app: AppHandle,
) -> Result<(), String> {
    _ = ctsync_clear_store(app);
    let task_handle = state.task_handle.clone();
    terminate_con(task_handle).await;
    Ok(())
}
