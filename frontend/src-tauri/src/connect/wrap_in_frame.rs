use crate::constants::CEQUIQ_PREFIX;
use std::mem;

pub fn create_frame(data: &[u8]) -> Vec<u8> {
    let mut frame = Vec::with_capacity(CEQUIQ_PREFIX.len() + mem::size_of::<u64>() + data.len());
    frame.extend_from_slice(CEQUIQ_PREFIX.as_bytes());
    let size: u64 = data.len() as u64;
    let size_be = size.to_be_bytes();
    frame.extend_from_slice(&size_be);
    frame.extend_from_slice(data);
    frame
}
