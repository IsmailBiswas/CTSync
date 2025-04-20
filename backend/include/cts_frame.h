#pragma  once
#include "parser.h"
#include <stdint.h>

#define REQUEST_ID_BYTE (sizeof(uint32_t))
#define STATUS_CODE_BYTE (sizeof(uint8_t))
#define KEY_SIZE_BYTE (sizeof(uint8_t))
#define VALUE_SIZE_BYTE (sizeof(uint64_t))
#define KV_DESCRIPTOR_SIZE KEY_SIZE_BYTE + VALUE_SIZE_BYTE
#define HEADER_SIZE REQUEST_ID_BYTE + STATUS_CODE_BYTE


typedef enum {
  CTSYNC_OK = 20,
  CTSYNC_FAIL=40,
  CTSYNC_SERVER_ERROR=50,
 }CTSyncStatus;

CTSFrame *ctsync_deserialize(const void *data, uint64_t size);
void free_owned_frame(CTSFrame *cts_frame);
void free_req_frame(CTSFrame *cts_frame);
int add_header(KVList *header_list, KV *header);

typedef enum {
    SERVER = 0,
    CLIENT = 1
} ResOrigin;

void *ctsync_serialize(CTSFrame *cts_frame,uint64_t *size, ResOrigin res_origin);


char *get_string_value_by_string_key(CTSFrame *cts_frame, const char *r_key);
void *get_byte_value_by_string_key(CTSFrame *cts_frame, const char *r_key,
                                   uint64_t *value_size);


int add_kv(CTSFrame *res_frame, uint8_t key_size, void *key,
           uint64_t value_size, void *value);

int server_send(CTSFrame *frame, const char *conn_id);
int server_respond(CTSFrame *frame, const char *conn_id);
CTSFrame *create_cts_frame();

