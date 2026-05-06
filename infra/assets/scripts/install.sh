#!/usr/bin/env bash
set -euo pipefail

REPO="Kinal-Lang/Kinal"
USER_AGENT="kinal-install/1.1"
GITHUB_API_ROOT="https://api.github.com"
GITHUB_RELEASE_ROOT="https://github.com/${REPO}/releases"

VERSION="latest"
VERSION_EXPLICIT=0
BIN_DIR="${HOME}/.local/bin"
ROOT_DIR="${HOME}/.local/share/kinal"
VERIFY_CHECKSUMS=1
UNINSTALL=0
PROXY_URL="${KINAL_PROXY:-}"
ZIG_EXTENSION_VERSION="0.15.2"

UPDATE=0
FORCE=0
NO_PATH=0
QUIET=0
DRY_RUN=0
NO_WRAPPER=0
LIST_VERSIONS=0
LIST_INSTALLED=0
PRINT_BIN_DIR=0
PRINT_ROOT_DIR=0
PRINT_CURRENT=0
REINSTALL=0
SET_DEFAULT=""
REMOVE_VERSION=""
KEEP_DOWNLOAD=0
TIMEOUT="${KINAL_INSTALL_TIMEOUT:-15}"
GITHUB_TOKEN_VALUE="${GITHUB_TOKEN:-}"
LINK_DIR=""
INSTALL_ZIG_EXTENSION=-1
SKIP_BUNDLE_DOWNLOAD=0
KINAL_WRAPPER_MARKER="# kinal-install-wrapper"

usage() {
  cat <<'EOF'
Install and manage the Kinal compiler on macOS or Linux.

Usage:
  install.sh [options]

Install and update:
  --version <tag>          Install a specific release tag, for example v0.6.1.
                           Defaults to the latest published release.
  --update                 Update to the latest release, or to --version if specified.
  --reinstall              Reinstall the current version. If no current version exists,
                           installs the latest release.
  --force                  Overwrite an existing version if it is already installed.
  --dry-run                Show what would be done without changing files.

Directories:
  --install-dir <dir>      Directory for launcher scripts. Defaults to ~/.local/bin.
  --root-dir <dir>         Directory for installed toolchains. Defaults to ~/.local/share/kinal.
  --no-wrapper             Do not create launcher scripts in --install-dir.
  --no-path                Do not print PATH setup instructions.

Version management:
  --list-versions          List available GitHub release versions and exit.
  --list-installed         List locally installed versions and exit.
  --set-default <tag>      Switch current Kinal to an already installed version.
  --remove-version <tag>   Remove a locally installed version.
  --link <dir>             Register a local Kinal bundle directory as a version and make it current.
                           Use with --version <name> to choose the local version name.
                           Defaults to version name "dev".

Inspection:
  --print-bin-dir          Print the launcher directory and exit.
  --print-root-dir         Print the toolchain root directory and exit.
  --current                Print the current selected version and exit.

Extensions:
  --with-extension <name>  Install an optional extension for the selected version.
                           Currently supported: zig
  --without-extension <name>
                           Skip an optional extension prompt for the selected version.
  --with-zig               Convenience alias for --with-extension zig.
  --without-zig            Convenience alias for --without-extension zig.

Network and verification:
  --proxy <url>            Use a proxy for GitHub downloads. Standard proxy env vars also work.
  --timeout <seconds>      Network connection timeout. Defaults to 15.
  --github-token <token>   Use a GitHub token for API and download requests.
                           GITHUB_TOKEN environment variable is preferred.
  --no-verify             Skip SHA256 verification.
  --keep-download          Keep downloaded archives under <root-dir>/downloads/<tag>/.

Output:
  --quiet                  Suppress informational output.
  -h, --help               Show this help message.

Examples:
  curl -fsSL https://kinal.org/install.sh | bash
  curl -fsSL https://kinal.org/install.sh | bash -s -- --update
  curl -fsSL https://kinal.org/install.sh | bash -s -- --version v0.6.1
  curl -fsSL https://kinal.org/install.sh | bash -s -- --list-versions
  curl -fsSL https://kinal.org/install.sh | bash -s -- --set-default v0.6.1
  curl -fsSL https://kinal.org/install.sh | bash -s -- --remove-version v0.6.0
  curl -fsSL https://kinal.org/install.sh | bash -s -- --with-zig
  curl -fsSL https://kinal.org/install.sh | bash -s -- --proxy http://127.0.0.1:7890
  curl -fsSL https://kinal.org/install.sh | bash -s -- --uninstall
EOF
}

