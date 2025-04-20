#pragma  once
#include "conn_association.h"
typedef enum {
  DEVICE_STATE_PENDING = 0,
  DEVICE_STATE_ACCEPTED = 1,
  DEVICE_STATE_REJECTED = 2,
  DEVICE_STATE_KICKED = 3,
  DEVICE_STATE_NEW = 4,
  DEVICE_STATE_LEFT = 5
} DeviceState;

typedef struct {
  char **device_id_list;
  char **p_enc_key_list;
  int count;
} data_interested_dev_list;


int db_is_in_group(const char *device_id);
int db_create_new_group(char *new_group_id, size_t size);
int can_create_group(const char *device_id);
int db_update_device_group(const char *device_id, const char *group_id, const int group_pk,
                           DeviceState con_state);
void db_fetch_column_data(char **result_data, const char *table_name,
                          const char *search_column, const char *search_value,
                          const char *target_column);

const char *device_state_to_string(DeviceState state);
DeviceState string_to_device_state(const char *state_str);
void db_get_invite_key(char **is_valid, char **creation_time, char **exp,
                       char **group_id, const char *invite_Key);




GroupDeviceList *db_get_devices_by_group(const char *group_id,
                                         DeviceState con_state);
void db_fetch_column_data(char **result_data, const char *table_name,
                          const char *search_column, const char *search_value,
                          const char *target_column);
int db_update_connection_state(const char *device_id, DeviceState con_state);

void free_data_interested_dev_list(data_interested_dev_list *list);
void free_group_device_list(GroupDeviceList *list);

int db_delete_invite_key(const char *device_id);

data_interested_dev_list *db_get_interested_devices(const char *group_id);
int db_register_device(char *device_name, char *access_key_hash,
                       char *device_id, char *sign_pub_key, char *pub_key);
void db_get_hash_salt(char **data, const char *device_id);

void db_get_sing_pub_key(char **data, const char *device_id);
int db_create_invite_key(char *key, char *exp, char *creator, char *group_id,
                         char *is_valid, time_t creation_time);

