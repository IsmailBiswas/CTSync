#include "actions.h"
#include "cequiq.h"
#include "cjson/cJSON.h"
#include "conn_association.h"
#include "cts_frame.h"
#include "ctsync.h"
#include "ctsync_log.h"
#include "db_functions.h"
#include "parser.h"
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

int is_key_in_line(char *line, const char *key) {
  char *word = strtok(line, " ");
  while (word != NULL) {
    // Ignore comments
    if (word[0] == '#')
      break;

    // Compare first word with the key
    if (strcmp(word, key) == 0) {
      return KEY_FOUND;
    }
    word = strtok(NULL, " ");
  }
  return KEY_NOT_FOUND;
}

// Server access key validation
int is_invite_key_valid(const char *key) {
  FILE *invite_key_file = fopen(INVITE_KEY_FILE, "r");
  if (invite_key_file == NULL) {
    CTLOG(error, "wasn't able to open server invite key file: %s",
          strerror(errno));
    return KEY_NOT_FOUND;
  }

  char *line = NULL;
  size_t bufsize = 0;
  int key_found = KEY_NOT_FOUND;

  while (getline(&line, &bufsize, invite_key_file) != -1) {
    // Check if the key exists in the line
    if (is_key_in_line(line, key) == KEY_FOUND) {
      key_found = KEY_FOUND;
      break;
    }
  }

  free(line);
  fclose(invite_key_file);

  if (key_found == KEY_NOT_FOUND) {
    CTLOG(error, "invalid server invite key");
  }

  return key_found;
}

void server_access_verify_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  char *server_invite_key =
      get_string_value_by_string_key(req_frame, "server_invite_key");

  if (!server_invite_key) {
    char *error_key = "error";
    char *error_msg = "required key not found";
    (void)add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                 error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    return;
  }

  int key_valid = is_invite_key_valid(server_invite_key);

  free(server_invite_key);
  if (key_valid) {
    set_server_access(ACCESS_GRANTED);
    res_frame->status_code = CTSYNC_OK;
    CTLOG(debug, "server invite key is valid");
  } else {
    char *error_key = "error";
    char *error_msg = "invalid invite key";
    (void)add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                 error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(error, "server access verification failed");
  }
}

void generate_random_string(char *output, size_t length) {
  const char charset[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  size_t charset_size = sizeof(charset) - 1;
  unsigned char random_bytes[length];
  if (RAND_bytes(random_bytes, length) != 1) {
    CTLOG(error, "failed to generate random bytes.\n");
  }

  for (size_t i = 0; i < length; ++i) {
    output[i] = charset[random_bytes[i] % charset_size];
  }
  output[length] = '\0'; // Null-terminate the string
}

void generate_hash(const char *input, char *output) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256((unsigned char *)input, strlen(input), hash);

  // Convert the hash to a hexadecimal string
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    sprintf(&output[i * 2], "%02x", hash[i]);
  }
  output[SHA256_DIGEST_LENGTH * 2] = '\0'; // Null-terminate the string
}

void register_device_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  /* Generates an access key and device id, stores hash of 'access key + salt'
   * and stores it in db then returns the access key to the client and stores
   * other information such as device name and public key.
   * */

  char *device_name = get_string_value_by_string_key(req_frame, "device_name");
  char *sign_pub_key =
      get_string_value_by_string_key(req_frame, "sign_pub_key");
  char *pub_key = get_string_value_by_string_key(req_frame, "pub_key");

  if (!device_name || !sign_pub_key || !pub_key) {
    return;
  }

  /* generate access key */
  char access_key[ACCESS_KEY_LENGTH + 1];
  char salt[SALT_LENGTH + 1];

  const int delimiter_len = strlen(KEY_VALUE_DELIMITER);

  char key_salt_combo[KEY_SALT_LENGTH + delimiter_len + 1];
  char hash_output[HASH256_TEXT_LENGTH + 1];
  char hash_salt_combo[HASH_SALT_LENGTH + delimiter_len + 1];
  char device_id_str[DEVICE_ID_LENGTH];

  uuid_t device_id;
  uuid_generate_random(device_id);
  uuid_unparse(device_id, device_id_str);

  generate_random_string(access_key, ACCESS_KEY_LENGTH);
  generate_random_string(salt, SALT_LENGTH);

  snprintf(key_salt_combo, sizeof(key_salt_combo) + 2, "%s%s%s", access_key,
           KEY_VALUE_DELIMITER, salt);

  // generate hash of access + separator + salt
  generate_hash(key_salt_combo, hash_output);

  snprintf(hash_salt_combo, sizeof(hash_salt_combo) + 2, "%s%s%s", hash_output,
           KEY_VALUE_DELIMITER, salt);

  // Send back the access_key and hash to the client as cookie.
  size_t response_len =
      ACCESS_KEY_LENGTH + DEVICE_ID_LENGTH + delimiter_len + 1;
  char response_buf[response_len];

  snprintf(response_buf, sizeof(response_buf), "%s%s%s", access_key,
           KEY_VALUE_DELIMITER, device_id_str);

  char *key = "login_key";
  int ret = add_kv(res_frame, strlen(key), key, strlen(access_key), access_key);
  if (ret == -1) {
    goto cleanup;
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  char *id = "device_id";
  int ret1 =
      add_kv(res_frame, strlen(id), id, strlen(device_id_str), device_id_str);
  if (ret1 == -1) {
    goto cleanup;
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  res_frame->status_code = CTSYNC_OK;

  // Store new device in DB
  db_register_device(device_name, hash_salt_combo, device_id_str, sign_pub_key,
                     pub_key);
  set_authentication(AUTH_STATUS_AUTHENTICATED);
  set_dev_id(device_id_str);

cleanup:
  free(device_name);
  free(sign_pub_key);
  free(pub_key);
}

void group_check_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  char *device_id = get_dev_id();
  if (!device_id) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
  }

  if (db_is_in_group(device_id) <= 0) {
    char *error_Key = "error";
    char *error_msg = "You are not part of any group";
    add_kv(res_frame, strlen(error_Key), error_Key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    return;
  }
  res_frame->status_code = CTSYNC_OK;
}

