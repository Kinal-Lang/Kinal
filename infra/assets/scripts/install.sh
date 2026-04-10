#!/usr/bin/env bash
set -euo pipefail

REPO="Kinal-Lang/Kinal"
USER_AGENT="kinal-install/1.0"
GITHUB_API_ROOT="https://api.github.com"
GITHUB_RELEASE_ROOT="https://github.com/${REPO}/releases"

VERSION="latest"
BIN_DIR="${HOME}/.local/bin"
ROOT_DIR="${HOME}/.local/share/kinal"
VERIFY_CHECKSUMS=1

usage() {
  cat <<'EOF'
Install the Kinal compiler on macOS or Linux.

Usage:
  install.sh [options]

Options:
  --version <tag>       Install a specific release tag, for example v0.6.0.
                        Defaults to the latest published release.
  --install-dir <dir>   Directory for the launcher scripts. Defaults to ~/.local/bin
  --root-dir <dir>      Directory for the installed toolchain. Defaults to ~/.local/share/kinal
  --no-verify           Skip SHA256 verification.
  -h, --help            Show this help message.

Examples:
  curl -fsSL https://kinal.org/install.sh | bash
  curl -fsSL https://kinal.org/install.sh | bash -s -- --version v0.6.0
EOF
}

note() {
  printf '[INFO] %s\n' "$*"
}

warn() {
  printf '[WARN] %s\n' "$*" >&2
}

fail() {
  printf '[ERROR] %s\n' "$*" >&2
  exit 1
}

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

expand_path() {
  case "$1" in
    "~")
      printf '%s\n' "$HOME"
      ;;
    "~/"*)
      printf '%s/%s\n' "$HOME" "${1#~/}"
      ;;
    *)
      printf '%s\n' "$1"
      ;;
  esac
}

download_text() {
  local url="$1"
  if has_cmd curl; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      curl -fsSL \
        -H "Accept: application/vnd.github+json" \
        -H "Authorization: Bearer ${GITHUB_TOKEN}" \
        -H "User-Agent: ${USER_AGENT}" \
        "$url"
    else
      curl -fsSL \
        -H "Accept: application/vnd.github+json" \
        -H "User-Agent: ${USER_AGENT}" \
        "$url"
    fi
    return 0
  fi

  if has_cmd wget; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      wget -qO- \
        --header="Accept: application/vnd.github+json" \
        --header="Authorization: Bearer ${GITHUB_TOKEN}" \
        --header="User-Agent: ${USER_AGENT}" \
        "$url"
    else
      wget -qO- \
        --header="Accept: application/vnd.github+json" \
        --header="User-Agent: ${USER_AGENT}" \
        "$url"
    fi
    return 0
  fi

  fail "curl or wget is required"
}

download_file() {
  local url="$1"
  local dest="$2"
  if has_cmd curl; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      curl -fsSL \
        -H "Authorization: Bearer ${GITHUB_TOKEN}" \
        -H "User-Agent: ${USER_AGENT}" \
        "$url" \
        -o "$dest"
    else
      curl -fsSL \
        -H "User-Agent: ${USER_AGENT}" \
        "$url" \
        -o "$dest"
    fi
    return 0
  fi

  if has_cmd wget; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      wget -qO "$dest" \
        --header="Authorization: Bearer ${GITHUB_TOKEN}" \
        --header="User-Agent: ${USER_AGENT}" \
        "$url"
    else
      wget -qO "$dest" \
        --header="User-Agent: ${USER_AGENT}" \
        "$url"
    fi
    return 0
  fi

  fail "curl or wget is required"
}

require_tools() {
  has_cmd tar || fail "tar is required"
  has_cmd mktemp || fail "mktemp is required"
  has_cmd uname || fail "uname is required"
}

detect_host_tag() {
  local os_name
  local arch_name
  local os_tag
  local arch_tag

  os_name="$(uname -s)"
  arch_name="$(uname -m)"

  case "$os_name" in
    Linux)
      os_tag="linux"
      ;;
    Darwin)
      os_tag="macos"
      ;;
    *)
      fail "unsupported operating system: $os_name"
      ;;
  esac

  case "$arch_name" in
    x86_64|amd64)
      arch_tag="x64"
      ;;
    arm64|aarch64)
      arch_tag="arm64"
      ;;
    *)
      fail "unsupported architecture: $arch_name"
      ;;
  esac

  printf '%s-%s\n' "$os_tag" "$arch_tag"
}

resolve_latest_tag() {
  local payload
  local tag

  payload="$(download_text "${GITHUB_API_ROOT}/repos/${REPO}/releases/latest")"
  tag="$(printf '%s' "$payload" | tr -d '\r\n' | sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')"
  [ -n "$tag" ] || fail "failed to resolve the latest release tag"
  printf '%s\n' "$tag"
}

normalize_tag() {
  local raw="$1"
  if [ -z "$raw" ] || [ "$raw" = "latest" ]; then
    resolve_latest_tag
    return 0
  fi
  case "$raw" in
    v*)
      printf '%s\n' "$raw"
      ;;
    *)
      printf 'v%s\n' "$raw"
      ;;
  esac
}

compute_sha256() {
  local file="$1"
  if has_cmd sha256sum; then
    sha256sum "$file" | awk '{print $1}'
    return 0
  fi
  if has_cmd shasum; then
    shasum -a 256 "$file" | awk '{print $1}'
    return 0
  fi
  return 1
}