note() {
  if [ "$QUIET" -eq 0 ]; then
    printf '[INFO] %s\n' "$*" >&2
  fi
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
  [ -t 2 ] && [ "$QUIET" -eq 0 ]
}

run() {
  if [ "$DRY_RUN" -eq 1 ]; then
    note "would run: $*"
  else
    "$@"
  fi
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

resolve_extension_name() {
  case "$1" in
    zig)
      printf 'zig\n'
      ;;
    *)
      return 1
      ;;
  esac
}

enable_extension() {
  local name
  name="$(resolve_extension_name "$1" 2>/dev/null || true)"
  [ -n "$name" ] || fail "unknown extension: $1"
  case "$name" in
    zig)
      INSTALL_ZIG_EXTENSION=1
      ;;
  esac
}

disable_extension() {
  local name
  name="$(resolve_extension_name "$1" 2>/dev/null || true)"
  [ -n "$name" ] || fail "unknown extension: $1"
  case "$name" in
    zig)
      INSTALL_ZIG_EXTENSION=0
      ;;
  esac
}

host_supports_zig_extension() {
  case "$1" in
    linux-*|macos-*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

zig_extension_url() {
  case "$1" in
    linux-x64)
      printf 'https://ziglang.org/download/%s/zig-x86_64-linux-%s.tar.xz\n' "$ZIG_EXTENSION_VERSION" "$ZIG_EXTENSION_VERSION"
      ;;
    linux-arm64)
      printf 'https://ziglang.org/download/%s/zig-aarch64-linux-%s.tar.xz\n' "$ZIG_EXTENSION_VERSION" "$ZIG_EXTENSION_VERSION"
      ;;
    macos-x64)
      printf 'https://ziglang.org/download/%s/zig-x86_64-macos-%s.tar.xz\n' "$ZIG_EXTENSION_VERSION" "$ZIG_EXTENSION_VERSION"
      ;;
    macos-arm64)
      printf 'https://ziglang.org/download/%s/zig-aarch64-macos-%s.tar.xz\n' "$ZIG_EXTENSION_VERSION" "$ZIG_EXTENSION_VERSION"
      ;;
    *)
      return 1
      ;;
  esac
}

zig_extension_binary_for_dir() {
  printf '%s/extensions/zig/zig\n' "$1"
}

zig_extension_installed_for_dir() {
  local install_dir="$1"
  [ -n "$install_dir" ] || return 1
  [ -x "$(zig_extension_binary_for_dir "$install_dir")" ]
}

can_prompt_tty() {
  [ -r /dev/tty ] && [ -w /dev/tty ]
}

prompt_yes_no_default_yes() {
  local prompt="$1"
  local reply=""
  while true; do
    printf '%s [Y/n] ' "$prompt" > /dev/tty
    if ! IFS= read -r reply < /dev/tty; then
      return 1
    fi
    case "$reply" in
      ''|y|Y|yes|YES|Yes)
        return 0
        ;;
      n|N|no|NO|No)
        return 1
        ;;
      *)
        printf 'Please answer y or n.\n' > /dev/tty
        ;;
    esac
  done
}

decide_optional_extensions() {
  if ! host_supports_zig_extension "$HOST_TAG"; then
    if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ]; then
      warn "the Zig extension is only supported by this installer on macOS and Linux"
      INSTALL_ZIG_EXTENSION=0
    elif [ "$INSTALL_ZIG_EXTENSION" -lt 0 ]; then
      INSTALL_ZIG_EXTENSION=0
    fi
    return 0
  fi

  if [ "$INSTALL_ZIG_EXTENSION" -ge 0 ]; then
    return 0
  fi

  if zig_extension_installed_for_dir "$INSTALL_DIR"; then
    INSTALL_ZIG_EXTENSION=1
    note "keeping the existing Zig extension for ${TAG}"
    return 0
  fi

  if [ -n "${CURRENT_VERSION:-}" ] && zig_extension_installed_for_dir "${ROOT_DIR}/versions/${CURRENT_VERSION}"; then
    INSTALL_ZIG_EXTENSION=1
    note "preserving the Zig extension from the current installation"
    return 0
  fi

  if can_prompt_tty; then
    cat > /dev/tty <<EOF

Optional extension available: zig

Recommended on ${HOST_TAG}:
- Kinal prefers Zig first for hosted Linux/macOS linking.
- Zig handles libc, sysroot, and platform linker details more reliably than raw lld.
- If Zig is missing, Kinal can fall back to lld with a warning, but the current environment still recommends Zig.

