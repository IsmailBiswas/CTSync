#!/bin/sh
set -e 
CERT_DIR="server_files"
KEY_FILE="$CERT_DIR/private_key.pem"
CERT_FILE="$CERT_DIR/certificate.pem"
INVITE_FILE="$CERT_DIR/invite_keys.txt"

mkdir -p "$CERT_DIR"
echo "invitekey123 " > $INVITE_FILE  # the space after the key is required for now

openssl req -x509 -newkey rsa:2048 -keyout "$KEY_FILE" -out "$CERT_FILE" \
    -days 365 -nodes -subj "/CN=localhost"