void create_group_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  char *device_id = get_dev_id();
  CTLOG(info, "device_id from state: %s", device_id);

  if (can_create_group(device_id) != 0) {
    char *error_key = "error";
    char *error_msg = "you are not allowed to create new group";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    return;
  }

  char new_group_id[UUID_STR_LEN];
  if (db_create_new_group(new_group_id, sizeof(new_group_id)) < 0) {
    char *error_key = "error";
    char *error_msg = "failed to created new group";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_SERVER_ERROR;

    return;
  };
  CTLOG(debug, "value should be uuid: %s", new_group_id);

  //  update the device to indicate it's group

  if (db_update_device_group(device_id, new_group_id, -1,
                             DEVICE_STATE_ACCEPTED) < 0) {
    char *error_key = "error";
    char *error_msg = "failed to update group information";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  };

  // Send group creation status.
  res_frame->status_code = CTSYNC_OK;
}

void login_device_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  // what is device_id?
  char *device_id = get_string_value_by_string_key(req_frame, "device_id");
  // what is key???
  char *key = get_string_value_by_string_key(req_frame, "login_key");

  if (!device_id || !key) {
    return;
  }

  char *stored_hash_salt = NULL;

  db_get_hash_salt(&stored_hash_salt,
                   device_id); // TODO: could you use that generic fucntion that
                               // returns one value

  if (!stored_hash_salt) {
    char *error_key = "error";
    char *error_msg =
        "Login failed. Was not able to retrive data from database";

    (void)add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
                 error_msg);

    res_frame->status_code = CTSYNC_SERVER_ERROR;

    free(key);
    return;
  }

  char *delimiter_pos = strstr(stored_hash_salt, KEY_VALUE_DELIMITER);
  if (!delimiter_pos) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }
  size_t hash_length = delimiter_pos - stored_hash_salt;
  size_t salt_length = strlen(delimiter_pos + strlen(KEY_VALUE_DELIMITER));

  char *extracted_salt = malloc(salt_length + 1);
  if (!extracted_salt) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  char *extracted_hash = malloc(hash_length + 1);
  if (!extracted_hash) {
    free(extracted_salt);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  strncpy(extracted_hash, stored_hash_salt, hash_length);
  extracted_hash[hash_length] = '\0';

  strncpy(extracted_salt, delimiter_pos + strlen(KEY_VALUE_DELIMITER),
          salt_length);
  extracted_salt[salt_length] = '\0';

  char computed_hash[HASH256_TEXT_LENGTH + 1];

  size_t key_salt_length =
      strlen(key) + salt_length + strlen(KEY_VALUE_DELIMITER) + 1;
  char *key_salt_combo = malloc(key_salt_length);
  if (!key_salt_combo) {
    free(extracted_hash);
    free(extracted_salt);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  snprintf(key_salt_combo, key_salt_length, "%s%s%s", key, KEY_VALUE_DELIMITER,
           extracted_salt);
  generate_hash(key_salt_combo, computed_hash);

  if (strcmp(computed_hash, extracted_hash) == 0) {
    set_authentication(AUTH_STATUS_AUTHENTICATED);
    set_dev_id(device_id);
    res_frame->status_code = CTSYNC_OK;
  }

  free(key_salt_combo);
  free(extracted_salt);
  free(stored_hash_salt);
  free(extracted_hash);
  free(device_id);
};

unsigned char *decode_base64(const char *input, size_t *sig_size) {
  BIO *b64_bio, *mem_bio;
  unsigned char *decoded_data;
  size_t input_lenght = strlen(input);

  decoded_data = (unsigned char *)malloc(input_lenght);
  if (!decoded_data)
    return NULL;

  b64_bio = BIO_new(BIO_f_base64());
  mem_bio = BIO_new_mem_buf(input, -1);
  BIO_set_flags(b64_bio, BIO_FLAGS_BASE64_NO_NL);
  mem_bio = BIO_push(b64_bio, mem_bio);

  *sig_size = BIO_read(mem_bio, decoded_data, input_lenght);

  BIO_free_all(mem_bio);
  return decoded_data;
}

int verify_signature(const char *public_key_str, const char *signature_b64,
                     const unsigned char *data1, size_t data1_len,
                     const unsigned char *data2, size_t data2_len) {

  EVP_PKEY *public_key = NULL;
  BIO *bio = NULL;
  int result = 0;
  unsigned char *signature = NULL;
  size_t sig_len = 0;

  // Create a BIO from the public key string
  bio = BIO_new_mem_buf(public_key_str, -1);
  if (!bio) {
    CTLOG(error, "failed to create BIO for public key");
    return 0;
  }

  // Read public key from memory
  public_key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  BIO_free(bio);

  if (!public_key) {
    CTLOG(error, "failed to read public key");
    return 0;
  }

  // Decode base64 signature
  signature = decode_base64(signature_b64, &sig_len);
  if (!signature) {
    CTLOG(error, "failed to decode base64 signature");
    EVP_PKEY_free(public_key);
    return 0;
  }

  // Create the message digest context
  EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
  if (!md_ctx) {
    CTLOG(debug, "failed to create message digest context");
    free(signature);
    EVP_PKEY_free(public_key);
    return 0;
  }

  // Initialize verification
  if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, public_key) != 1) {
    CTLOG(debug, "failed to initialize verification");
    goto cleanup;
  }

  // Update with first string
  if (EVP_DigestVerifyUpdate(md_ctx, data1, data1_len) != 1) {
    CTLOG(debug, "failed to update digest with first string");
    goto cleanup;
  }

  // Update with second string
  if (EVP_DigestVerifyUpdate(md_ctx, data2, data2_len) != 1) {
    CTLOG(debug, "failed to update digest with second string");
    goto cleanup;
  }

  // Verify the signature
  int verify_result = EVP_DigestVerifyFinal(md_ctx, signature, sig_len);

  if (verify_result == 1) {
    CTLOG(debug, "signature verified successfully!");
    result = 1;
  } else if (verify_result == 0) {
    CTLOG(debug, "signature verification failed - invalid signature");
  } else {
    CTLOG(debug, "signature verification failed - error occurred");
  }

