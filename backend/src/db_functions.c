#include "db_functions.h"
#include "conn_association.h"
#include "cts_frame.h"
#include "ctsync.h"
#include "ctsync_log.h"
#include <openssl/crypto.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

const char *device_state_to_string(DeviceState state) {
  switch (state) {
  case DEVICE_STATE_PENDING:
    return "Pending";
  case DEVICE_STATE_ACCEPTED:
    return "Accepted";
  case DEVICE_STATE_REJECTED:
    return "Rejected";
  case DEVICE_STATE_KICKED:
    return "Kicked";
  case DEVICE_STATE_NEW:
    return "New";
  case DEVICE_STATE_LEFT:
    return "Left";
  default:
    return "Unknown";
  }
}

DeviceState string_to_device_state(const char *state_str) {
  if (strcmp(state_str, "Accepted") == 0)
    return DEVICE_STATE_ACCEPTED;
  if (strcmp(state_str, "Rejected") == 0)
    return DEVICE_STATE_REJECTED;
  if (strcmp(state_str, "Kicked") == 0)
    return DEVICE_STATE_KICKED;
  if (strcmp(state_str, "New") == 0)
    return DEVICE_STATE_NEW;
  return DEVICE_STATE_PENDING;
}

int db_is_in_group(const char *device_id) {
  sqlite3 *db;
  int rc;
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // SQL statement to update the group_id of a specific device
  char *sql = "SELECT connection_state FROM devices WHERE device_id = ?;";
  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // Bind the group_id and device_id parameters
  sqlite3_bind_text(stmt, 1, device_id, -1, SQLITE_TRANSIENT);

  // Execute the statement
  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    const unsigned char *connection_state = sqlite3_column_text(stmt, 0);
    CTLOG(debug, "connection state: %s", connection_state);

    DeviceState state = string_to_device_state((const char *)connection_state);
    if (state == DEVICE_STATE_ACCEPTED) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return 1;
    }
  } else {
    CTLOG(debug, "device_id not found");
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return -1;
}

// If device can create group returns 0 else returns 1, on error return -1
int can_create_group(const char *device_id) {
  sqlite3 *db;
  int rc;
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // SQL statement to update the group_id of a specific device
  char *sql = "SELECT connection_state FROM devices WHERE device_id = ?;";
  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // Bind the group_id and device_id parameters
  sqlite3_bind_text(stmt, 1, device_id, -1, SQLITE_TRANSIENT);

  // Execute the statement
  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    const unsigned char *connection_state = sqlite3_column_text(stmt, 0);
    CTLOG(debug, "connection state: %s", connection_state);

    DeviceState state = string_to_device_state((const char *)connection_state);
    if (state != DEVICE_STATE_ACCEPTED) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return 0;
    }
  } else {
    CTLOG(debug, "device_id not found");
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return -1;
}

