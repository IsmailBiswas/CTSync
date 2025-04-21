#include "ctsync.h"
#include "actions.h"
#include "cequiq.h"
#include "conn_association.h"
#include "cts_frame.h"
#include "ctsync_log.h"
#include "database.h"
#include "parser.h"
#include <errno.h>
#include <openssl/crypto.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <arpa/inet.h>
// static SSLToDevice ssl_to_device_list[FD_SETSIZE] = {{NULL, NULL}};

void pre_authz_action_processor(CTSFrame *req_frame, CTSFrame *res_frame) {
  CTLOG(debug, "pre_authz action processor");

  char *action = get_string_value_by_string_key(req_frame, ACTION_KEY);

  if (!action) {
    char *error_key = "error";
    char *error_msg = "Missing required key: 'action'.";
    int ret = add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                     error_msg);
    if (ret == -1) {
      res_frame->status_code = CTSYNC_SERVER_ERROR;
      return;
    }

    res_frame->status_code = CTSYNC_FAIL;
  }

  // ping action, server should respond with CTSYNC_OK status
  if (strcmp(action, "PING") == 0) {
    ping_action(req_frame, res_frame);

    //  client verifies it's access to this server
  } else if (strcmp(action, "ACCESS_SERVER") == 0) {
    server_access_verify_action(req_frame, res_frame);

    // create new client account
  } else if (strcmp(action, "REGISTER_DEVICE") == 0) {
    // check client is allowed to access this server
    if (has_server_access() == ACCESS_GRANTED) {
      register_device_action(req_frame, res_frame);
      // send error if client has not verified they have access to this server
    } else {
      char *error_key = "error";
      char *error_msg = "Access denied. Send an 'ACCESS_SERVER' request first "
                        "to verify server access.";
      (void)add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                   error_msg);
      res_frame->status_code = CTSYNC_FAIL;
    }

    // login clinet
  } else if (strcmp(action, "LOGIN") == 0) {
    login_device_action(req_frame, res_frame);

    // unrecoginezed request
  } else {
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(warning, "Unrecoginezed action for unauthenticated connection: %s",
          action);
  }

  free(action);
};

void authenticated_data_process(CTSFrame *cts_frame, CTSFrame *res_frame) {
  CTLOG(debug, "authz action processor");

  char *action = get_string_value_by_string_key(cts_frame, ACTION_KEY);
  if (!action) {
    char *error_key = "error";
    char *error_msg = "Missing required key: 'action'.";
    int ret = add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                     error_msg);
    if (ret == -1) {
      res_frame->status_code = CTSYNC_SERVER_ERROR;
      return;
    }

    res_frame->status_code = CTSYNC_FAIL;
  }

  if (strcmp(action, "ping") == 0) {
    ping_action(res_frame, res_frame);
  } else if (strcmp(action, "CREATE_GROUP") == 0) {
    create_group_action(cts_frame, res_frame);
  } else if (strcmp(action, "GROUP_CHECK") == 0) {
    group_check_action(cts_frame, res_frame);
  } else if (strcmp(action, "STORE_INVITE_KEY") == 0) {
    store_invite_key_action(cts_frame, res_frame);
  } else if (strcmp(action, "JOIN_GROUP") == 0) {
    join_group_action(cts_frame, res_frame);
  } else if (strcmp(action, "ACCEPT_DEVICE") == 0) {
    accept_device_action(cts_frame, res_frame);
  } else if (strcmp(action, "PULL") == 0) {
    pull_action(cts_frame, res_frame);
  } else if (strcmp(action, "CLIPBOARD") == 0) {
    send_clipboard_action(cts_frame, res_frame);
  } else if (strcmp(action, "ONLINE_DEVICES") == 0) {
    online_devices_action(cts_frame, res_frame);
  } else if (strcmp(action, "GET_INVITE_KEY") == 0) {
    get_invite_key_action(cts_frame, res_frame);
  } else if (strcmp(action, "KICK_DEVICE") == 0) {
    kick_device_action(cts_frame, res_frame);
  } else {
    char *error_key = "error";
    char *error_msg = "unrecognised action";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(warning, "unrecoginezed action for authenticated connection: %s",
          action);
  }
  free(action);
}

int get_port() {
  char *port_env = getenv("CTSYNC_PORT");
  if (port_env) {
    return atoi(port_env);
  }
  return 4343; // Default port
}

