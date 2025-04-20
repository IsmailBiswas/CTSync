// pub const HEADER_BODY_SEPARATOR: &str = "\r\n\r\n";
// pub const DELIMITER: &str = "\r\n";
// pub const ENDING_SEQ: &str = "\r\r\n";
// pub const ENDING_BYTE: u8 = ENDING_SEQ.as_bytes()[ENDING_SEQ.len() - 1];
pub const APPLICATION_STATE: &str = "./application_state.txt";
pub const COOKIE_FILE_LOCATION: &str = "./cookie.txt";
pub const INVITE_KEY_SIZE: u16 = 8;
pub const SIGN_PRIVATE_KEY_FILE: &str = "./pems/sign_private_key.pem";
pub const SIGN_PUBLIC_KEY_FILE: &str = "./pems/sign_public_key.pem";
pub const PRIVATE_KEY_FILE: &str = "./pems/private_key.pem";
pub const PUBLIC_KEY_FILE: &str = "./pems/public_key.pem";

pub const CTSYNC_STORE_NAME: &str = "ctsync_store.json";

// Constants related to the CTSYNC frame structure.
pub const DEFAULT_FRAME_STATUS: u8 = 0;
pub const DEFAULT_FRAME_INDEX: u32 = 0;
pub const CTSYNC_RESPONSE_INDEX_SIZE_BYTES: u8 = 4;
pub const CTSYNC_RESPONSE_STATUS_SIZE: u8 = 1;
pub const CTSYNC_KEY_SIZE_FIELD_SIZE_BYTES: u8 = 1;
pub const CTSYNC_VALUE_SIZE_FIELD_SIZE_BYTES: u8 = 8;
pub const CTSYNC_FRAME_HEADER_SIZE_BYTES: u8 =
    CTSYNC_RESPONSE_INDEX_SIZE_BYTES + CTSYNC_RESPONSE_STATUS_SIZE;

// Cequiq frame
pub const CEQUIQ_PREFIX: &str = "XOXO";
pub const CEQUIQ_BODY_SIZE_FIELD_BYTES: u8 = 8;
pub const CEQUIQ_HEADER_SIZE_BYTES: u8 = CEQUIQ_PREFIX.len() as u8 + CEQUIQ_BODY_SIZE_FIELD_BYTES;

//
pub const RESPONSE_VALID_WINTHIN_SEC: u8 = 10;

// response code
pub const RES_FAIL: u8 = 40;
pub const RES_OK: u8 = 20;
pub const RES_ERROR: u8 = 50;
