use openssl::rsa::Rsa;
use std::io;
use tauri::AppHandle;

use crate::restore::ctsync_set_key;

/// Generates two pairs of RSA keys (one for signing and one general-purpose),
/// saves them as PEM files in the app's local data directory, and returns the
/// signing and general-purpose public keys as UTF-8 strings.
pub fn gen_keys(app: AppHandle) -> io::Result<(String, String)> {
    // Generate two 4096-bit RSA key pairs
    let sign_rsa = Rsa::generate(4096)?;
    let rsa = Rsa::generate(4096)?;
    let sign_private_key = sign_rsa.private_key_to_pem()?;
    let sign_public_key = sign_rsa.public_key_to_pem()?;
    let private_key = rsa.private_key_to_pem()?;
    let public_key = rsa.public_key_to_pem()?;

    let sign_public_key_s = String::from_utf8(sign_public_key).unwrap();
    let sign_private_key_s = String::from_utf8(sign_private_key).unwrap();
    let public_key_s = String::from_utf8(public_key).unwrap();
    let private_key_s = String::from_utf8(private_key).unwrap();

    _ = ctsync_set_key(app.clone(), "pem_sign_public_key", &sign_public_key_s);
    _ = ctsync_set_key(app.clone(), "pem_sign_private_key", &sign_private_key_s);
    _ = ctsync_set_key(app.clone(), "pem_public_key", &public_key_s);
    _ = ctsync_set_key(app.clone(), "pem_private_key", &private_key_s);

    Ok((sign_public_key_s, public_key_s))
}
