#pragma once
// 1. connection_id to device_id hashmap
#include <stdbool.h>
#include <uthash.h>


typedef enum {
  AUTH_STATUS_UNAUTHENTICATED = 0,
  AUTH_STATUS_AUTHENTICATED = 1
} auth_status_t;

typedef enum {
  ACCESS_DENIED = 0,
  ACCESS_GRANTED = 1
} access_t;

typedef struct {
  char *conn_id; // connection identifier
  char *dev_id;  // device identifier, this is the id application uses to
                 // identify which device it is
  access_t svr_access;
  auth_status_t  auth;
  UT_hash_handle hh;
} ConnInfo;

// 2. device_id to ConnInfo hashmap
typedef struct {
  char *dev_id;
  char *conn_id;
  UT_hash_handle hh;
} DevToConn;

typedef struct {
  char **device_id_list;
  char **device_name_list;
  char **sign_public_key;
  char **public_key;
  bool *device_online_list;
  int count;
} GroupDeviceList;

// typedef struct{
//   char **conn_id;
//   int count;
// }OnlineConnIdList;

//returns a boolean value concidering everything to indicate if connection is authenticated.
auth_status_t is_authenticated();
int set_server_access(access_t access);
int add_conn_to_dev_map();
access_t has_server_access();

int set_authentication(auth_status_t auth_state);
int set_dev_id(char *dev_id);
char *get_dev_id();

char *get_conn_id_by_dev_id(const char *dev_id);
char* connection_close_free();


// OnlineConnIdList *get_online_devs_conn_id(GroupDeviceList *dev_list);