cleanup:
  // Clean up
  EVP_MD_CTX_free(md_ctx);
  EVP_PKEY_free(public_key);
  free(signature);

  return result;
}

void store_invite_key_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  char *key = get_string_value_by_string_key(req_frame, "key");
  char *exp = get_string_value_by_string_key(req_frame, "exp");
  char *signature = get_string_value_by_string_key(req_frame, "signature");
  char *group_id = NULL;
  char *sing_pub_key = NULL;
  char *connection_state = NULL;
  char *device_id = NULL;

  if (!key || !exp || !signature) {
    char *error_key = "error";
    char *error_msg = "Missing required key.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(warning, "store_invite_Key: key exp or signature not found");
    goto cleanup;
  }

  device_id = get_dev_id();
  if (!device_id) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key: device id not found");
    goto cleanup;
  }

  db_fetch_column_data(&group_id, "devices", "device_id", device_id,
                       "group_id");

  if (!group_id) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key: group_id not found");
    goto cleanup;
  }

  CTLOG(info, "The invite key: %s", key);
  CTLOG(info, "expiry time: %s", exp);
  CTLOG(info, "signature: %s", signature);
  CTLOG(info, "user id is: %s", device_id);
  CTLOG(info, "user group id is: %s", group_id);

  db_get_sing_pub_key(&sing_pub_key, device_id);

  if (!sing_pub_key) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key: sign_pub_key not found");
    goto cleanup;
  }

  if (verify_signature(sing_pub_key, signature, (unsigned char *)key,
                       strlen(key), (unsigned char *)exp, strlen(exp)) != 1) {

    char *error_key = "error";
    char *error_msg = "Signature verification failed.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;

    CTLOG(warning, "store_invite_key: signature verification failed");
    goto cleanup;
  }

  db_fetch_column_data(&connection_state, "devices", "device_id", device_id,
                       "connection_state");

  if (!connection_state) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key: connection_state not found");
    goto cleanup;
  }

  DeviceState state = string_to_device_state((const char *)connection_state);
  // check if the device status is `accepted`.
  if (state != DEVICE_STATE_ACCEPTED) {

    char *error_key = "error";
    char *error_msg = "Your are not part of any group.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;

    CTLOG(warning, "store_invite_key: device connection_state is not "
                   "DEVICE_STATE_ACCEPTED");
    goto cleanup;
  }

  time_t current_time = time(NULL);
  // delete exising key
  if (db_delete_invite_key(device_id) == -1) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key:  failed to delete existing key");
    goto cleanup;
  };

  // store new key
  char is_key_valid = '1';
  if (db_create_invite_key(key, exp, device_id, group_id, &is_key_valid,
                           current_time) < 0) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(warning, "store_invite_key:  failed to store new key, DB problem");
    goto cleanup;
  }

  res_frame->status_code = CTSYNC_OK;
  char *rinvite_key_key = "invite_key";
  add_kv(res_frame, strlen(rinvite_key_key), rinvite_key_key, strlen(key), key);

