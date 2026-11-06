#!/usr/bin/env bash
# ROM-free PortMaster dual ABI selection regression (requires only Bash).
# Fake launcher files NEVER execute: KI_R36S_ABI_SELECT_ONLY exits pre-game.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/ports/killerinstinct" "$TMP/bin"
cp "$ROOT/tools/r36s/portmaster/Killer Instinct Arcade.sh" "$TMP/ports/Killer Instinct Arcade.sh"
touch "$TMP/ports/killerinstinct/ki_play.aarch64"
touch "$TMP/ports/killerinstinct/ki_play.glibc230.aarch64"
mkdir -p "$TMP/ports/killerinstinct/libs.glibc230" "$TMP/ports/killerinstinct/libs.aarch64"
cat > "$TMP/bin/getconf" <<'SH'
#!/bin/bash
[ "$1" = GNU_LIBC_VERSION ] || exit 1
printf 'glibc %s\n' "${FAKE_GLIBC:-2.30}"
SH
cat > "$TMP/bin/uname" <<'SH'
#!/bin/bash
[ "$1" = -m ] && { echo "${FAKE_ARCH:-aarch64}"; exit 0; }
/usr/bin/uname "$@"
SH
chmod +x "$TMP/bin/getconf" "$TMP/bin/uname"
export PATH="$TMP/bin:$PATH"
export KI_R36S_ABI_SELECT_ONLY=1
run_test() {
  local version="$1" expected="$2"
  local result
  result="$(FAKE_GLIBC="$version" bash "$TMP/ports/Killer Instinct Arcade.sh" 2>&1)" || {
    printf 'Failed on supported libc %s:\n%s\n' "$version" "$result" >&2
    exit 1
  }
  grep -Fq "$expected" <<< "$result" || {
    printf 'Wrong selection for GLIBC %s:\n%s\n' "$version" "$result" >&2
    exit 2
  }
}
run_test 2.28 'ki_play.glibc230.aarch64'
run_test 2.30 'ki_play.glibc230.aarch64'
run_test 2.35 'ki_play.glibc230.aarch64'
run_test 2.37 'ki_play.glibc230.aarch64'
run_test 2.38 'ki_play.aarch64'
run_test 2.39 'ki_play.aarch64'
set +e
FAKE_GLIBC=2.27 bash "$TMP/ports/Killer Instinct Arcade.sh" >"$TMP/unsupported.log" 2>&1
code=$?
set -e
[ "$code" = 12 ] || { cat "$TMP/unsupported.log"; echo "bad old-ABI failure: $code"; exit 3; }
rm "$TMP/ports/killerinstinct/ki_play.glibc230.aarch64"
set +e
FAKE_GLIBC=2.30 bash "$TMP/ports/Killer Instinct Arcade.sh" >"$TMP/missing.log" 2>&1
code=$?
set -e
[ "$code" = 13 ] || { cat "$TMP/missing.log"; echo "missing legacy binary was not rejected ($code)"; exit 4; }
echo "R36S ArkOS4Clone/dArkOSRE GLIBC routing: PASS"
