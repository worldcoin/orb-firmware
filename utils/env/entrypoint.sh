#!/bin/bash --login
# The --login should ensure the bash configuration is loaded (e.g. ~/.bash_profile)
# allowing us to invoke conda as `conda init bash` writes init scripts to ~/.bash_profile

set -euo pipefail
conda activate worldcoin