cleanup:
  free(key);
  free(exp);
  free(signature);
  free(device_id);
  free(connection_state);
  free(group_id);
  return;
}

int get_public_key_sha256(const char *pem_string, unsigned char *hash_output) {
  BIO *bio = NULL;
  EVP_PKEY *pkey = NULL;
  unsigned int hash_len;
  EVP_MD_CTX *mdctx = NULL;
  int success = 0;

  // Create a BIO from the PEM string
  bio = BIO_new_mem_buf(pem_string, -1);
  if (!bio) {
    goto cleanup;
  }

  // Read public key from PEM
  pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  if (!pkey) {
    goto cleanup;
  }

  // Create digest context
  mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    goto cleanup;
  }

  // Initialize the digest context for SHA256
  if (!EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
    goto cleanup;
  }

  // Get the raw public key data
  unsigned char *key_data = NULL;
  int key_len = i2d_PUBKEY(pkey, &key_data);
  if (key_len <= 0) {
    goto cleanup;
  }

  // Calculate hash
  if (!EVP_DigestUpdate(mdctx, key_data, key_len)) {
    OPENSSL_free(key_data);
    goto cleanup;
  }

  // Finalize hash
  if (!EVP_DigestFinal_ex(mdctx, hash_output, &hash_len)) {
    OPENSSL_free(key_data);
    goto cleanup;
  }

  OPENSSL_free(key_data);
  success = 1;

cleanup:
  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pkey);
  BIO_free(bio);
  return success;
}

void hash_to_hex(const unsigned char *hash, char *hex_string) {
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    sprintf(hex_string + (i * 2), "%02x", hash[i]);
  }
  hex_string[SHA256_DIGEST_LENGTH * 2] = '\0';
}

int send_group_join_notification(int *sent_number, char *device_id,
                                 char *groud_id) {

  char *pub_key = NULL;
  char *device_alias = NULL;

  // requesting device pub_key retrieves
  db_fetch_column_data(&pub_key, "devices", "device_id", device_id, "pub_key");
  if (!pub_key) {
    return -1;
  }

  // requesting device device alias
  db_fetch_column_data(&device_alias, "devices", "device_id", device_id,
                       "device_name");
  if (!device_alias) {
    return -1;
  }

  // create the pub key hash of the requesting device, this will help identify
  // the device by accepting device
  unsigned char hash[SHA256_DIGEST_LENGTH];
  char hex_string[SHA256_DIGEST_LENGTH * 2 + 1];

  if (!get_public_key_sha256(pub_key, hash)) {
    CTLOG(error, "failed to create public key hash");
    free(pub_key);
    free(device_alias);
    return -1;
  }

  hash_to_hex(hash, hex_string);

  GroupDeviceList *group_devices =
      db_get_devices_by_group(groud_id, DEVICE_STATE_ACCEPTED);

  CTSFrame *frame = create_cts_frame();
  if (!frame) {
    free(pub_key);
    free(device_alias);
    free_group_device_list(group_devices);
    return -1;
  }

  char *device_alias_key = "device_alias";
  char *device_id_key = "device_id";
  char *pub_key_hash_key = "pub_key_hash";
  char *action_key = ACTION_KEY;
  char *action = "GROUP_JOIN_REQUEST";

  add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  add_kv(frame, strlen(device_alias_key), device_alias_key,
         strlen(device_alias), device_alias);
  add_kv(frame, strlen(device_id_key), device_id_key, strlen(device_id),
         device_id);
  add_kv(frame, strlen(pub_key_hash_key), pub_key_hash_key, strlen(hex_string),
         hex_string);

  // send the group join notification to every online device in the requeseted
  // group
  for (int i = 0; i < group_devices->count; i++) {
    if (group_devices->device_online_list[i]) {
      CTLOG(info, "sending group join notification: %d", i);
      char *conn_id = get_conn_id_by_dev_id(group_devices->device_id_list[i]);
      if (!conn_id) {
        CTLOG(debug, "device offline");
        continue;
      }
      server_send(frame, conn_id);
      *sent_number += 1;
      free(conn_id);
    }
  }

  CTLOG(debug, "Sent this many devices: %d", group_devices->count);

  free_owned_frame(frame);
  free_group_device_list(group_devices);
  free(pub_key);
  free(device_alias);

  return 0;
}