int db_create_new_group(char *group_id_str, size_t size) {
  sqlite3 *db;
  int rc;
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  if (size < UUID_STR_LEN) {
    CTLOG(error, "group_id buffer size too small %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  };

  uuid_t group_id;
  uuid_generate_random(group_id);
  uuid_unparse(group_id, group_id_str);

  char *sql = "INSERT INTO groups ( group_id, "
              "last_seen ) "
              "VALUES (?,CURRENT_TIMESTAMP);";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_bind_text(stmt, 1, group_id_str, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);

  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);

  rc = rc == SQLITE_DONE ? SQLITE_OK : -1;

  // Close the database connection
  sqlite3_close(db);

  return rc;
}

int db_get_group_pk(sqlite3 *db, const char *group_id) {
  // Retrieve the primary key of the group
  int rc;
  sqlite3_stmt *stmt;
  int group_pk = -1;
  const char *sql_get_group = "SELECT id FROM groups WHERE group_id = ?;";

  rc = sqlite3_prepare_v2(db, sql_get_group, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare select statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_bind_text(stmt, 1, group_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    group_pk = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);

  if (group_pk == -1) {
    CTLOG(debug, "Group not found");
    return -1;
  }

  return group_pk;
}

int db_update_device_group(const char *device_id, const char *group_id,
                           const int group_pk, DeviceState con_state) {

  if (!device_id) {
    CTLOG(error, "device_id is required to update a device group");
    return -1;
  }

  sqlite3 *db;
  int rc;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  int pk = -1;
  if (!group_id) {
    pk = group_pk;
  } else {
    pk = db_get_group_pk(db, group_id);
    if (pk == -1) {
      sqlite3_close(db);
      return -1;
    }
  }

  // SQL statement to update the group_id of a specific device
  char *sql = "UPDATE devices SET group_id = ?, connection_state = ? WHERE "
              "device_id = ?;";

  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // Bind the group_id and device_id parameters
  const char *state = device_state_to_string(con_state);
  sqlite3_bind_int(stmt, 1, pk);
  sqlite3_bind_text(stmt, 2, state, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, device_id, -1, SQLITE_TRANSIENT);

  // Execute the statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return SQLITE_OK;
}

void db_get_invite_key(char **is_valid, char **creation_time, char **exp,
                       char **group_id, const char *invite_Key) {
  if (!invite_Key) {
    return;
  }

  sqlite3 *db;
  int rc;
  *is_valid = NULL;
  *exp = NULL;
  *creation_time = NULL;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }
  char *sql = "SELECT is_valid, creation_time, exp, group_id FROM invite_keys "
              "WHERE key = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, invite_Key, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return;
  }

  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    *is_valid = strdup((char *)sqlite3_column_text(stmt, 0));
    *creation_time = strdup((char *)sqlite3_column_text(stmt, 1));
    *exp = strdup((char *)sqlite3_column_text(stmt, 2));
    *group_id = strdup((char *)sqlite3_column_text(stmt, 3));

    // Check if any strdup failed
    if (!*is_valid || !*creation_time || !*exp || !*group_id) {
      free(*is_valid);
      free(*creation_time);
      free(*exp);
      free(*group_id);
      *is_valid = *creation_time = *exp = *group_id = NULL;
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return;
    }

  } else if (rc != SQLITE_DONE) {
    CTLOG(debug, "no rows found for invite_key: %s", invite_Key);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void free_group_device_list(GroupDeviceList *list) {
  if (!list)
    return;

  for (int i = 0; i < list->count; i++) {
    free(list->device_id_list[i]);
    free(list->device_name_list[i]);
    free(list->public_key[i]);
    free(list->sign_public_key[i]);
  }

  free(list->device_id_list);
  free(list->device_name_list);
  free(list->public_key);
  free(list->sign_public_key);
  free(list->device_online_list);

  list->count = 0;
  free(list);
}

GroupDeviceList *db_get_devices_by_group(const char *group_id,
                                         DeviceState con_state) {
  sqlite3 *db;
  int rc;

  GroupDeviceList *result = malloc(sizeof(GroupDeviceList));
  if (!result) {
    return NULL;
  }
  result->device_id_list = NULL;
  result->device_name_list = NULL;
  result->device_online_list = NULL;
  result->public_key = NULL;
  result->sign_public_key = NULL;
  result->count = 0;
  if (!group_id) {
    free(result);
    return NULL;
  }

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(fatal, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    free(result);
    return NULL;
  }

  const char *sql = "SELECT device_id, device_name, pub_key, sign_pub_key FROM "
                    "devices WHERE group_id = ? AND "
                    "connection_state = ?;";
  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare SQL statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return result;
  }
  const char *s_con_state = device_state_to_string(con_state);

  sqlite3_bind_text(stmt, 1, group_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, s_con_state, -1, SQLITE_TRANSIENT);

  // first, count the number of rows
  int row_count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    row_count++;
  }

  sqlite3_reset(stmt);
  if (row_count > 0) {
    // allocate memory for the device
    result->public_key = calloc(row_count, sizeof(char *));
    result->device_id_list = calloc(row_count, sizeof(char *));
    result->sign_public_key = calloc(row_count, sizeof(char *));
    result->device_name_list = calloc(row_count, sizeof(char *));
    result->device_online_list = calloc(row_count, sizeof(bool));
    result->count = row_count;

    // read the actual data
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *device_id = sqlite3_column_text(stmt, 0);
      const unsigned char *device_name = sqlite3_column_text(stmt, 1);
      const unsigned char *public_key = sqlite3_column_text(stmt, 2);
      const unsigned char *sign_public_key = sqlite3_column_text(stmt, 3);
      char *conn_id = get_conn_id_by_dev_id((const char *)device_id);
      if (conn_id) {
        result->device_online_list[i] = true;
      }
      free(conn_id);
      result->device_id_list[i] = strdup((const char *)device_id);
      result->device_name_list[i] = strdup((const char *)device_name);
      result->public_key[i] = strdup((const char *)public_key);
      result->sign_public_key[i] = strdup((const char *)sign_public_key);
      if (!result->device_id_list[i] || !result->device_id_list[i]) {
        result->count = i + 1;
        free_group_device_list(result);
      }
      result->count = ++i;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return result;
}

