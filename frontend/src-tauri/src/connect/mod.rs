pub mod datain;
pub mod wrap_in_frame;
use crate::constants::{CEQUIQ_HEADER_SIZE_BYTES, CEQUIQ_PREFIX};
use crate::data::{FrameAppState, OwnedCTSFrame, ReceivedCTSFrame};
use crate::keys;
use crate::response::{hn_device_register, hn_server_access};
use crate::restore::{ctsync_get_key, ctsync_set_key};
use native_tls::TlsConnector as NativeTlsConnector;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::sync::{mpsc, Arc};
use tokio::io::{split, AsyncReadExt, AsyncWriteExt};
use wrap_in_frame::create_frame;
use x509_parser::prelude::*;

use tauri::{AppHandle, Emitter};
use tokio::net::TcpStream;
use tokio::sync::{broadcast, oneshot, Mutex};
use tokio_native_tls::{TlsConnector, TlsStream};

#[derive(Serialize, Deserialize)]
pub struct EventData {
    pub event_name: String,
    pub payload: String,
}

pub struct AppState {
    pub data_channel: Arc<Mutex<Option<broadcast::Sender<Vec<u8>>>>>, //For receiving data which will be transmited though socket
    pub task_handle: Arc<Mutex<Option<broadcast::Sender<()>>>>,       // For terminating tokio tasks
    pub cb_monitor_handle: Arc<Mutex<Option<oneshot::Sender<()>>>>,
    pub server_pub_key_hash: Arc<Mutex<Option<String>>>,
    pub con_addr: Arc<Mutex<Option<String>>>,
    pub content_hash: Arc<Mutex<Option<Vec<u8>>>>,
    // The HashMap fields are response id (u32), timestamp(u64) and the callbacke function
    pub response_queue: Arc<Mutex<Option<HashMap<u32, (u64, fn(AppHandle, ReceivedCTSFrame))>>>>,
    pub current_res_index: Arc<Mutex<Option<u32>>>,
}

#[derive(Serialize, Deserialize)]
pub struct ApplicationData {
    pub address: String,
    pub port: u16,
    pub hash: String,
    pub setup_complete: bool,
    pub receive_content: bool,
    pub send_content: bool,
}

#[derive(Serialize, Deserialize)]
pub struct Cookie {
    pub key: String,
    pub device_id: String,
}

fn gen_public_key_hash(tls_stream: &TlsStream<TcpStream>) -> Option<String> {
    if let Some(cert) = tls_stream.get_ref().peer_certificate().unwrap() {
        // pet DER-encoded certificate
        let der = cert.to_der().unwrap();

        // parse the certificate
        let (_, x509) = X509Certificate::from_der(&der)
            .map_err(|e| format!("Error parsing certificate: {:?}", e))
            .unwrap();

        // extract the public key
        let public_key = x509.public_key().raw;

        // compute the hash of the public key
        let mut hasher = Sha256::new();
        hasher.update(public_key);
        let hash = hasher.finalize();

        println!("server's public key hash: {:x}", hash);
        return Some(format!("{:x}", hash));
    } else {
        println!("no peer certificate found.");
        None
    }
}

#[derive(Debug)]
enum CequiqFrameResult {
    ConnectionClosed,
    BadData,
    Error,
}

async fn get_cequiq_frame_body(
    stream: &mut tokio::io::ReadHalf<TlsStream<TcpStream>>,
    buffer: &mut Vec<u8>,
) -> Result<Vec<u8>, CequiqFrameResult> {
    // loop until minimum of header size data is received
    while buffer.len() < CEQUIQ_HEADER_SIZE_BYTES as usize {
        match stream.read_buf(buffer).await {
            Ok(0) => return Err(CequiqFrameResult::ConnectionClosed),
            Ok(_) => {}
            Err(_) => {
                return Err(CequiqFrameResult::Error);
            }
        }
    }

    // check if data starts with cequiq prefix
    match std::str::from_utf8(&buffer[0..CEQUIQ_PREFIX.len()]) {
        Ok(s) => {
            if s != CEQUIQ_PREFIX {
                return Err(CequiqFrameResult::BadData);
            }
        }
        Err(_) => return Err(CequiqFrameResult::BadData),
    }

    // try to extract body size
    let body_size = u64::from_be_bytes(
        buffer[CEQUIQ_PREFIX.len()..CEQUIQ_HEADER_SIZE_BYTES as usize]
            .try_into()
            .map_err(|_| CequiqFrameResult::BadData)?,
    );

    // TODO: maybe have a limit on body_size

    // remove header data from the buffer
    if buffer.len() >= CEQUIQ_HEADER_SIZE_BYTES as usize {
        buffer.drain(0..CEQUIQ_HEADER_SIZE_BYTES as usize);
    }
    // read until body_size data has been read
    while buffer.len() < body_size as usize {
        match stream.read_buf(buffer).await {
            Ok(0) => return Err(CequiqFrameResult::ConnectionClosed),
            Ok(_) => {}
            Err(_) => {
                return Err(CequiqFrameResult::Error);
            }
        }
    }

    // make copy of the body data
    let data = buffer[0..body_size as usize].to_vec();

    // clear body data of buffer
    if buffer.len() >= body_size as usize {
        buffer.drain(0..body_size as usize);
    }

    Ok(data)
}

