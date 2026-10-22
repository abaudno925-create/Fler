#!/usr/bin/env bash
#
# Pulls the real Counter-Strike 2 agent meshes out of the local game install
# and converts them to glTF for the ESP preview.
#
# The meshes belong to Valve, so they are not shipped with this repository:
# run this once on a machine that has CS2 installed.
#
#   ./tools/fetch_models.sh
#
# Result: assets/ct.glb (counter terrorist) and assets/t.glb (terrorist).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSETS="$ROOT/assets"
WORK="$ROOT/.vrf"
VRF_VERSION="20.1"

find_game() {
  local candidates=(
    "$HOME/.steam/steam/steamapps/common/Counter-Strike Global Offensive"
    "$HOME/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive"
    "/mnt/c/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive"
  )
  for path in "${candidates[@]}"; do
    if [ -f "$path/game/csgo/pak01_dir.vpk" ]; then
      echo "$path"
      return 0
    fi
  done
  return 1
}

if ! GAME="$(find_game)"; then
  echo "Could not find a Counter-Strike 2 install (looked in the usual Steam libraries)." >&2
  exit 1
fi
echo "game:  $GAME"

mkdir -p "$ASSETS" "$WORK"

if [ ! -x "$WORK/Source2Viewer-CLI" ]; then
  echo "fetching ValveResourceFormat $VRF_VERSION"
  curl -sSL -o "$WORK/vrf.zip" \
    "https://github.com/ValveResourceFormat/ValveResourceFormat/releases/download/$VRF_VERSION/cli-linux-x64.zip"
  unzip -o -q "$WORK/vrf.zip" -d "$WORK"
  chmod +x "$WORK/Source2Viewer-CLI"
fi

VRF="$WORK/Source2Viewer-CLI"
MODELS=(
  "agents/models/ctm_st6/ctm_st6_variante.vmdl_c:ct"
  "agents/models/tm_phoenix/tm_phoenix.vmdl_c:t"
)

for entry in "${MODELS[@]}"; do
  path="${entry%%:*}"
  name="${entry##*:}"
  raw="$WORK/${path##*/}"

  echo "extracting $path"
  "$VRF" -i "$GAME/game/csgo/pak01_dir.vpk" --vpk_filepath "$path" -o "$WORK" >/dev/null

  echo "converting $name"
  "$VRF" -i "$raw" -d --gltf_export_format glb -o "$ASSETS/$name.glb" >/dev/null
  rm -f "$raw"
done

echo
ls -lh "$ASSETS"
echo
echo "done — the preview picks the meshes up from $ASSETS"