EOF
    if prompt_yes_no_default_yes "Install the Zig extension now?"; then
      INSTALL_ZIG_EXTENSION=1
    else
      INSTALL_ZIG_EXTENSION=0
    fi
    return 0
  fi

  INSTALL_ZIG_EXTENSION=0
  note "non-interactive install detected; skipping the optional Zig extension"
  warn "Zig is recommended on Linux/macOS. Re-run with --with-extension zig if you want the preferred linker toolchain installed."
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
    local curl_args=(
      -fsSL
      --retry 3
      --connect-timeout "$TIMEOUT"
      -H "Accept: application/vnd.github+json"
      -H "User-Agent: ${USER_AGENT}"
    )
    if [ -n "${GITHUB_TOKEN_VALUE:-}" ]; then
      curl_args+=(-H "Authorization: Bearer ${GITHUB_TOKEN_VALUE}")
    fi
    curl "${curl_args[@]}" "$url" || return 1
    return 0
  fi

  if has_cmd wget; then
    local wget_args=(
      -qO-
      --tries=3
      --timeout="$TIMEOUT"
      --header="Accept: application/vnd.github+json"
      --header="User-Agent: ${USER_AGENT}"
    )
    if [ -n "${GITHUB_TOKEN_VALUE:-}" ]; then
      wget_args+=(--header="Authorization: Bearer ${GITHUB_TOKEN_VALUE}")
    fi
    wget "${wget_args[@]}" "$url" || return 1
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
      --connect-timeout "$TIMEOUT"
      -H "User-Agent: ${USER_AGENT}"
    )
    if ! can_show_progress; then
      curl_args+=(-sS)
    fi
    if [ -n "${GITHUB_TOKEN_VALUE:-}" ]; then
      curl_args+=(-H "Authorization: Bearer ${GITHUB_TOKEN_VALUE}")
    fi
    curl "${curl_args[@]}" "$url" -o "$dest" || {
      rm -f "$dest"
      return 1
    }
    return 0
  fi

  if has_cmd wget; then
    local wget_args=(
      -O "$dest"
      --tries=3
      --timeout="$TIMEOUT"
      --header="User-Agent: ${USER_AGENT}"
    )
    if can_show_progress; then
      wget_args+=(--show-progress)
    else
      wget_args+=(-q)
    fi
    if [ -n "${GITHUB_TOKEN_VALUE:-}" ]; then
      wget_args+=(--header="Authorization: Bearer ${GITHUB_TOKEN_VALUE}")
    fi
    wget "${wget_args[@]}" "$url" || {
      rm -f "$dest"
      return 1
    }
    return 0
  fi

  fail "curl or wget is required"
}

require_tools() {
  has_cmd tar || fail "tar is required"
  has_cmd mktemp || fail "mktemp is required"
  has_cmd uname || fail "uname is required"
  has_cmd find || fail "find is required"
  has_cmd wc || fail "wc is required"
  has_cmd awk || fail "awk is required"
  has_cmd sed || fail "sed is required"
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
    [0-9]*)
      printf 'v%s\n' "$raw"
      ;;
    *)
      printf '%s\n' "$raw"
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
  if [ "$DRY_RUN" -eq 1 ]; then
    note "would write launcher: ${target} -> ${binary_path}"
    return 0
  fi
  cat > "$target" <<EOF
#!/usr/bin/env bash
set -euo pipefail
$KINAL_WRAPPER_MARKER
tool_root="\$(CDPATH= cd -- "\$(dirname -- "$binary_path")" && pwd -P)"
if [ -x "\$tool_root/extensions/zig/zig" ]; then
  : "\${KINAL_LINKER_ZIG:=\$tool_root/extensions/zig/zig}"
  : "\${KINAL_ZIG:=\$tool_root/extensions/zig/zig}"
  export KINAL_LINKER_ZIG KINAL_ZIG
fi
exec "$binary_path" "\$@"
EOF
  chmod 0755 "$target"
}

wrapper_matches_target() {
  local wrapper_path="$1"
  local expected_binary="$2"
  [ -f "$wrapper_path" ] || return 1
  case "$(cat "$wrapper_path" 2>/dev/null || true)" in
    *"$KINAL_WRAPPER_MARKER"*"exec \"$expected_binary\" \"\$@\""*)
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

current_tag() {
  local link_path="${ROOT_DIR}/current"
  local target
  if [ -L "$link_path" ]; then
    target="$(readlink "$link_path" || true)"
    [ -n "$target" ] || return 1
    basename "$target"
    return 0
  fi
  if [ -f "${link_path}/.kinal-install-tag" ]; then
    cat "${link_path}/.kinal-install-tag"
    return 0
  fi
  return 1
}

