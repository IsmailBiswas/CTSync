use colored::*;
use std::sync::Arc;
use std::{collections::HashMap, time::SystemTime};
use tauri::AppHandle;
use tokio::sync::{broadcast, Mutex};

use crate::constants::{
    CTSYNC_FRAME_HEADER_SIZE_BYTES, CTSYNC_KEY_SIZE_FIELD_SIZE_BYTES,
    CTSYNC_RESPONSE_INDEX_SIZE_BYTES, CTSYNC_VALUE_SIZE_FIELD_SIZE_BYTES, DEFAULT_FRAME_INDEX,
};
use crate::{connect::AppState, constants::DEFAULT_FRAME_STATUS};

#[derive(Clone)]
pub struct FrameAppState {
    pub data_channel: Arc<Mutex<Option<broadcast::Sender<Vec<u8>>>>>,
    pub response_queue: Arc<Mutex<Option<HashMap<u32, (u64, fn(AppHandle, ReceivedCTSFrame))>>>>,
    pub current_res_index: Arc<Mutex<Option<u32>>>,
}

impl From<&AppState> for FrameAppState {
    fn from(app_state: &AppState) -> Self {
        Self {
            data_channel: app_state.data_channel.clone(),
            response_queue: app_state.response_queue.clone(),
            current_res_index: app_state.current_res_index.clone(),
        }
    }
}

pub struct CTSFrameData {
    pub res_index: u32,
    pub status_code: u8,
    pub kv: HashMap<Vec<u8>, Vec<u8>>,
}

pub struct ReceivedCTSFrame(pub CTSFrameData);

pub struct OwnedCTSFrame(pub CTSFrameData, FrameAppState);

impl OwnedCTSFrame {
    pub fn new(state: FrameAppState) -> Self {
        let data = CTSFrameData {
            res_index: 0, // default request id, to be updated by serializer
            status_code: DEFAULT_FRAME_STATUS,
            kv: HashMap::new(),
        };
        Self(data, state)
    }

    pub fn add_kv(&mut self, key: &[u8], value: &[u8]) {
        self.inner().kv.insert(key.to_vec(), value.to_vec());
    }

    async fn serialize(&mut self) -> Result<Vec<u8>, ()> {
        let mut data: Vec<u8> = Vec::new();
        data.extend_from_slice(&self.inner().res_index.to_be_bytes());

        data.extend_from_slice(&[DEFAULT_FRAME_STATUS]);

        for (key, value) in &self.inner().kv {
            data.extend_from_slice(&(key.len() as u8).to_be_bytes());
            data.extend_from_slice(&(value.len() as u64).to_be_bytes());

            data.extend(key);
            data.extend(value);
        }

        Ok(data)
    }

    // returns the inner datatype, lol (I know explians nothing)
    pub fn inner(&mut self) -> &mut CTSFrameData {
        &mut self.0
    }

    // returns the tauri AppState from winthin the instance
    fn app_state(&self) -> &FrameAppState {
        &self.1
    }

    async fn add_response_queue(
        &self,
        response_queue: Arc<Mutex<Option<HashMap<u32, (u64, fn(AppHandle, ReceivedCTSFrame))>>>>,
        callback: fn(AppHandle, ReceivedCTSFrame),
        req_index: &u32,
    ) {
        let timestamp = SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();

        let mut guard = response_queue.lock().await;
        if let Some(ref mut res_queue) = *guard {
            res_queue.insert(*req_index, (timestamp, callback));
        }
    }

    // send the data (make request) to the server
    pub async fn send(
        &mut self,
        response_handler: fn(AppHandle, ReceivedCTSFrame),
    ) -> Result<(), ()> {
        let mut res_index = 0;
        {
            let mut guard = self.app_state().current_res_index.lock().await;
            if let Some(ref mut current_id) = *guard {
                *current_id = current_id.wrapping_add(2);
                res_index = *current_id;
            }
        }
        self.inner().res_index = res_index;

        // serialize the data
        let data = self.serialize().await?;

        // get the generated request index which will also work as response id when response is received
        let guard = self.app_state().data_channel.lock().await;
        if let Some(ref sender) = *guard {
            sender.send(data).map_err(|_| ())?;
            self.add_response_queue(
                self.app_state().response_queue.clone(),
                response_handler,
                &res_index,
            )
            .await;
            return Ok(());
        }
        return Err(());
    }
}

