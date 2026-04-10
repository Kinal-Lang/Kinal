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
UNINSTALL=0
PROXY_URL="${KINAL_PROXY:-}"

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
  --proxy <url>         Use a proxy for GitHub downloads. Standard proxy env vars also work.
  --uninstall           Remove the installed toolchain under --root-dir and its launcher scripts.
  --no-verify           Skip SHA256 verification.
  -h, --help            Show this help message.

Examples:
  curl -fsSL https://kinal.org/install.sh | bash
  curl -fsSL https://kinal.org/install.sh | bash -s -- --version v0.6.0
  curl -fsSL https://kinal.org/install.sh | bash -s -- --proxy http://127.0.0.1:7890
  curl -fsSL https://kinal.org/install.sh | bash -s -- --uninstall
EOF
}

note() {
  printf '[INFO] %s\n' "$*" >&2
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

can_show_progress() {
  [ -t 2 ]
}

detect_shell_rc() {
  local shell_name="${SHELL##*/}"
  case "$shell_name" in
    zsh)
      printf '%s\n' "${HOME}/.zshrc"
      ;;
    bash)
      printf '%s\n' "${HOME}/.bashrc"
      ;;
    *)
      printf '%s\n' "${HOME}/.profile"
      ;;
  esac
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

apply_proxy_settings() {
  if [ -n "${PROXY_URL:-}" ]; then
    export HTTP_PROXY="$PROXY_URL"
    export HTTPS_PROXY="$PROXY_URL"
    export ALL_PROXY="${ALL_PROXY:-$PROXY_URL}"
    export http_proxy="$PROXY_URL"
    export https_proxy="$PROXY_URL"
    export all_proxy="${all_proxy:-$PROXY_URL}"
    note "using proxy configuration for network downloads"
  elif [ -n "${HTTPS_PROXY:-}${https_proxy:-}${HTTP_PROXY:-}${http_proxy:-}${ALL_PROXY:-}${all_proxy:-}" ]; then
    note "using proxy settings from environment"
  fi
}