void print_u32_be_and_next_two_bytes(const void *buffer) {
  uint32_t network_order_value;
  uint8_t byte_at_offset_4;
  uint8_t byte_at_offset_5; // Variable for the byte at offset 5

  // Ensure the buffer pointer is treated as pointing to bytes
  uint8_t *byte_ptr = (uint8_t *)buffer;

  // --- Read and print uint32_t (bytes 0-3) ---
  memcpy(&network_order_value, byte_ptr, sizeof(uint32_t));
  uint32_t host_order_value = ntohl(network_order_value);
  printf("Value (uint32_t @ offset 0): %u\n", host_order_value);

  // --- Read and print uint8_t (byte 4) ---
  byte_at_offset_4 = *(byte_ptr + 4); // Access byte at offset 4
  printf("Value (uint8_t @ offset 4) : %u (0x%02X)\n", byte_at_offset_4,
         byte_at_offset_4);

  // --- Read and print uint8_t (byte 5) ---
  byte_at_offset_5 = *(byte_ptr + 5); // Access byte at offset 5
  printf("Value (uint8_t @ offset 5) : %u (0x%02X)\n", byte_at_offset_5,
         byte_at_offset_5);
}

void send_response(CTSFrame *res_frame) {
  char *conn_id = cq_get_connection_id();
  server_respond(res_frame, conn_id);
  free(conn_id);
  free_owned_frame(res_frame);
}

void ctsync_entry(void const *data, uint64_t size) {
  CTSFrame *cts_frame = ctsync_deserialize(data, size);
  if (!cts_frame) {
    CTLOG(error,
          "failed to deserialize request data, probably malformed data "
          "received %s",
          strerror(errno));
    return;
  }

  CTLOG(info, "number of kv %lu", cts_frame->kv_list->count);
  CTLOG(debug, "before deserialized frame response index %d",
        cts_frame->request_id);
  CTLOG(info, "deserialized frame status code  %d", cts_frame->status_code);

  // this is the response frame
  CTSFrame *res_frame = create_cts_frame();
  if (!res_frame) {
    CTLOG(error, "failed to create response frame");
    return;
  }
  res_frame->request_id = cts_frame->request_id;

  char *action = get_string_value_by_string_key(cts_frame, ACTION_KEY);
  if (!action) {
    CTLOG(error, "NO ACTION FOUND");
    return;
  }
  CTLOG(info, "RECEIVED ACTION: %s", action);
  free(action);

  if (is_authenticated() == AUTH_STATUS_AUTHENTICATED) {
    CTLOG(debug, "connection authenticated");
    authenticated_data_process(cts_frame, res_frame);
  } else {
    CTLOG(debug, "connection NOT authenticated");
    pre_authz_action_processor(cts_frame, res_frame);
  }

  // send response, freeies res_frame
  CTLOG(debug, "sending status code %d", res_frame->status_code);
  send_response(res_frame);

  free_req_frame(cts_frame);
}

void client_disconnect(char const *data) {
  char *device_id = connection_close_free();
  send_group_devices(device_id);
  free(device_id);
}

CTSyncState *ctsync_state = NULL;

int create_ctsync_state() {
  ctsync_state = malloc(sizeof(CequiqConfig));
  if (!ctsync_state) {
    return -1;
  }

  // ConnInfo stores server access permission and authentication status of every
  // connection
  ConnInfo **conn_info = malloc(sizeof(ConnInfo *));
  if (!conn_info) {
    free(ctsync_state);
    return -1;
  }
  *conn_info = NULL;

  // DevToConn allows retriving connection id uisng device id
  DevToConn **dev_to_conn = malloc(sizeof(DevToConn *));
  if (!dev_to_conn) {
    free(ctsync_state);
    free(conn_info);
    return -1;
  }
  *dev_to_conn = NULL;

  ctsync_state->dev_to_conn_id = dev_to_conn;
  ctsync_state->conn_info = conn_info;
  ctsync_state->res_id =
      2; // starting response id, TODO: should use local id for each connection
  return 0;
}

int main(int argc, char *argv[]) {

  if (create_ctsync_state() < 0) {
    return EXIT_FAILURE;
  }

  if (db_migrate() != 0) {
    fprintf(stderr, "there was problem migrating the database\n");
    return EXIT_FAILURE;
  }

  CequiqConfig *cequiq_config = Cequiq_init();
  if (!cequiq_config) {
    return EXIT_FAILURE;
  }

  cequiq_config->data_callback = ctsync_entry;
  cequiq_config->close_callback = client_disconnect;
  cequiq_config->port_number = get_port();
  cequiq_start(cequiq_config);

  free(cequiq_config);

  return EXIT_FAILURE;
}
