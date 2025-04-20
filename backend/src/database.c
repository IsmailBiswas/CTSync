#include "ctsync.h"
#include "ctsync_log.h"
#include <sqlite3.h>
#include <stdio.h>

int create_devices_table(sqlite3 *db) {
  char *err_msg = 0;
  const char *sql =
      "CREATE TABLE IF NOT EXISTS devices"
      "( id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "device_name TEXT,"
      "device_id TEXT, "
      "access_key_hash TEXT, "
      "sign_pub_key TEXT,"
      "pub_key TEXT,"
      "last_seen DATETIME,"
      "connection_state TEXT,"
      "receive_flag INTEGER,"
      "group_id INTEGER,"
      "FOREIGN KEY (group_id) REFERENCES groups (id) ON DELETE SET NULL );";

  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return rc;
  }
  return 0;
}

int create_group_table(sqlite3 *db) {
  char *err_msg = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS groups"
                    "( id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "group_id TEXT, "
                    "last_seen DATETIME );";
  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return rc;
  }
  return 0;
}
int create_invite_key_table(sqlite3 *db) {
  char *err_msg = 0;
  const char *sql =
      "CREATE TABLE IF NOT EXISTS invite_keys"
      "( id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "key TEXT,"
      "exp DATETIME, "
      "creator TEXT, "
      "group_id TEXT,"
      "is_valid INTEGER,"
      "creation_time,"
      "FOREIGN KEY (group_id) REFERENCES groups (id) ON DELETE SET NULL);";

  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "T--> SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return rc;
  }
  return 0;
}

int db_migrate() {
  sqlite3 *db;
  int rc;

  rc = sqlite3_open(DATABASE_LOC, &db);
  if (rc != SQLITE_OK) {
    CTLOG(error, "cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  create_devices_table(db);
  create_group_table(db);
  create_invite_key_table(db);

  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return 1;
  }

  // Close the database connection
  sqlite3_close(db);

  return 0;
}