async fn create_tls_stream(server: &str, port: u16) -> Result<TlsStream<TcpStream>, String> {
    let native_tls_connector = NativeTlsConnector::builder()
        .danger_accept_invalid_certs(true)
        .build()
        .map_err(|e| format!("TLS connector initialization failed: {}", e))?;

    let connector = TlsConnector::from(native_tls_connector);

    let tcp_connection = TcpStream::connect(format!("{}:{}", server, port))
        .await
        .map_err(|e| format!("TCP connection to {}:{} failed: {}", server, port, e))?;

    tcp_connection
        .set_nodelay(true)
        .map_err(|e| format!("Failed to set TCP_NODELAY: {}", e))?;

    connector
        .connect(server, tcp_connection)
        .await
        .map_err(|e| format!("TLS handshake with {}:{} failed: {}", server, port, e))
}

// sends signal to stop listening for incoming data
pub async fn terminate_con(task_handle: Arc<Mutex<Option<broadcast::Sender<()>>>>) {
    let mut guard = task_handle.lock().await;

    match guard.take() {
        Some(handle) => match handle.send(()) {
            Ok(_) => {
                eprintln!("Sent task termination signal");
            }
            Err(e) => {
                eprintln!("Failed to send termination signal: {}", e);
            }
        },
        None => {}
    }
}

async fn handle_stream(
    state: tauri::State<'_, AppState>,
    app: AppHandle,
    window: tauri::Window,
    tls_stream: TlsStream<TcpStream>,
    _data_channel: Arc<Mutex<Option<broadcast::Sender<Vec<u8>>>>>,
    task_handle: Arc<Mutex<Option<broadcast::Sender<()>>>>,
) {
    // create a channel to send a termination signal to the other tasks(thread~)
    let (task_handle_tx, mut task_handle_rx) = broadcast::channel(5);
    let mut task_handle_rx2 = task_handle_tx.subscribe();
    let mut task_handle_rx3 = task_handle_tx.subscribe();

    // create channel to send event data to the event emiter task(thread~)
    let (_event_data_tx, event_data_rx) = mpsc::channel::<String>();
    let event_data_rx = Arc::new(Mutex::new(event_data_rx));

    let window_clone = window.clone();

    // listens on mpsc channel and emits tauri event with the received data
    tokio::spawn({
        let event_rx = event_data_rx.clone();
        async move {
            tokio::select! {
                    _ = async {
                        task_handle_rx3.recv().await} =>{
                            println!("event emitter: exting");
                    }
                _ = async {
                    while let Ok(data) = event_rx.lock().await.recv() {
                        match serde_json::from_str::<EventData>(&data) {
                            Ok(event_data) => {
                                println!("sending event to name:{}, value:{}", event_data.event_name, event_data.payload);
                                if let Err(e) = window_clone.emit(&event_data.event_name, event_data.payload) {
                                    eprintln!("failed to emit event: {}", e);
                                }
                            }
                            Err(e) => {
                                eprintln!("failed to deserialize event data: {}", e);
                                continue;
                            }
                        }
                    }


                } =>{}

            }
        }
    });

    let (reader, writer) = split(tls_stream);
    let reader = Arc::new(Mutex::new(reader));
    let data_channel = state.data_channel.clone();

    // listens for incoming data from client
    tokio::spawn({
        let reader = Arc::clone(&reader);
        // let data_channel = state.data_channel.clone();
        let t_task_handle = state.task_handle.clone();
        let conetent_hash = state.content_hash.clone();
        let res_queue = state.response_queue.clone();
        async move {
            tokio::select! {
                _ = async { task_handle_rx.recv().await.ok()} =>{
                    println!("Reader: Received signal to terminate server connection.");
                }
                _ = async {

                    loop{
                        println!("Reader: Looping inside the reader tokio task");
                        let mut guard = reader.lock().await;
                        let read_half = &mut *guard;
                        let mut buffer: Vec<u8> = vec![];

                        // read data
                        match get_cequiq_frame_body(read_half, &mut buffer).await {
                            Ok(data) => {
                                datain::data_processor(&res_queue, app.clone(), data, conetent_hash.clone()).await;
                            }
                            // if reading data fials
                            Err(e) => {
                                match e {
                                    // emit event to fron-end when connection is closed
                                    CequiqFrameResult::ConnectionClosed =>{
                                         _ = app.emit("disconnect", "connection was closed");
                                        break;
                                    }
                                    // if malformed data is received, do nothing
                                    CequiqFrameResult::BadData=>{}
                                    CequiqFrameResult::Error=>{
                                        terminate_con(t_task_handle).await;
                                        break;
                                    }
                                }
                            }// read error
                        } // read data

                    } //loop

                }=>{}
            }
        }
    });

    // create channel to receive transmitable data
    let (task_msg_tx, mut task_msg_rx) = broadcast::channel::<Vec<u8>>(5);
    {
        let mut guard = data_channel.lock().await;
        println!("Storing sender channel in the state");
        *guard = Some(task_msg_tx);
    }

    let writer = Arc::new(Mutex::new(writer));

    // writes data
    tokio::spawn({
        let writer = Arc::clone(&writer);
        async move {
            tokio::select! {
            _ = async { task_handle_rx2.recv().await.ok()} =>{
                println!("Writer: Received signal to terminate server connection.");
            }
            _ = async {
                    loop {
                        println!("Writer: Looping inside the writer tokio task");
                        match task_msg_rx.recv().await {
                            Ok(message) =>{
                                let mut writer = writer.lock().await;
                                let frame_data = create_frame(&message);
                                _ = writer.write_all(&frame_data).await;
                                writer.flush().await.unwrap();
                            }
                            Err(_) =>{
                                println!("Writer: Stopped listening for incoming channel data as channel was not Ok.");
                                break;
                            }

                        };
                    }
                } =>{}
            }
        }
    });

    {
        let mut guard = task_handle.lock().await;
        *guard = Some(task_handle_tx);
    }
}

