## Problem
I naively thought I could create a TlsStream and use it in two different
threads—one for **continuously** listening for incoming data and the other for
writing data. However, I couldn't do so as it required multiple mutable borrows.

## Communicating with backend server

The TLS connection is first established with the backend server. This
connection (TlsStream) is then split using tokio::split into a reader and
writer. The reader half is used in a tokio task to listen for incoming data,
while the writer half is utilized in another tokio task. In the writer task, a
loop listens for data from a channel and writes it to the writer half of the
TlsStream.


## Text Protocol Overview 

### Frame Structure
- Each request (Frame) ends with `ENDING_SEQ`: `\r\r\n`

### Sections
- A Frame has two sections: `HEADER` and `BODY`
- `HEADER` and `BODY` are separated by `HEADER_BODY_DELIMITER`: `\r\n\r\n`

### HEADER Structure
- `HEADER` consists of `ACTION` and `HEADER_LINES`
- Each `ACTION` and `HEADER_LINE` ends with `DELIMITER`: `\r\n`
- The first delimited line in `HEADER` is the `ACTION`.

### HEADER_LINE Format
- Each `HEADER_LINE` contains `KEY` and `VALUE`
- `KEY` and `VALUE` are separated by `KEY_VALUE_DELIMITER`: `:`