void db_fetch_column_data(char **result_data, const char *table_name,
                          const char *search_column, const char *search_value,
                          const char *target_column) {
  sqlite3 *db;
  int rc;
  int sql_query_buf_size = 2048;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(fatal, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }

  char sql[sql_query_buf_size];
  int written = snprintf(sql, sizeof(sql), "SELECT %s FROM %s WHERE %s = ?;",
                         target_column, table_name, search_column);

  if (written < 0 || (size_t)written >= sizeof(sql)) {
    CTLOG(error, "SQL query too long");
    sqlite3_close(db);
    *result_data = NULL;
    return;
  }

  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare SQL statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    *result_data = NULL;
    return;
  }

  sqlite3_bind_text(stmt, 1, search_value, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const unsigned char *db_data = sqlite3_column_text(stmt, 0);
    if (db_data) {
      *result_data = strdup((const char *)db_data); // copy data
      if (!result_data)
        *result_data = NULL;
    } else {
      *result_data = NULL; // Set to NULL if no data found
    }
  } else if (rc == SQLITE_DONE) {
    *result_data = NULL; // No matching rows found, return NULL
  } else {
    CTLOG(error, "failed to execute SQL statement: %s", sqlite3_errmsg(db));
    *result_data = NULL;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

int db_update_connection_state(const char *device_id, DeviceState con_state) {
  if (!device_id || !con_state) {
    return -1;
  }
  sqlite3 *db;
  int rc;

  // Open the database
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // SQL statement to update the group_id of a specific device
  char *sql = "UPDATE devices SET connection_state = ? WHERE "
              "device_id = ?;";
  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // Bind the group_id and device_id parameters
  const char *state = device_state_to_string(con_state);
  sqlite3_bind_text(stmt, 1, state, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, device_id, -1, SQLITE_TRANSIENT);

  // Execute the statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return -1;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return SQLITE_OK;
}

// returns only online devices who wants to receive data
data_interested_dev_list *db_get_interested_devices(const char *group_id) {

  sqlite3 *db;
  int rc;

  data_interested_dev_list *result = malloc(sizeof(data_interested_dev_list));
  result->device_id_list = NULL;
  result->count = 0;
  if (!group_id) {
    return NULL;
  }

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    free(result);
    return NULL;
  }

  const char *sql =
      "SELECT device_id, pub_key FROM devices WHERE group_id = ? AND "
      "connection_state = ? and receive_flag = ?;";

  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    free(result);
    return NULL;
  }

  const char *connnection_state = device_state_to_string(DEVICE_STATE_ACCEPTED);

  sqlite3_bind_text(stmt, 1, group_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, connnection_state, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, "1", -1, SQLITE_TRANSIENT);

  // First, count the number of rows
  int row_count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    row_count++;
  }

  sqlite3_reset(stmt);
  if (row_count > 0) {
    // allocate memory for the devices
    result->device_id_list = calloc(row_count, sizeof(char *));
    if (!result->device_id_list) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      free(result);
      return NULL;
    }
    result->p_enc_key_list = calloc(row_count, sizeof(char *));
    if (!result->p_enc_key_list) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      free(result);
      return NULL;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *device_id = sqlite3_column_text(stmt, 0);
      const unsigned char *p_enc_key = sqlite3_column_text(stmt, 1);
      // check if dev online;
      char *conn_id = get_conn_id_by_dev_id((const char *)device_id);
      if (!conn_id) {
        continue;
      }
      result->device_id_list[i] = strdup((const char *)device_id);
      result->p_enc_key_list[i] = strdup((const char *)p_enc_key);

      if (!result->device_id_list[i] || !result->p_enc_key_list[i]) {
        result->count = i + 1;
        free_data_interested_dev_list(result);
        break;
      }
      result->count = ++i;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return result;
}