pub async fn setup_connection(
    app: AppHandle,
    state: tauri::State<'_, AppState>,
    window: tauri::Window,
    server: &str,
    port: u16,
) -> Result<(), String> {
    let tls_stream = create_tls_stream(server, port)
        .await
        .map_err(|e| e.to_string())?;

    // ------------------------------------ do I need to store it here
    let server_pub_key_hash = gen_public_key_hash(&tls_stream)
        .ok_or_else(|| "failed to generate public key hash".to_string())?;
    ctsync_set_key(app.clone(), "server_pub_key_hash", &server_pub_key_hash)?;

    ctsync_set_key(app.clone(), "server_address", server)?;
    ctsync_set_key(app.clone(), "server_port", &port.to_string())?;

    // ------------------------------------

    // If task is running, send task termination signal.
    {
        let mut guard = state.data_channel.lock().await;
        *guard = None;
        println!("Removed sender channel");
    }
    let state_task_handle = state.task_handle.clone();
    terminate_con(state_task_handle).await;

    // Now there is no reader for the message channel but the writer exists.
    handle_stream(
        state.clone(),
        app,
        window,
        tls_stream,
        state.data_channel.clone(),
        state.task_handle.clone(),
    )
    .await;

    Ok(())
}

#[tauri::command]
pub async fn get_server_public_key_hash(
    state: tauri::State<'_, AppState>,
    window: tauri::Window,
    app: AppHandle,
    port: u16,
    server: &str,
) -> Result<(), String> {
    if let Err(_) = setup_connection(app.clone(), state.clone(), window, server, port).await {
        _ = app.emit("error", "failed to connect to the provided address");
        return Err(format!("failed to connect to {}:{}", server, port));
    }

    if let Ok(hash) = ctsync_get_key(app.clone(), "server_pub_key_hash") {
        _ = app.emit("server-pub-key-hash", hash);
        return Ok(());
    }

    Err("unexpected error occurred".to_string())
}

#[tauri::command]
pub async fn verify_server_access(
    state: tauri::State<'_, AppState>,
    invite_key: &str,
) -> Result<(), String> {
    let frame_appstate: FrameAppState = (&*state).into();
    let mut frame = OwnedCTSFrame::new(frame_appstate);

    frame.add_kv("action".as_bytes(), "ACCESS_SERVER".as_bytes());
    frame.add_kv("server_invite_key".as_bytes(), invite_key.as_bytes());
    match frame.send(hn_server_access).await {
        Ok(_) => return Ok(()),
        Err(_) => {
            return Err("failed to make 'server verify access' request".to_string());
        }
    }
}

#[tauri::command]
pub async fn register_device(
    state: tauri::State<'_, AppState>,
    device_name: &str,
    app: AppHandle,
) -> Result<(), String> {
    match keys::gen_keys(app.clone()) {
        Ok((sign_pub_key, pub_key)) => {
            let frame_appstate: FrameAppState = (&*state).into();
            let mut frame = OwnedCTSFrame::new(frame_appstate);
            frame.add_kv("action".as_bytes(), "REGISTER_DEVICE".as_bytes());
            frame.add_kv("device_name".as_bytes(), device_name.as_bytes());
            frame.add_kv("sign_pub_key".as_bytes(), sign_pub_key.as_bytes());
            frame.add_kv("pub_key".as_bytes(), pub_key.as_bytes());
            match frame.send(hn_device_register).await {
                Ok(_) => return Ok(()),
                Err(_) => {
                    return Err("failed to make device register request".to_string());
                }
            }
        }
        Err(_) => {
            // emit event which will be shown as notification
            _ = app.emit("error", "was unable to generate private and public keys");
            // return error for this command
            return Err("was unable to generate private and public keys".to_string());
        }
    }
}
