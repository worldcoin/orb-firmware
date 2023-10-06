# load env based on initial shell
if [ -n "${ZSH_NAME:-}" ]; then
    . "$HOME"/.zshrc
elif [ -n "${BASH:-}" ]; then
    . "$HOME"/.bashrc
fi

set -euo pipefail
conda activate worldcoin