write_wrapper() {
  local target="$1"
  local binary_path="$2"
  cat > "$target" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec "$binary_path" "\$@"
EOF
  chmod 0755 "$target"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --version)
      [ "$#" -ge 2 ] || fail "--version requires a value"
      VERSION="$2"
      shift 2
      ;;
    --install-dir)
      [ "$#" -ge 2 ] || fail "--install-dir requires a value"
      BIN_DIR="$2"
      shift 2
      ;;
    --root-dir)
      [ "$#" -ge 2 ] || fail "--root-dir requires a value"
      ROOT_DIR="$2"
      shift 2
      ;;
    --no-verify)
      VERIFY_CHECKSUMS=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

require_tools

BIN_DIR="$(expand_path "$BIN_DIR")"
ROOT_DIR="$(expand_path "$ROOT_DIR")"
HOST_TAG="$(detect_host_tag)"
TAG="$(normalize_tag "$VERSION")"
VERSION_NUMBER="${TAG#v}"
RELEASE_BASE="${GITHUB_RELEASE_ROOT}/download/${TAG}"
ARCHIVE_PREFIX="Kinal-${VERSION_NUMBER}-${HOST_TAG}"

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

ARCHIVE_PATH=""
ARCHIVE_NAME=""
for ext in tar.xz tar.gz; do
  candidate_name="${ARCHIVE_PREFIX}.${ext}"
  candidate_path="${TMP_DIR}/${candidate_name}"
  candidate_url="${RELEASE_BASE}/${candidate_name}"
  if download_file "$candidate_url" "$candidate_path"; then
    ARCHIVE_NAME="$candidate_name"
    ARCHIVE_PATH="$candidate_path"
    break
  fi
done

[ -n "$ARCHIVE_PATH" ] || fail "failed to download a release archive for ${HOST_TAG}"

if [ "$VERIFY_CHECKSUMS" -eq 1 ]; then
  checksum_file=""
  for checksum_name in "SHA256SUMS-${HOST_TAG}.txt" "SHA256SUMS.txt"; do
    candidate_checksum="${TMP_DIR}/${checksum_name}"
    if download_file "${RELEASE_BASE}/${checksum_name}" "$candidate_checksum"; then
      checksum_file="$candidate_checksum"
      break
    fi
  done

  [ -n "$checksum_file" ] || fail "failed to download a checksum manifest for ${TAG}"

  expected_checksum="$(awk -v name="$ARCHIVE_NAME" '$2 == name { print $1; exit }' "$checksum_file")"
  [ -n "$expected_checksum" ] || fail "could not find ${ARCHIVE_NAME} in ${checksum_file##*/}"

  actual_checksum="$(compute_sha256 "$ARCHIVE_PATH" || true)"
  [ -n "$actual_checksum" ] || fail "sha256sum or shasum is required for checksum verification"

  if [ "$expected_checksum" != "$actual_checksum" ]; then
    fail "checksum verification failed for ${ARCHIVE_NAME}"
  fi
  note "verified checksum for ${ARCHIVE_NAME}"
else
  warn "skipping checksum verification"
fi

EXTRACT_DIR="${TMP_DIR}/extract"
mkdir -p "$EXTRACT_DIR"
tar -xf "$ARCHIVE_PATH" -C "$EXTRACT_DIR"

entry_count="$(find "$EXTRACT_DIR" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')"
if [ "$entry_count" = "1" ] && [ -d "$(find "$EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)" ]; then
  BUNDLE_DIR="$(find "$EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
else
  BUNDLE_DIR="$EXTRACT_DIR"
fi

[ -f "$BUNDLE_DIR/kinal" ] || fail "release bundle does not contain the kinal executable"

INSTALL_DIR="${ROOT_DIR}/versions/${TAG}"
mkdir -p "${ROOT_DIR}/versions" "$BIN_DIR"
rm -rf "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
cp -a "$BUNDLE_DIR"/. "$INSTALL_DIR"/

chmod +x "$INSTALL_DIR/kinal" || true
if [ -f "$INSTALL_DIR/kinalvm" ]; then
  chmod +x "$INSTALL_DIR/kinalvm" || true
fi

ln -sfn "$INSTALL_DIR" "${ROOT_DIR}/current"
write_wrapper "${BIN_DIR}/kinal" "${ROOT_DIR}/current/kinal"
if [ -f "$INSTALL_DIR/kinalvm" ]; then
  write_wrapper "${BIN_DIR}/kinalvm" "${ROOT_DIR}/current/kinalvm"
fi

note "installed Kinal ${TAG} for ${HOST_TAG}"
note "toolchain root: ${INSTALL_DIR}"
note "command shims: ${BIN_DIR}"

if "${BIN_DIR}/kinal" --help >/dev/null 2>&1; then
  note "kinal launcher is working"
else
  warn "installation finished, but 'kinal --help' did not complete successfully"
fi

case ":${PATH}:" in
  *":${BIN_DIR}:"*)
    note "PATH already contains ${BIN_DIR}"
    ;;
  *)
    printf '\n'
    printf 'Add this directory to your PATH:\n'
    printf '  export PATH="%s:$PATH"\n' "$BIN_DIR"
    ;;
esac
