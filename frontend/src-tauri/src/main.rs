// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
use tokio;

#[tokio::main]
async fn main() {
    ctsync_lib::run().await
}