list_available_versions() {
  local payload
  note "fetching Kinal release list"
  payload="$(download_text "${GITHUB_API_ROOT}/repos/${REPO}/releases?per_page=100")"
  printf '%s\n' "$payload" \
    | tr '{' '\n' \
    | sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'
}

list_installed_versions() {
  local versions_dir="${ROOT_DIR}/versions"
  local cur=""
  local found=0
  local path
  local tag

  cur="$(current_tag 2>/dev/null || true)"
  if [ ! -d "$versions_dir" ]; then
    note "no installed versions found under ${versions_dir}"
    return 0
  fi

  for path in "${versions_dir}"/*; do
    [ -e "$path" ] || continue
    found=1
    tag="${path##*/}"
    if [ "$tag" = "$cur" ]; then
      printf '* %s\n' "$tag"
    else
      printf '  %s\n' "$tag"
    fi
  done

  if [ "$found" -eq 0 ]; then
    note "no installed versions found under ${versions_dir}"
  fi
}

remove_own_launcher_if_present() {
  local wrapper_path="$1"
  local expected_binary="$2"
  if [ -e "$wrapper_path" ] || [ -L "$wrapper_path" ]; then
    if wrapper_matches_target "$wrapper_path" "$expected_binary"; then
      run rm -f "$wrapper_path"
      note "removed launcher: ${wrapper_path}"
    else
      warn "skipping ${wrapper_path} because it was not created by this installer"
    fi
  fi
}

activate_installation() {
  local tag="$1"
  local install_dir="$2"
  local current_dir="${ROOT_DIR}/current"

  [ -d "$install_dir" ] || [ -L "$install_dir" ] || fail "version directory does not exist: ${install_dir}"
  [ -f "$install_dir/kinal" ] || fail "version directory does not contain the kinal executable: ${install_dir}"

  run mkdir -p "$ROOT_DIR"
  run ln -sfn "$install_dir" "$current_dir"

  if [ "$DRY_RUN" -eq 0 ]; then
    printf '%s\n' "$tag" > "${install_dir}/.kinal-install-tag" 2>/dev/null || true
  fi

  if [ "$NO_WRAPPER" -eq 0 ]; then
    run mkdir -p "$BIN_DIR"
    write_wrapper "${BIN_DIR}/kinal" "${ROOT_DIR}/current/kinal"
    if [ -f "$install_dir/kinalvm" ]; then
      write_wrapper "${BIN_DIR}/kinalvm" "${ROOT_DIR}/current/kinalvm"
    else
      remove_own_launcher_if_present "${BIN_DIR}/kinalvm" "${ROOT_DIR}/current/kinalvm"
    fi
  else
    note "skipping launcher scripts because --no-wrapper was set"
  fi

  note "current Kinal version: ${tag}"
}

print_path_instructions() {
  local path_dir
  local shell_rc

  [ "$NO_PATH" -eq 0 ] || return 0
  [ "$QUIET" -eq 0 ] || return 0

  if [ "$NO_WRAPPER" -eq 1 ]; then
    path_dir="${ROOT_DIR}/current"
  else
    path_dir="$BIN_DIR"
  fi

  shell_rc="$(detect_shell_rc)"
  case ":${PATH}:" in
    *":${path_dir}:"*)
      note "PATH already contains ${path_dir}"
      printf '\n' >&2
      printf 'If "kinal" is still unavailable in this shell, refresh it and try again:\n' >&2
      printf '  hash -r\n' >&2
      printf '  source "%s"\n' "$shell_rc" >&2
      ;;
    *)
      printf '\n'
      printf 'Add this directory to your PATH:\n'
      printf '  export PATH="%s:$PATH"\n' "$path_dir"
      printf '\n'
      printf 'To make it persistent for future shells, add it to your shell config and reload it:\n'
      printf '  echo '\''export PATH="%s:$PATH"'\'' >> "%s"\n' "$path_dir" "$shell_rc"
      printf '  source "%s"\n' "$shell_rc"
      printf '\n'
      printf 'The installer did not modify your shell config automatically.\n'
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
      run rm -f "${BIN_DIR}/kinal"
      removed_any=1
      note "removed launcher: ${BIN_DIR}/kinal"
    else
      warn "skipping ${BIN_DIR}/kinal because it was not created by this installer"
    fi
  fi

  if [ -e "${BIN_DIR}/kinalvm" ] || [ -L "${BIN_DIR}/kinalvm" ]; then
    if wrapper_matches_target "${BIN_DIR}/kinalvm" "$current_kinalvm"; then
      run rm -f "${BIN_DIR}/kinalvm"
      removed_any=1
      note "removed launcher: ${BIN_DIR}/kinalvm"
    else
      warn "skipping ${BIN_DIR}/kinalvm because it was not created by this installer"
    fi
  fi

  if [ -d "$ROOT_DIR" ] || [ -L "$ROOT_DIR" ]; then
    assert_safe_root_dir "$ROOT_DIR"
    run rm -rf "$ROOT_DIR"
    removed_any=1
    note "removed toolchain root: ${ROOT_DIR}"
  fi

  if [ "$removed_any" -eq 0 ]; then
    note "nothing to uninstall"
  else
    note "uninstall completed"
    if [ "$NO_PATH" -eq 0 ] && [ "$QUIET" -eq 0 ]; then
      shell_rc="$(detect_shell_rc)"
      printf '\n' >&2
      printf 'If your current shell still resolves "kinal", refresh it and try again:\n' >&2
      printf '  hash -r\n' >&2
      printf '  source "%s"\n' "$shell_rc" >&2
    fi
  fi
}

