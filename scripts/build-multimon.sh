#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bundle_dir=$(realpath -m -- "${1:-$source_dir/dist/flameshot-15-multimon}")
build_dir=${FLAMESHOT_MULTIMON_BUILD_DIR:-$source_dir/build-multimon}
export GIT_HASH
GIT_HASH=$(git -C "$source_dir" rev-parse --short HEAD)
cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$bundle_dir" \
    -DUSE_PORTABLE_CONFIG=ON -DDISABLE_UPDATE_CHECKER=ON \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build "$build_dir" --target flameshot --parallel "${BUILD_JOBS:-4}"
install -Dm755 "$build_dir/src/flameshot" "$bundle_dir/bin/flameshot"
install -Dm755 "$source_dir/scripts/run-multimon.sh" "$bundle_dir/flameshot-multimon"
install -Dm644 "$source_dir/LICENSE" "$bundle_dir/LICENSE"
install -Dm644 "$source_dir/docs/multimonitor.md" "$bundle_dir/README.md"
install -Dm644 "$source_dir/data/img/app/org.flameshot.Flameshot.svg" "$bundle_dir/flameshot.svg"
mkdir -p "$bundle_dir/bin/translations"
cp "$build_dir"/src/*.qm "$bundle_dir/bin/translations/"
printf 'Bundle ready: %s\n' "$bundle_dir"

