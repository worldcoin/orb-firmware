#include <bootutil/sign_key.h>
#include <mcuboot_config/mcuboot_config.h>

#ifdef CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256
/* Signature keys
 * - private key provided using KConfig BOOT_SIGNATURE_KEY_FILE
 * - public key is auto-generated during build, see `autogen-pubkey.c` in
 * ZEPHYR_BINARY_DIR and included into the final image, see below
 */
extern const unsigned char ecdsa_pub_key[];
extern unsigned int ecdsa_pub_key_len;

const struct bootutil_key bootutil_keys[] = {
    {
        .key = ecdsa_pub_key,
        .len = &ecdsa_pub_key_len,
    },
};
const int bootutil_key_cnt = 1;
#endif

#ifdef CONFIG_BOOT_ENCRYPT_EC256

/*
 * Private key based on PEM file, processed by imgtool to create C variables
 * $ imgtool getpriv -k path/to/private_key.pem --minimal >
 * share/autogen-enc-privkey.c See CMakeLists.txt & share/autogen-enc-privkey.c
 */
extern const unsigned char enc_priv_key[];
extern unsigned int enc_priv_key_len;

const struct bootutil_key bootutil_enc_key = {
    .key = enc_priv_key,
    .len = &enc_priv_key_len,
};

#endif