set_default_version() {
  local tag="$1"
  local install_dir

  tag="$(normalize_tag "$tag")"
  install_dir="${ROOT_DIR}/versions/${tag}"
  [ -d "$install_dir" ] || [ -L "$install_dir" ] || fail "version is not installed: ${tag}"

  note "switching default Kinal version to ${tag}"
  activate_installation "$tag" "$install_dir"
  print_path_instructions
}

remove_installed_version() {
  local tag="$1"
  local install_dir
  local cur=""

  tag="$(normalize_tag "$tag")"
  install_dir="${ROOT_DIR}/versions/${tag}"
  [ -e "$install_dir" ] || [ -L "$install_dir" ] || fail "version is not installed: ${tag}"

  cur="$(current_tag 2>/dev/null || true)"
  if [ "$cur" = "$tag" ] && [ "$FORCE" -ne 1 ]; then
    fail "refusing to remove current version ${tag}; use --force to remove it anyway"
  fi

  note "removing installed Kinal version ${tag}"
  run rm -rf "$install_dir"

  if [ "$cur" = "$tag" ]; then
    run rm -f "${ROOT_DIR}/current"
    remove_own_launcher_if_present "${BIN_DIR}/kinal" "${ROOT_DIR}/current/kinal"
    remove_own_launcher_if_present "${BIN_DIR}/kinalvm" "${ROOT_DIR}/current/kinalvm"
    warn "removed the current version; install or set another version before using kinal"
  fi
}

link_local_bundle() {
  local src_dir="$1"
  local tag
  local install_dir

  src_dir="$(expand_path "$src_dir")"
  [ -d "$src_dir" ] || fail "local bundle directory does not exist: ${src_dir}"
  [ -f "${src_dir}/kinal" ] || fail "local bundle directory does not contain kinal: ${src_dir}"

  if [ "$VERSION_EXPLICIT" -eq 1 ]; then
    tag="$(normalize_tag "$VERSION")"
  else
    tag="dev"
  fi

  install_dir="${ROOT_DIR}/versions/${tag}"
  if [ -e "$install_dir" ] || [ -L "$install_dir" ]; then
    [ "$FORCE" -eq 1 ] || fail "version already exists: ${tag}; use --force to overwrite it"
    run rm -rf "$install_dir"
  fi

  run mkdir -p "${ROOT_DIR}/versions"
  note "linking local Kinal bundle ${src_dir} as ${tag}"
  run ln -sfn "$src_dir" "$install_dir"
  activate_installation "$tag" "$install_dir"
  print_path_instructions
}

