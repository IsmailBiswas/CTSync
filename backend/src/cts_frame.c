#include "cts_frame.h"
#include "cequiq.h"
#include "ctsync.h"
#include "ctsync_log.h"
#include "parser.h"
#include <errno.h>
#include <netinet/in.h>
#include <openssl/crypto.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t htonll(uint64_t val); // from cequiq

// this function frees a 'deserialized' frame. it is similar to
// `free_owned_frame`, except it does not free individual keys and values,
// as they are not dynamically allocated (with malloc) during deserialization.
// instead, they are just pointers to locations in the receive buffer
void free_req_frame(CTSFrame *cts_frame) {
  if (!cts_frame)
    return;

  if (!cts_frame->kv_list) {
    free(cts_frame);
    return;
  }
  for (int i = 0; i < cts_frame->kv_list->count; i++) {
    free(cts_frame->kv_list->kv[i]);
  }
  free(cts_frame->kv_list->kv);
  free(cts_frame->kv_list);
  free(cts_frame);
}

// frees manually created frames
void free_owned_frame(CTSFrame *cts_frame) {
  if (!cts_frame)
    return;

  if (!cts_frame->kv_list) {
    free(cts_frame);
    return;
  }
  for (int i = 0; i < cts_frame->kv_list->count; i++) {
    free(cts_frame->kv_list->kv[i]->value);
    free(cts_frame->kv_list->kv[i]->key);
    free(cts_frame->kv_list->kv[i]);
  }
  free(cts_frame->kv_list->kv);
  free(cts_frame->kv_list);
  free(cts_frame);
}

static inline uint64_t ntohll(uint64_t val) {
  if (__BYTE_ORDER == __LITTLE_ENDIAN) {
    return ((uint64_t)ntohl(val & 0xFFFFFFFF) << 32) | ntohl(val >> 32);
  }
  return val;
}

KV *create_kv(uint8_t key_size, uint64_t value_size) {
  KV *kv = malloc(sizeof(KV));
  if (!kv) {
    return NULL;
  }
  kv->key_size = key_size;
  kv->value_size = value_size;

  return kv;
}

int parse_kv(const void *data, uint64_t *size, CTSFrame *cts_frame,
             uint64_t *read_byte) {

  if (KV_DESCRIPTOR_SIZE > (size - read_byte)) {
    errno = EBADMSG;
    return -1;
  }

  uint8_t key_size;
  key_size = *((uint8_t *)data + *read_byte);
  *read_byte = *read_byte + sizeof(uint8_t);

  if ((key_size + *read_byte) > *size) {
    CTLOG(error, "malformed data");
    errno = EBADMSG;
    return -1;
  }

  uint64_t value_size;
  memcpy(&value_size, (uint8_t *)data + *read_byte, sizeof(uint64_t));
  *read_byte = *read_byte + sizeof(uint64_t);
  value_size = ntohll(value_size);

  if ((value_size + *read_byte) > *size) {
    CTLOG(error, "malformed data");
    errno = EBADMSG;
    return -1;
  }

  // craete new instance of KV
  KV *kv = malloc(sizeof(KV));
  if (!kv) {
    return -1;
  }

  kv->key_size = key_size;
  kv->value_size = value_size;

  kv->key = (uint8_t *)data + *read_byte;
  *read_byte = *read_byte + key_size;

  kv->value = (uint8_t *)data + *read_byte;
  *read_byte = *read_byte + value_size;

  // expand the list(array?) by one
  KV **kv_arr = realloc(cts_frame->kv_list->kv,
                        (cts_frame->kv_list->count + 1) * sizeof(KV *));
  if (!kv_arr) {
    free(kv);
    return -1;
  }

  cts_frame->kv_list->kv = kv_arr;

  cts_frame->kv_list->kv[cts_frame->kv_list->count] = kv;
  cts_frame->kv_list->count += 1;
  return 0;
}

// parse the header section and populate the provided frame
int parse_header(const void *data, uint64_t *size, CTSFrame *cts_frame,
                 uint64_t *read_byte) {
  if (*size < HEADER_SIZE) {
    errno = EBADMSG;
    return -1;
  }
  *read_byte = HEADER_SIZE;
  uint32_t request_id;
  memcpy(&request_id, data, sizeof(uint32_t));
  CTLOG(debug, "received request index %d", ntohl(request_id));
  // convert from network byte order to hostbyte order
  cts_frame->request_id = ntohl(request_id);

  // no need to convert to host byte order as it's a sigle byte value
  cts_frame->status_code = *((uint8_t *)data + sizeof(uint32_t));

  return 0;
}

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