void join_group_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  char *key = get_string_value_by_string_key(req_frame, "key");

  if (!key) {
    char *error_key = "error";
    char *error_msg = "Missing required key";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    return;
  }

  char *is_valid = NULL;
  char *exp = NULL;
  char *creation_time = NULL;
  char *group_id = NULL;

  db_get_invite_key(&is_valid, &creation_time, &exp, &group_id, key);

  if (!is_valid || !exp || !creation_time || !group_id) {
    char *error_key = "error";
    char *error_msg = "Invalid group join key";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;

    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);
    return;
  }

  errno = 0;
  char *endptr;
  int group_pk = strtol(group_id, &endptr, 10);
  if (errno != 0) {
    char *error_key = "error";
    char *error_msg = "Invalid data";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    CTLOG(error, "failed to convert group_id to group_pk");
    res_frame->status_code = CTSYNC_FAIL;
    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);
    return;
  }

  // check is the invite key is marked as invalid
  if (!(int)*is_valid) {
    char *error_key = "error";
    char *error_msg = "Invalid invite key";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    CTLOG(error, "invite key is not valid");
    res_frame->status_code = CTSYNC_FAIL;
    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);

    return;
  }

  long exp_time = atoi(creation_time) + atoi(exp) * 3600;

  time_t cur_time = time(NULL);
  if (cur_time == (time_t)(-1) || cur_time > exp_time) {
    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);
    CTLOG(info, "current time: %lu  expiry time: %lu", cur_time, exp_time);

    char *error_key = "error";
    char *error_msg = "Invite key has expired.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);

    CTLOG(error, "invite key has expired");
    res_frame->status_code = CTSYNC_FAIL;
    return;
  }

  char *device_id = get_dev_id();
  if (db_update_device_group(device_id, NULL, group_pk, DEVICE_STATE_PENDING) <
      0) {
    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  };

  int sent_number = 0;
  if (send_group_join_notification(&sent_number, device_id, group_id) == -1) {
    char *error_key = "error";
    char *error_msg = "failed to sent notification to other devices";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
  };

  if (sent_number <= 0) {
    char *error_key = "error";
    char *error_msg =
        "There is no device online in the group you want to join.";
    char *no_online_key = "no_online";
    char *no_online = "1";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    add_kv(res_frame, strlen(no_online_key), no_online, strlen(no_online),
           no_online);
    res_frame->status_code = CTSYNC_FAIL;
    free(is_valid);
    free(exp);
    free(creation_time);
    free(group_id);
    return;
  }

  res_frame->status_code = CTSYNC_OK;
  free(is_valid);
  free(exp);
  free(creation_time);
  free(group_id);
};

void accept_device_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  char *aprv_device_id = get_string_value_by_string_key(
      req_frame, "device_id"); // device_id which will be approved

  char *aprv_group_id = NULL; // group_id where new device want to get approved

  char *req_group_id = NULL; // group_id of device who is making request to
                             // approve the new device

  if (!aprv_device_id) {
    CTLOG(debug, "device_id is not provided");
    char *error_key = "error";
    char *error_msg = "Required key missing";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    free(aprv_group_id);
    free(aprv_device_id);
    free(aprv_device_id);
    return;
  }

  db_fetch_column_data(&aprv_group_id, "devices", "device_id", aprv_device_id,
                       "group_id");
  if (!aprv_device_id) {
    free(aprv_device_id);
    char *error_key = "error";
    char *error_msg = "Requested group id not found.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);

    res_frame->status_code = CTSYNC_FAIL;
    free(aprv_group_id);
    free(aprv_device_id);
    free(aprv_device_id);

    return;
  }

  char *device_id = get_dev_id();
  db_fetch_column_data(&req_group_id, "devices", "device_id", device_id,
                       "group_id");
  free(device_id);
  if (!req_group_id) {
    CTLOG(error, "Was not able to find group id of device that received group "
                 "join request approval notification.");
    free(aprv_group_id);
    free(aprv_device_id);
    free(aprv_device_id);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  // make sure device is only approving devices in its own group
  if (strcmp(aprv_group_id, req_group_id) != 0) {
    char *error_key = "error";
    char *error_msg = "You can only approve devices in your own group.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    free(aprv_device_id);
    free(req_group_id);
    free(aprv_group_id);
    return;
  }

  if (db_update_connection_state(aprv_device_id, DEVICE_STATE_ACCEPTED) < 0) {
    CTLOG(error, "was not able to update connection status for new device");
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    free(aprv_group_id);
    free(aprv_device_id);
    free(aprv_device_id);
    return;
  };

  online_devices_action(req_frame, res_frame); //

  CTSFrame *frame = create_cts_frame();
  // if frame craetion fail, return success as the DB is already updated
  if (!frame) {
    res_frame->status_code = CTSYNC_OK;
    free(req_group_id);
    free(aprv_device_id);
    free(aprv_group_id);
    return;
  }

  // send notify requester that request has been appcepted
  char *action = "GROUP_JOIN_RESPONSE";
  char *action_key = "action";
  frame->status_code = CTSYNC_OK;
  add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  char *requester_conn_id = get_conn_id_by_dev_id(aprv_device_id);
  server_send(frame, requester_conn_id);
  free_owned_frame(frame);

  res_frame->status_code = CTSYNC_OK;

  free(req_group_id);
  free(aprv_device_id);
  free(aprv_group_id);
}