impl ReceivedCTSFrame {
    pub fn get_key(&self, key: &[u8]) -> Result<Vec<u8>, ()> {
        let value = self.inner().kv.get(key).ok_or(())?;
        return Ok(value.to_vec());
    }

    // TODO: comment
    pub fn deserialize(data: Vec<u8>) -> Result<CTSFrameData, ()> {
        /*
        +--------------------+
        | request_id (u32)   |
        +--------------------+
        | status_code (u8)   |
        +--------------------+
        | key1_size (u8)     |
        +--------------------+
        | value1_size (u64)  |
        +--------------------+
        | key1               |
        +--------------------+
        | value1             |
        +--------------------+
        | key2_size (u8)     |
        +--------------------+
        | value2_size (u64)  |
        +--------------------+
        | key2               |
        +--------------------+
        | value2             |
        +--------------------+
        | ...                |
        +--------------------+
         */

        if data.len() < CTSYNC_FRAME_HEADER_SIZE_BYTES as usize {
            return Err(());
        }

        let mut frame = CTSFrameData {
            res_index: DEFAULT_FRAME_INDEX,
            status_code: DEFAULT_FRAME_STATUS,
            kv: HashMap::new(),
        };

        let res_index = u32::from_be_bytes(
            data[0..CTSYNC_RESPONSE_INDEX_SIZE_BYTES.into()]
                .try_into()
                .map_err(|_| ())?,
        );

        let status_code = *data
            .get(CTSYNC_RESPONSE_INDEX_SIZE_BYTES as usize)
            .ok_or_else(|| ())?;

        frame.res_index = res_index;
        frame.status_code = status_code;

        let mut read_byte: usize = CTSYNC_FRAME_HEADER_SIZE_BYTES.into();
        let data_length = data.len();

        while read_byte < data_length {
            let key_size_end =
                match read_byte.checked_add(CTSYNC_KEY_SIZE_FIELD_SIZE_BYTES as usize) {
                    Some(pos) if pos <= data_length => pos,
                    _ => {
                        break;
                    }
                };

            let key_size = *data.get(read_byte as usize).ok_or_else(|| {
                println!("{}", "Failed to get key.".red());
            })?;

            read_byte = key_size_end;

            let value_size_end =
                match read_byte.checked_add(CTSYNC_VALUE_SIZE_FIELD_SIZE_BYTES as usize) {
                    Some(pos) if pos <= data_length => pos,
                    _ => {
                        return Err(());
                    }
                };

            let value_size =
                u64::from_be_bytes(data[read_byte..value_size_end].try_into().map_err(|_| ())?);

            read_byte = value_size_end;
            let key_end = match read_byte.checked_add(key_size as usize) {
                Some(pos) if pos <= data_length => pos,
                _ => {
                    return Err(());
                }
            };

            let key = &data[read_byte..key_end];
            read_byte = key_end;

            let value_end = match read_byte.checked_add(value_size as usize) {
                Some(pos) if pos <= data_length => pos,
                _ => {
                    return Err(());
                }
            };

            let value = &data[read_byte..value_end];
            read_byte = value_end;

            frame.kv.insert(key.to_vec(), value.to_vec());
        }
        Ok(frame)
    }

    pub fn new(data: Vec<u8>) -> Result<ReceivedCTSFrame, ()> {
        if let Ok(frame) = Self::deserialize(data) {
            return Ok(Self(frame));
        }
        Err(())
    }

    pub fn inner(&self) -> &CTSFrameData {
        &self.0
    }
}