CTSFrame *ctsync_deserialize(const void *data, uint64_t size) {
  // check is received data size is less than header size

  CTSFrame *frame = create_cts_frame();
  if (!frame) {
    return NULL;
  }

  uint64_t read_byte = 0;
  // process the header
  if (parse_header(data, &size, frame, &read_byte) == -1) {
    free_owned_frame(frame);
    return NULL;
  };

  while (read_byte < size) {
    if (parse_kv(data, &size, frame, &read_byte) == -1) {
      free_owned_frame(frame);
      return NULL;
    };
  }

  return frame;
}

int cal_data_size(CTSFrame *cts_frame, uint64_t *size) {

  *size += HEADER_SIZE;

  if (!cts_frame->kv_list) {
    return -1;
  }

  for (uint64_t i = 0; i < cts_frame->kv_list->count; i++) {
    uint8_t key_size = cts_frame->kv_list->kv[i]->key_size;
    uint64_t value_size = cts_frame->kv_list->kv[i]->value_size;

    if (*size > UINT64_MAX - sizeof(uint8_t)) {
      errno = EOVERFLOW;
      return -1;
    }
    *size += sizeof(uint8_t);

    if (*size > UINT64_MAX - sizeof(uint64_t)) {
      errno = EOVERFLOW;
      return -1;
    }
    *size += sizeof(uint64_t);

    if (*size > UINT64_MAX - key_size) {
      errno = EOVERFLOW;
      return -1;
    }

    *size += key_size;

    if (*size > UINT64_MAX - value_size) {
      errno = EOVERFLOW;
      return -1;
    }
    *size += value_size;
  }

  return 0;
}

// caller must make sure data size is enough to store the frame data
void copy_from_frame(void *data, CTSFrame *cts_frame) {
  uint64_t copied_byte = 0;
  uint32_t res_id = htonl(cts_frame->request_id);

  memcpy(data, &res_id, sizeof(uint32_t));
  copied_byte += sizeof(uint32_t);

  memcpy(data + copied_byte, &cts_frame->status_code, sizeof(uint8_t));
  copied_byte += sizeof(uint8_t);

  if (!cts_frame->kv_list) {
    return;
  }

  for (uint64_t i = 0; i < cts_frame->kv_list->count; i++) {
    // copy key size byte
    memcpy((uint8_t *)data + copied_byte, &cts_frame->kv_list->kv[i]->key_size,
           sizeof(uint8_t));
    copied_byte += sizeof(uint8_t);

    uint64_t be_value_size = htonll(cts_frame->kv_list->kv[i]->value_size);
    // copy value size
    memcpy((uint8_t *)data + copied_byte, &be_value_size,
           sizeof(be_value_size));
    copied_byte += sizeof(uint64_t);

    // copy key data
    memcpy((uint8_t *)data + copied_byte, cts_frame->kv_list->kv[i]->key,
           cts_frame->kv_list->kv[i]->key_size);
    copied_byte += cts_frame->kv_list->kv[i]->key_size;

    // copy value data
    memcpy((uint8_t *)data + copied_byte, cts_frame->kv_list->kv[i]->value,
           cts_frame->kv_list->kv[i]->value_size);
    copied_byte += cts_frame->kv_list->kv[i]->value_size;
  }
}

/**
 * Serializes a CTSFrame into a byte stream.
 *
 * @param cts_frame   The request frame to serialize.
 * @param size        Output: size of the serialized data.
 * @param res_origin  Indicates if the request is from client or server.
 * @return            Pointer to serialized data (must be freed by caller).
 */
void *ctsync_serialize(CTSFrame *cts_frame, uint64_t *size,
                       ResOrigin res_origin) {
  uint32_t res_id;
  if (res_origin == SERVER) {
    // TODO: response id is getting shared accross connections,like there is
    // only one global id counter; it would be better to have local id counter
    // per connection
    res_id = ctsync_state->res_id + 2;
    ctsync_state->res_id = res_id;
    cts_frame->request_id = ctsync_state->res_id;
    cts_frame->status_code = CTSYNC_OK;
  }

  *size = 0;
  if (cal_data_size(cts_frame, size) == -1) {
    return NULL;
  }

  // TODO: should I set a max size?
  void *data = malloc(*size);
  if (!data) {
    return NULL;
  }

  copy_from_frame(data, cts_frame);

  return data;
}