// creates a json object with all interested devices id and their public enc key
void pull_action(CTSFrame *req_frame, CTSFrame *res_frame) {

  /*
  When a device locally detects clipboard change it makes the 'CLIPBOARD'
  request.
  */
  char *json_data = NULL;
  char *device_id = get_dev_id();
  if (!device_id) {
    CTLOG(error, "was not able to retrive device_id: pull_action");
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  char *group_id = NULL;
  db_fetch_column_data(&group_id, "devices", "device_id", device_id,
                       "group_id");

  if (!group_id) {
    CTLOG(error, "was not able to retrive group id: pull_action");
    free(device_id);
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  data_interested_dev_list *devices = db_get_interested_devices(group_id);

  if (!devices) {
    CTLOG(error, "was not abble to get interesed devies from DB");
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    free(device_id);
    free(group_id);
    return;
  }

  // creates a joson object that contains every intereseted device's id and
  // pub_key
  cJSON *root = cJSON_CreateArray();
  for (int i = 0; i < devices->count; i++) {
    if (strcmp(devices->device_id_list[i], device_id) == 0) {
      CTLOG(debug, "excluding own entry from interesed recipient list");
      continue;
    }

    CTLOG(info, "adding device info to json object");
    cJSON *obj = cJSON_CreateObject();

    if (!cJSON_AddStringToObject(obj, "device_id",
                                 devices->device_id_list[i]) ||
        !cJSON_AddStringToObject(obj, "enc_key", devices->p_enc_key_list[i]) ||
        !cJSON_AddItemToArray(root, obj)) {

      res_frame->status_code = CTSYNC_SERVER_ERROR;
      CTLOG(error, "unable to create jsong object");
      goto cleanup;
    }
  }

  CTLOG(info, "number of online devices: %d", devices->count);
  json_data = cJSON_Print(root);
  char *data_key = "data";
  res_frame->status_code = CTSYNC_OK;
  add_kv(res_frame, strlen(data_key), data_key, strlen(json_data), json_data);

cleanup:
  cJSON_Delete(root);
  free(group_id);
  free_data_interested_dev_list(devices);
  free(json_data);
}

// so basically, the server here is relaying data from the sender to the
// recipient
void send_clipboard_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  // get the recipient, device_id, signature and the body.

  char *rec_device_id = get_string_value_by_string_key(req_frame, "device_id");

  uint64_t encd_data_signature_size = 0;
  void *encd_data_signature = get_byte_value_by_string_key(
      req_frame, "encd_data_signature", &encd_data_signature_size);

  uint64_t encd_data_size = 0;
  void *encd_data =
      get_byte_value_by_string_key(req_frame, "encd_data", &encd_data_size);

  uint64_t encd_key_size = 0;
  void *encd_key =
      get_byte_value_by_string_key(req_frame, "encd_key", &encd_key_size);
  uint64_t encd_iv_size = 0;
  void *encd_iv =
      get_byte_value_by_string_key(req_frame, "encd_iv", &encd_iv_size);

  if (!rec_device_id || !encd_data_signature || !encd_data) {
    char *error_key = "error";
    char *error_msg = "Missing required key.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;

    free(rec_device_id);
    free(encd_data_signature);
    free(encd_data);
    free(encd_key);
    free(encd_iv);
    return;
  }

  char *conn_id = get_conn_id_by_dev_id(rec_device_id);
  if (!conn_id) {
    CTLOG(info, "device went offline");
    char *error_key = "error";
    char *error_msg = "Device is not online";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    free(rec_device_id);
    free(encd_data_signature);
    free(encd_data);
    free(encd_key);
    free(encd_iv);
    return;
  }

  char *sender_device_id = get_dev_id();
  if (!sender_device_id) {
    CTLOG(error, "DIDN'T find current connection's device id.");
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    free(rec_device_id);
    free(encd_data_signature);
    free(encd_data);
    free(encd_key);
    free(encd_iv);
    return;
  }

  CTSFrame *frame = create_cts_frame();
  if (!frame) {
    free(rec_device_id);
    free(encd_data_signature);
    free(sender_device_id);
    free(conn_id);
    free(encd_key);
    free(encd_iv);
    return;
  }

  char *action_key = "action";
  char *action = "RECV_CLIPBOARD";

  char *device_id_key = "device_id";
  char *signature_key = "encd_data_signature";
  char *data_key = "encd_data";
  char *encd_key_key = "encd_key";
  char *encd_iv_key = "encd_iv";

  add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  add_kv(frame, strlen(device_id_key), device_id_key, strlen(sender_device_id),
         sender_device_id);
  add_kv(frame, strlen(signature_key), signature_key, encd_data_signature_size,
         encd_data_signature);
  add_kv(frame, strlen(data_key), data_key, encd_data_size, encd_data);

  add_kv(frame, strlen(encd_key_key), encd_key_key, encd_key_size, encd_key);
  add_kv(frame, strlen(encd_iv_key), encd_iv_key, encd_iv_size, encd_iv);

  frame->status_code = CTSYNC_OK;
  server_send(frame, conn_id);

#ifdef DEBUG
  char *recp_device_name = NULL;
  db_fetch_column_data(&recp_device_name, "devices", "device_id", rec_device_id,
                       "device_name");
  CTLOG(info, "sending clipboard content to: %s", recp_device_name);
  free(recp_device_name);
#endif

  free(rec_device_id);
  free(encd_data_signature);
  free(sender_device_id);
  free(conn_id);
  free(encd_iv);
  free(encd_key);
}

int send_group_devices(char *device_id) {
  //

  char *group_id = NULL;
  // get group id
  db_fetch_column_data(&group_id, "devices", "device_id", device_id,
                       "group_id");

  if (!group_id) {
    return -1;
  }
  // get all devices in own group
  GroupDeviceList *group_devices =
      db_get_devices_by_group(group_id, DEVICE_STATE_ACCEPTED);

  // loop through each device in the active connection and create json object

  cJSON *root = cJSON_CreateArray();
  int is_online = 0;

  for (int i = 0; i < group_devices->count; i++) {
    cJSON *obj = cJSON_CreateObject();
    if (!cJSON_AddStringToObject(obj, "device_id",
                                 group_devices->device_id_list[i]) ||
        !cJSON_AddStringToObject(obj, "device_name",
                                 group_devices->device_name_list[i]) ||
        !cJSON_AddStringToObject(obj, "public_key",
                                 group_devices->public_key[i]) ||
        !cJSON_AddStringToObject(obj, "sign_public_key",
                                 group_devices->sign_public_key[i])

    ) {
      CTLOG(error,
            "skipping one device inclution: unable to create json object");
      continue;
    }

    // if device is online set the is_online field
    if (group_devices->device_online_list[i]) {
      if (!cJSON_AddBoolToObject(obj, "is_online", true)) {
        CTLOG(error, "failed to add a field to online devices list");
      }
    } else {
      if (!cJSON_AddBoolToObject(obj, "is_online", false)) {
        CTLOG(error, "failed to add a field to online devices list");
      }
    }
    cJSON_AddItemToArray(root, obj);
  }

  char *json_data = cJSON_Print(root);

  char *data_key = "data";
  char *action_key = ACTION_KEY;
  char *action = "ONLINE_DEVICES";

  CTSFrame *frame = create_cts_frame();
  if (!frame) {
    free_group_device_list(group_devices);
    free(group_id);
    free(json_data);
    cJSON_Delete(root);
    return -1;
  }

  // TODO: check every add_kv return value
  add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  add_kv(frame, strlen(data_key), data_key, strlen(json_data), json_data);

  for (int i = 0; i < group_devices->count; i++) {
    if (group_devices->device_online_list[i]) {
      char *conn_id = get_conn_id_by_dev_id(group_devices->device_id_list[i]);
      if (!conn_id) {
        CTLOG(debug, "device offline");
        continue;
      }
      server_send(frame, conn_id);
      free(conn_id);
    }
  }

  free_group_device_list(group_devices);
  free(group_id);
  free(json_data);
  cJSON_Delete(root);
  return 0;
}

// a device requesting this means that it just came online and wants to know
// who's else online. Instead of replying only to it, broadcast the current
// online list to every online device in the group
// should be renamed to get_group_devices_action
void online_devices_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  char *group_id = NULL;

  char *device_id = get_dev_id();
  if (!device_id) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  if (send_group_devices(device_id) == -1) {
    req_frame->status_code = CTSYNC_FAIL;
  };

  // get group id
  // db_fetch_column_data(&group_id, "devices", "device_id", device_id,
  //                      "group_id");

  // if (!group_id) {
  //   res_frame->status_code = CTSYNC_SERVER_ERROR;
  //   free(device_id);
  //   return;
  // }

  // // get all devices in own group
  // GroupDeviceList *group_devices =
  //     db_get_devices_by_group(group_id, DEVICE_STATE_ACCEPTED);

  // // loop through each device in the active connection and create json object

  // cJSON *root = cJSON_CreateArray();
  // int is_online = 0;

  // for (int i = 0; i < group_devices->count; i++) {
  //   cJSON *obj = cJSON_CreateObject();
  //   if (!cJSON_AddStringToObject(obj, "device_id",
  //                                group_devices->device_id_list[i]) ||
  //       !cJSON_AddStringToObject(obj, "device_name",
  //                                group_devices->device_name_list[i]) ||
  //       !cJSON_AddStringToObject(obj, "public_key",
  //                                group_devices->public_key[i]) ||
  //       !cJSON_AddStringToObject(obj, "sign_public_key",
  //                                group_devices->sign_public_key[i])

  //   ) {
  //     CTLOG(error,
  //           "skipping one device inclution: unable to create json object");
  //     continue;
  //   }

  //   // if device is online set the is_online field
  //   if (group_devices->device_online_list[i]) {
  //     if (!cJSON_AddBoolToObject(obj, "is_online", true)) {
  //       CTLOG(error, "failed to add a field to online devices list");
  //     }
  //   } else {
  //     if (!cJSON_AddBoolToObject(obj, "is_online", false)) {
  //       CTLOG(error, "failed to add a field to online devices list");
  //     }
  //   }
  //   cJSON_AddItemToArray(root, obj);
  // }

  // char *json_data = cJSON_Print(root);

  // char *data_key = "data";
  // char *action_key = ACTION_KEY;
  // char *action = "ONLINE_DEVICES";

  // CTSFrame *frame = create_cts_frame();
  // if (!frame) {
  //   free_group_device_list(group_devices);
  //   free(group_id);
  //   free(json_data);
  //   cJSON_Delete(root);
  //   return;
  // }

  // // TODO: check every add_kv return value
  // add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  // add_kv(frame, strlen(data_key), data_key, strlen(json_data), json_data);

  // for (int i = 0; i < group_devices->count; i++) {
  //   if (group_devices->device_online_list[i]) {
  //     char *conn_id =
  //     get_conn_id_by_dev_id(group_devices->device_id_list[i]); if (!conn_id)
  //     {
  //       CTLOG(debug, "device offline");
  //       continue;
  //     }
  //     server_send(frame, conn_id);
  //     free(conn_id);
  //   }
  // }

  // free_group_device_list(group_devices);
  // free(group_id);
  // free(json_data);
  // cJSON_Delete(root);
}

