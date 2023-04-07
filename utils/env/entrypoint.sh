#!/bin/bash --login
. "$HOME"/.bash_profile
# The --login should ensure the bash configuration is loaded,
# which would make conda available but apparently that's not
# enough to load conda

set -euo pipefail
conda activate worldcoin