install_zig_extension() {
  local install_dir="$1"
  local zig_url
  local zig_archive_name
  local zig_archive_path
  local zig_extract_dir
  local zig_bundle_dir
  local entry_count
  local zig_extension_dir="${install_dir}/extensions/zig"

  zig_url="$(zig_extension_url "$HOST_TAG" 2>/dev/null || true)"
  [ -n "$zig_url" ] || fail "the Zig extension is not available for ${HOST_TAG}"

  zig_archive_name="${zig_url##*/}"
  zig_archive_path="${TMP_DIR}/${zig_archive_name}"
  note "downloading Zig ${ZIG_EXTENSION_VERSION} extension for ${HOST_TAG}"
  download_file "$zig_url" "$zig_archive_path" || fail "failed to download the Zig extension archive"

  if [ "$KEEP_DOWNLOAD" -eq 1 ]; then
    local extension_download_dir="${ROOT_DIR}/downloads/${TAG}/extensions"
    mkdir -p "$extension_download_dir"
    cp -f "$zig_archive_path" "${extension_download_dir}/${zig_archive_name}"
    note "kept Zig extension archive under ${extension_download_dir}"
  fi

  zig_extract_dir="${TMP_DIR}/zig-extension"
  rm -rf "$zig_extract_dir"
  mkdir -p "$zig_extract_dir"
  tar -xf "$zig_archive_path" -C "$zig_extract_dir"

  entry_count="$(find "$zig_extract_dir" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')"
  if [ "$entry_count" = "1" ] && [ -d "$(find "$zig_extract_dir" -mindepth 1 -maxdepth 1 -type d | head -n 1)" ]; then
    zig_bundle_dir="$(find "$zig_extract_dir" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  else
    zig_bundle_dir="$zig_extract_dir"
  fi

  [ -f "${zig_bundle_dir}/zig" ] || fail "the Zig extension archive does not contain the zig executable"

  rm -rf "$zig_extension_dir"
  mkdir -p "$zig_extension_dir"
  cp -a "$zig_bundle_dir"/. "$zig_extension_dir"/
  chmod +x "${zig_extension_dir}/zig" || true
  printf '%s\n' "$ZIG_EXTENSION_VERSION" > "${zig_extension_dir}/.kinal-extension-version" 2>/dev/null || true
  note "installed Zig extension into ${zig_extension_dir}"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --version)
      [ "$#" -ge 2 ] || fail "--version requires a value"
      VERSION="$2"
      VERSION_EXPLICIT=1
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
    --timeout)
      [ "$#" -ge 2 ] || fail "--timeout requires a value"
      TIMEOUT="$2"
      shift 2
      ;;
    --github-token)
      [ "$#" -ge 2 ] || fail "--github-token requires a value"
      GITHUB_TOKEN_VALUE="$2"
      shift 2
      ;;
    --uninstall)
      UNINSTALL=1
      shift
      ;;
    --update)
      UPDATE=1
      shift
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --no-path)
      NO_PATH=1
      shift
      ;;
    --quiet)
      QUIET=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --no-wrapper)
      NO_WRAPPER=1
      shift
      ;;
    --list-versions)
      LIST_VERSIONS=1
      shift
      ;;
    --list-installed)
      LIST_INSTALLED=1
      shift
      ;;
    --print-bin-dir)
      PRINT_BIN_DIR=1
      shift
      ;;
    --print-root-dir)
      PRINT_ROOT_DIR=1
      shift
      ;;
    --current)
      PRINT_CURRENT=1
      shift
      ;;
    --reinstall)
      REINSTALL=1
      FORCE=1
      shift
      ;;
    --set-default)
      [ "$#" -ge 2 ] || fail "--set-default requires a value"
      SET_DEFAULT="$2"
      shift 2
      ;;
    --remove-version)
      [ "$#" -ge 2 ] || fail "--remove-version requires a value"
      REMOVE_VERSION="$2"
      shift 2
      ;;
    --with-extension)
      [ "$#" -ge 2 ] || fail "--with-extension requires a value"
      enable_extension "$2"
      shift 2
      ;;
    --without-extension)
      [ "$#" -ge 2 ] || fail "--without-extension requires a value"
      disable_extension "$2"
      shift 2
      ;;
    --with-zig)
      enable_extension zig
      shift
      ;;
    --without-zig)
      disable_extension zig
      shift
      ;;
    --link)
      [ "$#" -ge 2 ] || fail "--link requires a directory"
      LINK_DIR="$2"
      shift 2
      ;;
    --no-verify)
      VERIFY_CHECKSUMS=0
      shift
      ;;
    --keep-download)
      KEEP_DOWNLOAD=1
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

case "$TIMEOUT" in
  ''|*[!0-9]*)
    fail "--timeout must be a positive integer"
    ;;
  0)
    fail "--timeout must be greater than zero"
    ;;
esac

BIN_DIR="$(expand_path "$BIN_DIR")"
ROOT_DIR="$(expand_path "$ROOT_DIR")"

if [ "$PRINT_BIN_DIR" -eq 1 ]; then
  printf '%s\n' "$BIN_DIR"
  exit 0
fi

if [ "$PRINT_ROOT_DIR" -eq 1 ]; then
  printf '%s\n' "$ROOT_DIR"
  exit 0
fi