void kick_device_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  // update the database
  char *kick_device_id = get_string_value_by_string_key(
      req_frame, "device_id"); // id of the device to be removed
  if (!kick_device_id) {
    char *error_key = "error";
    char *error_msg = "Missing required key.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(error, "device_id key not found to be removed device");
    return;
  }
  char *kick_device_grp_id = NULL;
  char *requester_grp_id = NULL; // group id of the device who made this request

  // check request device is in the same group
  db_fetch_column_data(&kick_device_grp_id, "devices", "device_id",
                       kick_device_id, "group_id");

  if (!kick_device_grp_id) {
    char *error_key = "error";
    char *error_msg = "Group ID of the device to be kicked not found.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    CTLOG(error, "ground id of the device to be removed not found");
    free(kick_device_id);
    return;
  }

  char *device_id = get_dev_id(); // id of the device making this request
  db_fetch_column_data(&requester_grp_id, "devices", "device_id", device_id,
                       "group_id");

  if (!requester_grp_id) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    CTLOG(error, "group id of the device making the kick request was not "
                 "found; this is unexpected");
    free(kick_device_grp_id);
    free(kick_device_id);
    return;
  }

  if (strcmp(requester_grp_id, kick_device_grp_id) != 0) {
    char *error_key = "error";
    char *error_msg = "You can only remove devices in your own group.";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    res_frame->status_code = CTSYNC_FAIL;
    free(requester_grp_id);
    free(kick_device_grp_id);
    free(kick_device_id);
    return;
  }

  db_update_connection_state(kick_device_id, DEVICE_STATE_KICKED);

  send_group_devices(device_id);
  free(device_id);

  // notifying other device that a device is gonski
  /*
  online_devices_action(req_frame, res_frame); // TODO: how do I handle this?
  */

  // if kicked device is not online then just return
  char *kick_conn_id = get_conn_id_by_dev_id(kick_device_id);

  if (!kick_conn_id) {
    free(requester_grp_id);
    free(kick_device_grp_id);
    free(kick_device_id);
    return;
  }

  CTSFrame *frame = create_cts_frame();
  if (!frame) {
    res_frame->status_code = CTSYNC_SERVER_ERROR;
    free(requester_grp_id);
    free(kick_device_grp_id);
    free(kick_device_id);
    return;
  }
  char *action_key = ACTION_KEY;
  char *action = "KICKED";
  add_kv(frame, strlen(action_key), action_key, strlen(action), action);
  CTLOG(debug, "SENDING KICKED NOTIFICATION TO KICKED DEVICE!!");
  server_send(frame, kick_conn_id);
  CTLOG(debug, "SENDING KICKED NOTIFICATION TO KICKED DEVICE!!");
  free_owned_frame(frame);
  // disconnect the connection, maybe not.
}

void ping_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  res_frame->status_code = CTSYNC_OK;
};

void get_invite_key_action(CTSFrame *req_frame, CTSFrame *res_frame) {
  char *device_id = get_dev_id();
  if (!device_id) {
    req_frame->status_code = CTSYNC_SERVER_ERROR;
    return;
  }

  char *invite_key = NULL;
  db_fetch_column_data(&invite_key, "invite_keys", "creator", device_id, "key");
  free(device_id);
  if (!invite_key) {
    char *error_key = "error";
    char *error_msg = "No invite key found";
    add_kv(res_frame, strlen(error_key), error_key, strlen(error_msg),
           error_msg);
    req_frame->status_code = CTSYNC_FAIL;
    return;
  }

  char *invite_key_key = "invite_key";
  add_kv(res_frame, strlen(invite_key_key), invite_key_key, strlen(invite_key),
         invite_key);
  res_frame->status_code = CTSYNC_OK;
  free(invite_key);
}
