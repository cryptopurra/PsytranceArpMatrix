#!/usr/bin/env bash
# Crea il repository su GitHub e fa il primo push.
# Requisiti: git + GitHub CLI (https://cli.github.com) autenticata con `gh auth login`.
#
#   ./scripts/publish_to_github.sh [nome-repo] [public|private]
set -euo pipefail

REPO="${1:-PsytranceArpMatrix}"
VIS="${2:-private}"

cd "$(dirname "$0")/.."

if [ ! -d .git ]; then
  git init -b main
  git add -A
  git commit -m "Initial commit: Psytrance Arp Matrix"
fi

gh repo create "$REPO" --"$VIS" --source . --remote origin --push \
  --description "Psytrance multi-channel MIDI arp / sequencer matrix (JUCE VST3/AU/Standalone)"

echo
echo "Fatto. Per pubblicare una release con i binari compilati dalla CI:"
echo "  git tag v0.1.0 && git push origin v0.1.0"