if [ "$PRINT_CURRENT" -eq 1 ]; then
  current_tag || fail "no current Kinal version is selected"
  exit 0
fi

if [ "$LIST_INSTALLED" -eq 1 ]; then
  list_installed_versions
  exit 0
fi

apply_proxy_settings

if [ "$UNINSTALL" -eq 1 ]; then
  uninstall_installation
  exit 0
fi

if [ "$LIST_VERSIONS" -eq 1 ]; then
  list_available_versions
  exit 0
fi

if [ -n "$SET_DEFAULT" ]; then
  set_default_version "$SET_DEFAULT"
  exit 0
fi

if [ -n "$REMOVE_VERSION" ]; then
  remove_installed_version "$REMOVE_VERSION"
  exit 0
fi

if [ -n "$LINK_DIR" ]; then
  link_local_bundle "$LINK_DIR"
  exit 0
fi

require_tools

if [ "$REINSTALL" -eq 1 ] && [ "$VERSION_EXPLICIT" -eq 0 ]; then
  CURRENT_VERSION="$(current_tag 2>/dev/null || true)"
  if [ -n "$CURRENT_VERSION" ]; then
    VERSION="$CURRENT_VERSION"
  fi
fi

if [ "$UPDATE" -eq 1 ]; then
  if [ "$VERSION_EXPLICIT" -eq 1 ]; then
    note "updating Kinal to ${VERSION}"
  else
    note "updating Kinal to the latest release"
  fi
elif [ "$REINSTALL" -eq 1 ]; then
  note "reinstalling Kinal"
else
  note "starting Kinal installer"
fi

HOST_TAG="$(detect_host_tag)"
TAG="$(normalize_tag "$VERSION")"
VERSION_NUMBER="${TAG#v}"
RELEASE_BASE="${GITHUB_RELEASE_ROOT}/download/${TAG}"
ARCHIVE_PREFIX="Kinal-${VERSION_NUMBER}-${HOST_TAG}"
INSTALL_DIR="${ROOT_DIR}/versions/${TAG}"
CURRENT_VERSION="$(current_tag 2>/dev/null || true)"

note "target release: ${TAG}"
note "detected host platform: ${HOST_TAG}"

decide_optional_extensions

ZIG_EXTENSION_PRESENT=0
if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ] && zig_extension_installed_for_dir "$INSTALL_DIR"; then
  ZIG_EXTENSION_PRESENT=1
fi

if [ "$UPDATE" -eq 1 ] && [ "$CURRENT_VERSION" = "$TAG" ] && [ "$FORCE" -ne 1 ]; then
  if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ] && [ "$ZIG_EXTENSION_PRESENT" -eq 0 ]; then
    note "Kinal is already up to date: ${TAG}"
    note "keeping the installed bundle and adding the requested Zig extension"
    SKIP_BUNDLE_DOWNLOAD=1
  else
    note "Kinal is already up to date: ${TAG}"
    activate_installation "$TAG" "$INSTALL_DIR"
    print_path_instructions
    exit 0
  fi
fi

if [ -e "$INSTALL_DIR" ] || [ -L "$INSTALL_DIR" ]; then
  if [ "$FORCE" -ne 1 ]; then
    if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ] && [ "$ZIG_EXTENSION_PRESENT" -eq 0 ]; then
      note "version is already installed: ${TAG}"
      note "keeping the existing bundle and adding the requested Zig extension"
      SKIP_BUNDLE_DOWNLOAD=1
    else
      note "version is already installed: ${TAG}"
      note "activating existing version; use --force to reinstall it"
      activate_installation "$TAG" "$INSTALL_DIR"
      print_path_instructions
      exit 0
    fi
  fi
fi