void *get_byte_value_by_string_key(CTSFrame *cts_frame, const char *r_key,
                                   uint64_t *value_size) {

  size_t r_key_size = strlen(r_key);
  if (cts_frame->kv_list->count == 0) {
    CTLOG(error, "frame doesn't contain any key value");
    return NULL;
  }

  for (int i = 0; i < cts_frame->kv_list->count; i++) {
    void *key = cts_frame->kv_list->kv[i]->key;

    // if request key size isn't same as current key then continue
    if (r_key_size != cts_frame->kv_list->kv[i]->key_size) {
      continue;
    }

    // compare the key
    if (memcmp(key, r_key, r_key_size) == 0) {
      // copy the key

      *value_size = cts_frame->kv_list->kv[i]->value_size;
      char *value = malloc(*value_size);
      if (!value) {
        return NULL;
      }

      memcpy(value, cts_frame->kv_list->kv[i]->value, *value_size);
      return value;
    }
  }

  CTLOG(debug, "key: '%s' not found", r_key);
  return NULL;
}

/*
caller must free the returned char, as it returns NULL on failuer you can
safely(I think) call free on ruturned value
*/
char *get_string_value_by_string_key(CTSFrame *cts_frame, const char *r_key) {

  size_t r_key_size = strlen(r_key);
  if (cts_frame->kv_list->count == 0) {
    CTLOG(error, "frame doesn't contain any key value");
    return NULL;
  }

  for (int i = 0; i < cts_frame->kv_list->count; i++) {
    void *key = cts_frame->kv_list->kv[i]->key;

    // if request key size isn't same as current key then continue
    if (r_key_size != cts_frame->kv_list->kv[i]->key_size) {
      continue;
    }

    // compare the key
    if (memcmp(key, r_key, r_key_size) == 0) {
      // copy the key

      uint64_t value_size = cts_frame->kv_list->kv[i]->value_size;
      char *value = malloc(value_size + 1);
      if (!value) {
        return NULL;
      }

      memcpy(value, cts_frame->kv_list->kv[i]->value, value_size);
      value[value_size] = '\0';
      return value;
    }
  }

  CTLOG(debug, "key: '%s' not found", r_key);
  return NULL;
}

int add_kv(CTSFrame *res_frame, uint8_t key_size, void *key,
           uint64_t value_size, void *value) {

  KV *new_kv = malloc(sizeof(KV));
  if (!new_kv)
    return -1;

  char *key_copy;
  if (!(key_copy = malloc(key_size))) {
    return -1;
  }
  memcpy(key_copy, key, key_size);

  char *value_copy;
  if (!(value_copy = malloc(value_size))) {
    return -1;
  }
  memcpy(value_copy, value, value_size);

  new_kv->key_size = key_size;
  new_kv->key = key_copy;
  new_kv->value_size = value_size;
  new_kv->value = value_copy;

  // CTLOG(debug, "add_kv: from new_kv: %.*s", (int)value_size,
  //       (char *)new_kv->value);

  KV **kv_arr = realloc(res_frame->kv_list->kv,
                        (res_frame->kv_list->count + 1) * sizeof(KV *));

  if (!kv_arr) {
    free(new_kv);
    return -1;
  }

  res_frame->kv_list->kv = kv_arr;
  res_frame->kv_list->kv[res_frame->kv_list->count] = new_kv;
  res_frame->kv_list->count += 1;

  return 0;
};

// for sending data to client when clinet didn't make request, things like send
// clipboard data
int server_send(CTSFrame *frame, const char *conn_id) {
  uint64_t size;
  void *data = ctsync_serialize(frame, &size, SERVER);
  if (!data) {
    CTLOG(debug, "serialized data is NULL");
    return -1;
  }

  char *action = get_string_value_by_string_key(frame, "action");
  CTLOG(debug, "Server originated response: sending %lu bytes to client", size);
  CTLOG(debug, "Server originated response-> action: %s", action);
  free(action);

  int ret = cequiq_write(conn_id, data, size);
  free(data);
  return ret;
}

// for sending response data of a request
int server_respond(CTSFrame *frame, const char *conn_id) {
  uint64_t size;
  void *data = ctsync_serialize(frame, &size, CLIENT);
  if (!data) {
    CTLOG(debug, "serialized data is NULL");
    return -1;
  }

  CTLOG(debug, "status code %d", frame->status_code);
  CTLOG(debug, "nunber of kv %lu", frame->kv_list->count);
  CTLOG(debug, "sending %lu bytes to client", size);

  int ret = cequiq_write(conn_id, data, size);
  free(data);
  return ret;
}

CTSFrame *create_cts_frame() {
  CTSFrame *frame = malloc(sizeof(CTSFrame));
  if (!frame) {
    return NULL;
  }
  frame->kv_list = NULL;
  frame->request_id = 0;
  frame->status_code = 0;

  KVList *kv_list = malloc(sizeof(KVList));
  if (!kv_list) {
    free(frame);
    return NULL;
  }
  kv_list->count = 0;
  kv_list->kv = NULL;
  frame->kv_list = kv_list;
  return frame;
}
