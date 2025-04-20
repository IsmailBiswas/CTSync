# CTSYNC

## Overview
CTSYNC is a cross-platform application that automatically synchronizes the latest clipboard text across your devices. You host and own the server.

## Architecture Overview
- **Client-Server Model:** Devices must register and join a server to form or join a group.
- **Real-Time Sync:** Once in a group, any clipboard update on one device is instantly transmitted (if allowed) to all other devices in the group.
- **Security First:** End-to-end encryption ensures that clipboard data remains confidential, with all communication taking place over secure TLS channels.

## Group Management
### Group Definition
- **Group:** A collection of devices that share the latest clipboard text.
- Each device in the group automatically receives updated clipboard content from any other device in the same group.

### Group Controls and Administration
- **Device Removal:** Any device within a group can remove another device.
- **Adding Devices:** To include a new device, an existing online device must approve the request.
- **Constraints:**
    - An account can only be a member of one group.
    - If an account logs out (losing its login cookie), the device must re-register (create a new account) and rejoin a group.

## User Options and Preferences
- **Transmission Preferences:**
    - **Opt-out Sending:** A device may choose not to send its clipboard content to the group.
    - **Opt-out Receiving:** A device may choose not to receive clipboard content from the group.

## Security and Data Transfer
- The server does not store clipboard data for offline devices.
- Notifications and join requests are only delivered to devices that are online.
- When a new device joins a group, an existing device must be online to approve the connection, as there will be no message storage system.

### End-to-End Encryption
- **Key Generation:** Upon joining a group, a device generates two pairs of asymmetric keys:
    - **Encryption Keys:** Used to encrypt clipboard content for each recipient using their public key.
    - **Signing Keys:** Used to sign the AES key and initialization vector to prove the sender's authenticity.
- **Key Sharing:** Public keys are sent to the server and distributed to other devices in the group.

### Data Structrue for Transfer 
For transferring data between the server and client, CTSYNC uses a custom structure. 

```
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

```


## Server and Account Management
### Joining the Server
- **Account Requirement:** A device must create an account to join or create a group.
- **Registration Information:**
    - **Device Name:** Visible to other devices.
    - **Invite Key:** Verifies the invitation to join the server.

### Server Selection
- **Connection Details:** Devices must provide the server address (IP address or domain name) and port number during registration.

### Invite Key Management
- **Format:** A alphanumeric character sequence.
- **Storage:** Invite keys are stored in a text file (`.txt` extension) with each line formatted as:
  ```
  <key> <valid-until>
  ```
  where `<valid-until>` is in ISO 8601 format.
- **Comments:** Lines beginning with `#` (and any text after a space followed by `#`) are ignored.
- The server uses the invite key to verify that the client is authorized to connect.

## Join Group Process
- **Creating a Group:** A device can create a new group, automatically becoming its first member.
- **Joining an Existing Group:** A device must obtain a join key from an existing member and submit a join request for approval.

## API Actions

### Pre-Authentication

| Action Name       | Description                                          |
|-------------------|------------------------------------------------------|
| **PING**          | Check server availability                            |
| **REGISTER_DEVICE** | Create a new account                                |
| **ACCESS_SERVER** | Validate the server join invite key                  |
| **LOGIN**         | Authenticate with the server                         |

### Protected

| Action Name                   | Description                                                        |
|-------------------------------|--------------------------------------------------------------------|
| **CREATE_GROUP**              | Establish a new group                                              |
| **STORE_INVITE_KEY**          | Generate and store a new group invite key                          |
| **JOIN_GROUP**                | Request to join an existing group                                  |
| **ACCEPT_DEVICE**             | Approve a pending join request                                     |
| **PULL**                      | Retrieve the list of devices eligible to receive clipboard content |
| **CLIPBOARD**                 | Send clipboard data to a specific device                           |
| **KICK_DEVICE**               | Remove a device from the group                                     |
| **GET_INVITE_KEY**            | Retrieve the group invite key for the requesting device            |
| **Disable Content Reception** | Option to opt out of receiving clipboard data (under development)  |


## Database Schema
### Devices Table

| Field                | Type      | Description                                               |
|----------------------|-----------|-----------------------------------------------------------|
| **id**               | INTEGER   | Unique identifier                                         |
| **device_name**      | TEXT      | Name of the device                                        |
| **device_id**        | TEXT      | Unique device identifier                                  |
| **access_key_hash**  | TEXT      | Hashed access key (password)                              |
| **sign_pub_key**     | TEXT      | Public key for verifying the originator                   |
| **pub_key**          | TEXT      | Public key for encrypting data                            |
| **last_seen**        | DATETIME  | Last login timestamp                                      |
| **connection_state** | TEXT      | Current state (e.g., online, offline, in-group)           |
| **receive_flag**     | INTEGER   | Flag indicating if the device accepts incoming data       |
| **group_id**         | INTEGER   | Foreign key referencing the group membership              |

### Group Table

| Field  | Type    | Description                        |
|--------|---------|------------------------------------|
| **id** | INTEGER | Unique identifier |
| **group_id** | TEXT | Group identifier     |

### Invite Keys Table

| Field             | Type      | Description                                      |
|-------------------|-----------|--------------------------------------------------|
| **id**            | INTEGER   | Unique identifier                                |
| **key**           | TEXT      | Invite key                                       |
| **exp**           | DATETIME  | Expiration timestamp                             |
| **creator**       | TEXT      | Identifier of the key creator                    |
| **group_id**      | INTEGER   | Associated group ID (matches Group Table id)     |
| **is_valid**      | INTEGER   | Flag indicating key validity                     |
| **creation_time** | DATETIME  | Timestamp when the invite key was created        |


