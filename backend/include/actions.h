// TODO fix header import mess
#ifndef ACTION_H
#define ACTION_H
#ifndef OPENSSL_UTIL_H
#define OPENSSL_UTIL_H
#include <openssl/ssl.h>
#endif
#include "parser.h"

#define ACTION_REGISTRAION_STATUS "REGISTRAION_STATUS"

#define KEY_NOT_FOUND 0
#define KEY_FOUND 1
#define ACCESS_KEY_LENGTH 40
#define SALT_LENGTH 40
#define KEY_SALT_LENGTH (ACCESS_KEY_LENGTH + SALT_LENGTH)
#define HASH256_TEXT_LENGTH 64
#define HASH_SALT_LENGTH (HASH256_TEXT_LENGTH + SALT_LENGTH)
#define DEVICE_ID_LENGTH 40

#define PING "PING"

void ping_action(CTSFrame *cts_frame, CTSFrame *res_frame);
void server_access_verify_action(CTSFrame *req_frame, CTSFrame *res_frame);
void register_device_action(CTSFrame *req_frame, CTSFrame *res_frame);
void login_device_action(CTSFrame *req_frame, CTSFrame *res_frame);
void create_group_action(CTSFrame *req_frame, CTSFrame *res_frame);
void store_invite_key_action(CTSFrame *req_frame, CTSFrame *res_frame);
void join_group_action(CTSFrame *req_frame, CTSFrame *res_frame);
void accept_device_action(CTSFrame *req_frame, CTSFrame *res_frame);
void pull_action(CTSFrame *req_frame, CTSFrame *res_frame);
void send_clipboard_action(CTSFrame *req_frame, CTSFrame *res_frame);
void group_check_action(CTSFrame *req_frame, CTSFrame *res_frame);
int send_group_devices(char *device_id);



// This function returns a JSON object containing information about all devices
// in the group, including the device ID, device name, IP address, and the online
// status of each device in the group that the requesting device is part of.
void online_devices_action(CTSFrame *req_frame, CTSFrame *res_frame);


// Retruns the invite key
void get_invite_key_action(CTSFrame *req_frame, CTSFrame *res_frame);

// Removes device from group
void kick_device_action(CTSFrame *cts_frame, CTSFrame *res_frame);
#endif