download_text() {
  local url="$1"
  if has_cmd curl; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      curl -fsSL --retry 3 --connect-timeout 15 \
        -H "Accept: application/vnd.github+json" \
        -H "Authorization: Bearer ${GITHUB_TOKEN}" \
        -H "User-Agent: ${USER_AGENT}" \
        "$url"
    else
      curl -fsSL --retry 3 --connect-timeout 15 \
        -H "Accept: application/vnd.github+json" \
        -H "User-Agent: ${USER_AGENT}" \
        "$url"
    fi
    return 0
  fi

  if has_cmd wget; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      wget -qO- \
        --tries=3 \
        --timeout=15 \
        --header="Accept: application/vnd.github+json" \
        --header="Authorization: Bearer ${GITHUB_TOKEN}" \
        --header="User-Agent: ${USER_AGENT}" \
        "$url"
    else
      wget -qO- \
        --tries=3 \
        --timeout=15 \
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
    local curl_args=(
      -fL
      --retry 3
      --connect-timeout 15
      -H "User-Agent: ${USER_AGENT}"
    )
    if can_show_progress; then
      curl_args+=(--progress-bar)
    else
      curl_args+=(-sS)
    fi
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      curl_args+=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
      curl "${curl_args[@]}" "$url" -o "$dest"
    else
      curl "${curl_args[@]}" "$url" -o "$dest"
    fi
    return 0
  fi

  if has_cmd wget; then
    local wget_args=(
      -O "$dest"
      --tries=3
      --timeout=15
      --header="User-Agent: ${USER_AGENT}"
    )
    if can_show_progress; then
      wget_args+=(--progress=bar:force:noscroll --show-progress)
    else
      wget_args+=(-q)
    fi
    if [ -n "${GITHUB_TOKEN:-}" ]; then
      wget_args+=(--header="Authorization: Bearer ${GITHUB_TOKEN}")
      wget "${wget_args[@]}" "$url"
    else
      wget "${wget_args[@]}" "$url"
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

  note "resolving the latest Kinal release tag"
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

wrapper_matches_target() {
  local wrapper_path="$1"
  local expected_binary="$2"
  [ -f "$wrapper_path" ] || return 1
  case "$(cat "$wrapper_path" 2>/dev/null || true)" in
    *"exec \"$expected_binary\" \"\$@\""*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

assert_safe_root_dir() {
  local candidate="$1"
  case "$candidate" in
    ""|"/"|"/root"|"/home"|"/usr"|"/usr/local"|"/opt"|"/var"|"/tmp"|"$HOME"|"$HOME/.local"|"$HOME/.local/bin")
      fail "refusing to remove an unsafe root directory: $candidate"
      ;;
  esac
}

uninstall_installation() {
  local removed_any=0
  local current_kinal="${ROOT_DIR}/current/kinal"
  local current_kinalvm="${ROOT_DIR}/current/kinalvm"
  local shell_rc

  note "uninstalling Kinal from ${ROOT_DIR}"

  if [ -e "${BIN_DIR}/kinal" ] || [ -L "${BIN_DIR}/kinal" ]; then
    if wrapper_matches_target "${BIN_DIR}/kinal" "$current_kinal"; then
      rm -f "${BIN_DIR}/kinal"
      removed_any=1
      note "removed launcher: ${BIN_DIR}/kinal"
    else
      warn "skipping ${BIN_DIR}/kinal because it was not created by this installer"
    fi
  fi

  if [ -e "${BIN_DIR}/kinalvm" ] || [ -L "${BIN_DIR}/kinalvm" ]; then
    if wrapper_matches_target "${BIN_DIR}/kinalvm" "$current_kinalvm"; then
      rm -f "${BIN_DIR}/kinalvm"
      removed_any=1
      note "removed launcher: ${BIN_DIR}/kinalvm"
    else
      warn "skipping ${BIN_DIR}/kinalvm because it was not created by this installer"
    fi
  fi

  if [ -d "$ROOT_DIR" ] || [ -L "$ROOT_DIR" ]; then
    assert_safe_root_dir "$ROOT_DIR"
    rm -rf "$ROOT_DIR"
    removed_any=1
    note "removed toolchain root: ${ROOT_DIR}"
  fi

  if [ "$removed_any" -eq 0 ]; then
    note "nothing to uninstall"
  else
    note "uninstall completed"
    shell_rc="$(detect_shell_rc)"
    printf '\n' >&2
    printf 'If your current shell still resolves "kinal", refresh it and try again:\n' >&2
    printf '  hash -r\n' >&2
    printf '  source "%s"\n' "$shell_rc" >&2
  fi
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
    --proxy)
      [ "$#" -ge 2 ] || fail "--proxy requires a value"
      PROXY_URL="$2"
      shift 2
      ;;
    --uninstall)
      UNINSTALL=1
      shift
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

BIN_DIR="$(expand_path "$BIN_DIR")"
ROOT_DIR="$(expand_path "$ROOT_DIR")"
apply_proxy_settings

if [ "$UNINSTALL" -eq 1 ]; then
  uninstall_installation
  exit 0
fi

require_tools

note "starting Kinal installer"
HOST_TAG="$(detect_host_tag)"
TAG="$(normalize_tag "$VERSION")"
VERSION_NUMBER="${TAG#v}"
RELEASE_BASE="${GITHUB_RELEASE_ROOT}/download/${TAG}"
ARCHIVE_PREFIX="Kinal-${VERSION_NUMBER}-${HOST_TAG}"

note "target release: ${TAG}"
note "detected host platform: ${HOST_TAG}"

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
  note "downloading ${candidate_name}"
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
    note "downloading ${checksum_name}"
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
note "extracting ${ARCHIVE_NAME}"
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
note "installing into ${INSTALL_DIR}"
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
    shell_rc="$(detect_shell_rc)"
    note "PATH already contains ${BIN_DIR}"
    printf '\n' >&2
    printf 'If "kinal" is still unavailable in this shell, refresh it and try again:\n' >&2
    printf '  hash -r\n' >&2
    printf '  source "%s"\n' "$shell_rc" >&2
    ;;
  *)
    shell_rc="$(detect_shell_rc)"
    printf '\n'
    printf 'Add this directory to your PATH:\n'
    printf '  export PATH="%s:$PATH"\n' "$BIN_DIR"
    printf '\n'
    printf 'To make it persistent for future shells, add it to your shell config and reload it:\n'
    printf '  echo '\''export PATH="%s:$PATH"'\'' >> "%s"\n' "$BIN_DIR" "$shell_rc"
    printf '  source "%s"\n' "$shell_rc"
    ;;
esac
