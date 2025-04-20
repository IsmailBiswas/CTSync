#pragma once
#include "conn_association.h"
#include <openssl/ssl.h>
#include <stdint.h>
#include <sys/types.h>

// #define IS_AUTHTHENTICATED (1 << 0) // 0b00000001
// #define HAS_SRV_ACCESS (1 << 1)     // 0b00000010
// #define PRIVATE_KEY_FILE "./server_files/pkey.pem"
// #define CERT_CHAIN_FILE "./server_files/self_signed_cert.pem"
#define DATABASE_LOC "./server_files/ctsync.db"

// new
#define INVITE_KEY_FILE "./server_files/invite_keys.txt"
#define ACTION_KEY "action"

typedef struct{
  ConnInfo **conn_info;
  DevToConn **dev_to_conn_id;
  uint32_t res_id; // do need it here? 
} CTSyncState;


extern CTSyncState *ctsync_state;