void free_data_interested_dev_list(data_interested_dev_list *list) {
  if (!list)
    return;

  for (int i = 0; i < list->count; i++) {
    free(list->device_id_list[i]);
    free(list->p_enc_key_list[i]);
  }
  list->count = 0;
  free(list);
}

int db_delete_invite_key(const char *device_id) {
  sqlite3 *db;
  int rc;

  // Open the database
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  // SQL statement to update the group_id of a specific device
  char *sql = "DELETE FROM invite_keys WHERE creator = ?;";
  sqlite3_stmt *stmt;

  // Prepare the SQL statement
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  sqlite3_bind_text(stmt, 1, device_id, -1, SQLITE_TRANSIENT);

  // Execute the statement
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return -1;
  }

  // finalize the update statement
  sqlite3_finalize(stmt);

  // close the database connection
  sqlite3_close(db);

  return SQLITE_OK;
}

int db_register_device(char *device_name, char *access_key_hash,
                       char *device_id, char *sign_pub_key, char *pub_key) {
  sqlite3 *db;
  int rc;
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  char *sql = "INSERT INTO devices (device_name, device_id, access_key_hash, "
              "sign_pub_key, pub_key,"
              "connection_state,"
              "receive_flag,"
              "last_seen ) "
              "VALUES (?,?,?,?,?,?,?,CURRENT_TIMESTAMP);";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, device_name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, device_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, access_key_hash, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, sign_pub_key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, pub_key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, device_state_to_string(DEVICE_STATE_NEW), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, "1", -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);

  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);

  rc = rc == SQLITE_DONE ? SQLITE_OK : rc;

  // close the database connection
  sqlite3_close(db);

  return rc;
}

void db_get_hash_salt(char **data, const char *device_id) {
  if (!device_id) {
    return;
  }
  sqlite3 *db;
  int rc;
  *data = NULL;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    if (db)
      sqlite3_close(db);
    return;
  }
  char *sql = "SELECT access_key_hash FROM devices WHERE device_id = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, device_id, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to bind parameter: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return;
  }

  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    const unsigned char *access_key_hash = sqlite3_column_text(stmt, 0);
    if (access_key_hash) {
      *data = strdup((const char *)access_key_hash);
      if (!*data) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return;
      }
    }
  } else if (rc == SQLITE_DONE) {
    CTLOG(debug, "no rows found for device_id: %s", device_id);
  } else {
    CTLOG(debug, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void db_get_sing_pub_key(char **key_ptr, const char *device_id) {
  sqlite3 *db;
  int rc;
  *key_ptr = NULL;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }
  char *sql = "SELECT sign_pub_key FROM devices WHERE device_id = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, device_id, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    // successfully retrieved a row
    const unsigned char *sign_pub_key = sqlite3_column_text(stmt, 0);
    *key_ptr = strdup((const char *)sign_pub_key);
  } else if (rc == SQLITE_DONE) {
    CTLOG(debug, "no rows found for device_id: %s", device_id);
  } else {
    CTLOG(debug, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

int db_create_invite_key(char *key, char *exp, char *creator, char *group_id,
                         char *is_valid, time_t creation_time) {
  if (!exp || !creator || !group_id || !is_valid || !creation_time)
    return -1;

  sqlite3 *db;
  int rc;
  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "can not open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  char *sql = "INSERT INTO invite_keys (key, exp, creator, "
              "group_id, is_valid, creation_time )"
              "VALUES (?,?,?,?,?,?);";

  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    CTLOG(error, "failed to prepare statement: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return rc;
  }

  sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, exp, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, creator, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, group_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, is_valid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, creation_time);

  rc = sqlite3_step(stmt);

  if (rc != SQLITE_DONE) {
    CTLOG(error, "failed to execute statement: %s", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  rc = rc == SQLITE_DONE ? SQLITE_OK : -1;
  sqlite3_close(db);
  return rc;
}
