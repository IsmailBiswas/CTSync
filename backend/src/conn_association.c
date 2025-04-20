#include "conn_association.h"
#include "cequiq.h"
#include "ctsync.h"
#include "ctsync_log.h"
#include <openssl/crypto.h>
#include <stdlib.h>
#include <string.h>
#include <uthash.h>

int add_conn_to_dev_map() {
  ConnInfo *entry = malloc(sizeof(ConnInfo));
  if (!entry) {
    return -1;
  }
  char *conn_id = cq_get_connection_id();
  if (!conn_id) {
    free(entry);
    return -1;
  }

  // TODO: check if key already exists, if so just return

  entry->conn_id = conn_id; // needs freeing
  entry->dev_id = NULL;     // needs freeing
  entry->auth = AUTH_STATUS_UNAUTHENTICATED;
  entry->svr_access = ACCESS_DENIED;

  HASH_ADD_STR(*ctsync_state->conn_info, conn_id, entry);
  return 0;
}

// local function. this the dev_id to conn_id finding hashmap setting function,
// there is no other data inside the map.
int add_dev_to_conn_map(char *dev_id) {
  DevToConn *entry = malloc(sizeof(DevToConn));
  if (!entry) {
    return -1;
  }

  char *conn_id = cq_get_connection_id();
  if (!conn_id) {
    free(entry);
    return -1;
  }
  entry->conn_id = conn_id;
  entry->dev_id = strdup(dev_id);
  if (!entry->dev_id) {
    free(entry);
    free(conn_id);
    return -1;
  }

  HASH_ADD_STR(*ctsync_state->dev_to_conn_id, dev_id, entry);
  return 0;
}

// returns heap-allocated device_id of current connection
char *get_dev_id() {
  char *conn_id = cq_get_connection_id();
  if (!conn_id) {
    return NULL;
  }

  ConnInfo *entry = NULL;
  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);
  if (!entry)
    return NULL;

  char *dev_id = strdup(entry->dev_id);

  if (!dev_id) {
    return NULL;
  }

  return dev_id;
}

// sets device_id for the current connection
int set_dev_id(char *dev_id) {
  if (add_dev_to_conn_map(dev_id) == -1)
    return -1;

  ConnInfo *entry = NULL;

  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return -1;

  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);

  if (!entry)
    return -1;

  entry->dev_id = strdup(dev_id);
  if (!entry->dev_id)
    return -1;

  return 0;
}

// freeies connection related heap memory
char *connection_close_free() {
  // find the ConnInfo object for current connection
  ConnInfo *conn_info_entry = NULL;

  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return NULL;

  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, conn_info_entry);
  free(conn_id);

  if (!conn_info_entry)
    return NULL;

  char *device_id = NULL;
  // check if disconnecting device is a logged in device, i.e has device_id
  if (conn_info_entry->dev_id) {
    DevToConn *dev_to_conn_entry = NULL;
    HASH_FIND_STR(*ctsync_state->dev_to_conn_id, conn_info_entry->dev_id,
                  dev_to_conn_entry);

    if (!dev_to_conn_entry) {
      return NULL;
    }
    device_id = strdup(conn_info_entry->dev_id);
    // remove dev_to_conn_id entry from hashmap
    HASH_DEL(*ctsync_state->dev_to_conn_id, dev_to_conn_entry);
    free(dev_to_conn_entry->conn_id);
    free(dev_to_conn_entry->dev_id);
    free(dev_to_conn_entry);
  }

  // remove conn_info entry from hashmap
  HASH_DEL(*ctsync_state->conn_info, conn_info_entry);
  free(conn_info_entry->conn_id);
  free(conn_info_entry->dev_id);
  free(conn_info_entry);
  return device_id;
}

// sets current connectio's server access permission
int set_server_access(access_t access) {
  // this function must be called only after calling `add_conn_to_dev_map`
  ConnInfo *entry = NULL;

  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return -1;

  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);

  if (!entry)
    return -1;

  entry->svr_access = access;
  return 0;
}

// returns if the current connection has permission to access the server
// so, the thing is, anyone can connect to the server at the TLS level, but to
// access any protected APIs, they must first verify their access
access_t has_server_access() {
  ConnInfo *entry = NULL;

  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return -1;

  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);

  if (!entry) {
    CTLOG(info, "din't find item in hashmap...");
    return -1;
  }

  CTLOG(info,
        "found object for this connection to check if it has server access");

  CTLOG(info, "server access value of this connection %d", entry->svr_access);

  return entry->svr_access;
}

// returns if the current connection is logged into an account. yes, there are
// two levels of access permissions: i. server access ii. account login
auth_status_t is_authenticated() {
  ConnInfo *entry = NULL;
  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return -1;
  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);
  if (!entry) {
    // bellow function must be called only once per connection
    add_conn_to_dev_map();
    return AUTH_STATUS_UNAUTHENTICATED;
  }
  return entry->auth;
}

int set_authentication(auth_status_t auth_state) {
  ConnInfo *entry = NULL;

  char *conn_id = cq_get_connection_id();
  if (!conn_id)
    return -1;

  HASH_FIND_STR(*ctsync_state->conn_info, conn_id, entry);
  free(conn_id);
  if (!entry) {
    return -1;
  }
  entry->auth = auth_state;
  return 0;
}

// returns connection id in a heap memory, caller must free the returned value
char *get_conn_id_by_dev_id(const char *dev_id) {
  DevToConn *entry = NULL;
  HASH_FIND_STR(*ctsync_state->dev_to_conn_id, dev_id, entry);
  if (!entry) {
    return NULL;
  }

  char *conn_id = strdup(entry->conn_id);
  if (!conn_id) {
    return NULL;
  }

  return conn_id;
}

// // TODO, I am pretty sure I am doing it wrong
// int append_conn(OnlineConnIdList *conn_list, char *new_conn_id) {
//   char **new_dev_list = realloc(conn_list->conn_id, conn_list->count + 1);
//   if (!new_dev_list) {
//     return -1;
//   }
//   new_dev_list[conn_list->count] = *new_dev_list;

//   conn_list->conn_id = new_dev_list;
//   conn_list->count += 1;
//   return 0;
// }

// OnlineConnIdList *get_online_devs_conn_id(GroupDeviceList *dev_list) {
//   OnlineConnIdList *conn_list = malloc(sizeof(OnlineConnIdList));
//   if (!conn_list)
//     return NULL;

//   for (int i = 0; i < dev_list->count; i++) {
//     char *conn_id = get_conn_id_by_dev_id(dev_list->device_id_list[i]);
//     if (conn_id) {
//       append_conn(conn_list, conn_id);
//     }
//   }

//   if (conn_list->count == 0) {
//     free(conn_list);
//     conn_list = NULL;
//   }

//   return conn_list;
// }
