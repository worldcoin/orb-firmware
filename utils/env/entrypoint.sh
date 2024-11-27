# Load the miniconda environment
# Can be used as `environment file` in CLion when setting up the toolchain

# reset PATH variable
export PATH=$(getconf PATH)

# load env based on initial shell
if [ -n "${ZSH_NAME:-}" ]; then
    # see `man zsh`
    . "$HOME"/.zshenv
    . /etc/zshrc
    . "$HOME"/.zshrc
elif [ -n "${BASH:-}" ]; then
    . "$HOME"/.bashrc
fi

set -euo pipefail
conda activate worldcoin
