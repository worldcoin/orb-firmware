#!/bin/sh

set -e

BASEDIR=$(dirname $0)

# --batch to prevent interactive command
# --yes to assume "yes" for questions
gpg --quiet --batch --yes --decrypt --passphrase="$FW_KEYS_PASSPHRASE" \
--output ${BASEDIR}/enc-ec256-priv.pem ${BASEDIR}/enc-ec256-priv.pem.gpg

gpg --quiet --batch --yes --decrypt --passphrase="$FW_KEYS_PASSPHRASE" \
--output ${BASEDIR}/enc-ec256-pub.pem ${BASEDIR}/enc-ec256-pub.pem.gpg

gpg --quiet --batch --yes --decrypt --passphrase="$FW_KEYS_PASSPHRASE" \
--output ${BASEDIR}/root-ec-p256.pem ${BASEDIR}/root-ec-p256.pem.gpg

echo "Successfully decrypted keys"
