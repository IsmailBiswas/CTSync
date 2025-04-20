#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

#define IS_AUTHTHENTICATED (1 << 0) // 0b00000001
#define HAS_SRV_ACCESS (1 << 1)     // 0b00000010
#define HEAD_BODY_DELIMITER "\r\n\r\n"
#define DELIMITER "\r\n"
#define KEY_VALUE_DELIMITER ":"
#define ENDING_SEQUENCE "\r\r\n"

typedef struct {
  void *key;
  uint8_t key_size;
  void *value;
  uint64_t value_size;
} KV;

typedef struct {
  KV **kv;
  uint64_t count;
} KVList;

typedef struct {
  uint32_t request_id;
  uint8_t status_code;
  KVList *kv_list;
} CTSFrame;

#endif