if [ "$DRY_RUN" -eq 1 ]; then
  if [ "$SKIP_BUNDLE_DOWNLOAD" -eq 1 ]; then
    note "would reuse the existing bundle at: ${INSTALL_DIR}"
  else
    note "would download one of:"
    note "  ${RELEASE_BASE}/${ARCHIVE_PREFIX}.tar.xz"
    note "  ${RELEASE_BASE}/${ARCHIVE_PREFIX}.tar.gz"
    if [ "$VERIFY_CHECKSUMS" -eq 1 ]; then
      note "would verify using SHA256SUMS-${HOST_TAG}.txt or SHA256SUMS.txt"
    else
      note "would skip checksum verification"
    fi
  fi
  note "would install into: ${INSTALL_DIR}"
  if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ]; then
    note "would install optional extension: zig ${ZIG_EXTENSION_VERSION}"
  fi
  if [ "$NO_WRAPPER" -eq 0 ]; then
    note "would create launchers under: ${BIN_DIR}"
  else
    note "would not create launcher scripts"
  fi
  exit 0
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if [ "$SKIP_BUNDLE_DOWNLOAD" -eq 0 ]; then
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

  CHECKSUM_FILE=""
  if [ "$VERIFY_CHECKSUMS" -eq 1 ]; then
    for checksum_name in "SHA256SUMS-${HOST_TAG}.txt" "SHA256SUMS.txt"; do
      candidate_checksum="${TMP_DIR}/${checksum_name}"
      note "downloading ${checksum_name}"
      if download_file "${RELEASE_BASE}/${checksum_name}" "$candidate_checksum"; then
        CHECKSUM_FILE="$candidate_checksum"
        break
      fi
    done

    [ -n "$CHECKSUM_FILE" ] || fail "failed to download a checksum manifest for ${TAG}"

    expected_checksum="$(awk -v name="$ARCHIVE_NAME" '$2 == name { print $1; exit }' "$CHECKSUM_FILE")"
    [ -n "$expected_checksum" ] || fail "could not find ${ARCHIVE_NAME} in ${CHECKSUM_FILE##*/}"

    actual_checksum="$(compute_sha256 "$ARCHIVE_PATH" || true)"
    [ -n "$actual_checksum" ] || fail "sha256sum or shasum is required for checksum verification"

    if [ "$expected_checksum" != "$actual_checksum" ]; then
      fail "checksum verification failed for ${ARCHIVE_NAME}"
    fi
    note "verified checksum for ${ARCHIVE_NAME}"
  else
    warn "skipping checksum verification"
  fi

  if [ "$KEEP_DOWNLOAD" -eq 1 ]; then
    DOWNLOAD_DIR="${ROOT_DIR}/downloads/${TAG}"
    mkdir -p "$DOWNLOAD_DIR"
    cp -f "$ARCHIVE_PATH" "${DOWNLOAD_DIR}/${ARCHIVE_NAME}"
    if [ -n "$CHECKSUM_FILE" ]; then
      cp -f "$CHECKSUM_FILE" "${DOWNLOAD_DIR}/${CHECKSUM_FILE##*/}"
    fi
    note "kept downloaded files under ${DOWNLOAD_DIR}"
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

  mkdir -p "${ROOT_DIR}/versions"
  if [ -e "$INSTALL_DIR" ] || [ -L "$INSTALL_DIR" ]; then
    [ "$FORCE" -eq 1 ] || fail "version already exists: ${TAG}; use --force to overwrite it"
    note "overwriting existing version: ${TAG}"
    rm -rf "$INSTALL_DIR"
  fi

  mkdir -p "$INSTALL_DIR"
  note "installing into ${INSTALL_DIR}"
  cp -a "$BUNDLE_DIR"/. "$INSTALL_DIR"/
  printf '%s\n' "$TAG" > "${INSTALL_DIR}/.kinal-install-tag" 2>/dev/null || true

  chmod +x "$INSTALL_DIR/kinal" || true
  if [ -f "$INSTALL_DIR/kinalvm" ]; then
    chmod +x "$INSTALL_DIR/kinalvm" || true
  fi
fi

if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ]; then
  install_zig_extension "$INSTALL_DIR"
fi

activate_installation "$TAG" "$INSTALL_DIR"

note "installed Kinal ${TAG} for ${HOST_TAG}"
note "toolchain root: ${INSTALL_DIR}"
if [ "$NO_WRAPPER" -eq 0 ]; then
  note "command shims: ${BIN_DIR}"
fi
if [ "$INSTALL_ZIG_EXTENSION" -eq 1 ]; then
  note "installed optional extension: zig ${ZIG_EXTENSION_VERSION}"
  if [ "$NO_WRAPPER" -eq 1 ]; then
    warn "wrappers are disabled; export KINAL_LINKER_ZIG=\"$(zig_extension_binary_for_dir "$INSTALL_DIR")\" before invoking kinal directly if you want to use the installed Zig extension"
  fi
fi

if [ "$NO_WRAPPER" -eq 0 ]; then
  if "${BIN_DIR}/kinal" --help >/dev/null 2>&1; then
    note "kinal launcher is working"
  else
    warn "installation finished, but 'kinal --help' did not complete successfully"
  fi
else
  if "${ROOT_DIR}/current/kinal" --help >/dev/null 2>&1; then
    note "kinal executable is working"
  else
    warn "installation finished, but 'kinal --help' did not complete successfully"
  fi
fi

print_path_instructions
