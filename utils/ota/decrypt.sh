#!/bin/sh

set -e

BASEDIR=$(dirname $0)

# --batch to prevent interactive command
# --yes to assume "yes" for questions

# for all files ending in .pem.gpg
for file in ${BASEDIR}/*.pem.gpg; do
  # remove the .gpg extension
  filename="${file%.*}"
  # decrypt with the passphrase
  echo "Decrypting $file to $filename"
  gpg --quiet --batch --yes --decrypt --passphrase="$FW_KEYS_PASSPHRASE" \
  --output $filename $file
done
