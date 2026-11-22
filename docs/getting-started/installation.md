# Installation Guide

This guide installs the C-based Kinal toolchain. Source documentation describes the current checkout; an older release can lack newer CLI options or bytecode instructions. Check the versions of the tools you actually run.

## Platforms and Dependencies

Official host workflows build Windows, Linux, and macOS bundles for x64 and ARM64. Choose the archive matching both your operating system and CPU architecture. OS/libc/SDK compatibility depends on the release's build environment; the platform name alone is not a promise of a particular minimum system version.

Keep the full extracted bundle together. The compiler uses LLVM libraries; native builds also need the selected linker, target runtime and system libraries. Packages can add native dependencies. A generated native executable does not need the compiler, but can still require CRT, system or package libraries. See [Backend Support](../advanced/backend-support.md).

## Linux and macOS Installer

Install the latest published release:

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

The script defaults to `~/.local/share/kinal` for installed toolchains and `~/.local/bin` for launcher scripts. Add the latter to your shell's `PATH` if needed:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Add that export to your shell's startup file to persist it. Installer options affect the installer process; they do not automatically change an already running parent shell.

Inspect available and installed versions before choosing a specific release:

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --list-versions
curl -fsSL https://kinal.org/install.sh | bash -s -- --list-installed
curl -fsSL https://kinal.org/install.sh | bash -s -- --current
curl -fsSL https://kinal.org/install.sh | bash -s -- --status
```

| Option | Purpose |
|--------|---------|
| `--version <tag>` | Install a specific published release tag |
| `--update` | Update to latest or the explicitly selected tag |
| `--set-default <tag>` | Select an already installed version |
| `--remove-version <tag>` | Remove one installed version |
| `--link <dir>` | Register a local complete toolchain directory; default local name is `dev` |
| `--root-dir <dir>` | Override installed-toolchain root |
| `--install-dir <dir>` | Override launcher directory |
| `--dry-run` | Show planned actions without modifying installation files |
| `--help` | Full installer option reference |

Use the actual tag from `--list-versions`; a source `VERSION` value does not prove that a release with that tag has been published. Installer SHA-256 verification is enabled unless explicitly disabled.

### Optional Zig Toolchain

Hosted Linux/macOS linking prefers Zig when available. The installer supports an optional Zig extension and can prompt interactively. Select it explicitly for unattended use:

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --with-zig
# Alternative: skip the optional toolchain
curl -fsSL https://kinal.org/install.sh | bash -s -- --without-zig
```

`--with-extension zig` and `--without-extension zig` are equivalent forms. The version launcher exports the installed Zig path. Without Zig, the driver can fall back to LLD, but success still depends on available platform libraries and linker configuration. See [Linking](../advanced/linking.md).

### Proxies and Custom Directories

The script supports standard `HTTPS_PROXY`, `HTTP_PROXY`, `ALL_PROXY`, and `NO_PROXY` settings. `--proxy` controls installer downloads; fetching the script through `curl` needs its own proxy environment when necessary:

```bash
HTTPS_PROXY=http://127.0.0.1:7890 \
curl -fsSL https://kinal.org/install.sh | \
bash -s -- --proxy http://127.0.0.1:7890
```

For private GitHub API access, prefer the installer's supported `GITHUB_TOKEN` environment variable over a token embedded in shell history. Do not put credentials into project files or documentation examples.

To uninstall using the defaults:

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --uninstall
```

If installed with custom directories, supply the same `--root-dir` and `--install-dir` when managing/removing that installation. `--remove-version` is the separate operation for deleting one selected toolchain version.

## Manual Installation, Including Windows

1. Download the matching asset from [Kinal Releases](https://github.com/Kinal-Lang/Kinal/releases).
2. Extract the entire archive into a permanent directory.
3. Add that directory, or its supplied launcher location, to `PATH`.
4. Open a new terminal and verify the selected tools.

Do not move only `kinal`/`kinal.exe` into a different directory. Its LLVM libraries, linker tools, standard packages and runtime files are part of the installation. Preserve relative paths when relocating the bundle. On Unix, retain executable permissions on the supplied tools.

A typical bundle includes:

```text
kinal/
├── kinal[.exe]
├── kinalvm[.exe]
├── kinal-lsp-server[.exe]
├── VERSION
├── locales/
├── llvm/
├── linker/
├── runtime/
├── stdpkg/
└── docs/
```

LLVM shared libraries and other host-specific files can also be beside the executables. Exact file names depend on the host release.

## Verify Installation

```bash
kinal --version --verbose
kinal --help
kinalvm --version
```

Standalone `kinalvm` supports execution, version reporting and `--disasm`; the current VM does not implement `--help`. Use `kinal vm --help` for compiler-side VM command help.

Create `hello.kn`:

```kinal
Unit Hello;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

```bash
kinal run hello.kn
kinal vm build hello.kn -o hello.knc
kinalvm hello.knc
```

Both paths should print `Hello, Kinal!`. Version/help checks only test tool startup; this additional program tests source compilation, native linking and bytecode execution.

## Diagnostic Language

```bash
kinal build --lang en hello.kn
kinal build --lang zh hello.kn
```

`--lang` can appear before or after a subcommand. The Chinese locale is normally loaded from `locales/zh-CN.json` beside the compiler; `--locale-file` selects an explicit custom file. Keep the locale directory with the executable.

## VS Code

Install the official [Kinal extension](https://marketplace.visualstudio.com/items?itemName=kinal.kinal). It uses `kinal` for compilation/formatting and `kinal-lsp-server` for language services. If they are not discoverable through `PATH`, configure `kinal.path` and `kinal.serverPath` with full executable paths.

The extension supports `.kn`, `.kinal`, and `.knproj`, plus **Kinal: New Project File**. Project analysis is driven by the nearest `kinal.knproj`; see [Project Files](../cli/project-files.md) and the [extension README](https://github.com/Kinal-Lang/Kinal-VSCode).

## Building the C Compiler from Source

The project uses Python 3.10+, CMake 3.20+, Ninja, a C/C++ host compiler, and LLVM/Clang. The repository's current host workflows select LLVM 21; use the repository bootstrap/configuration rather than assuming an arbitrary older LLVM is compatible.

```bash
python x.py fetch llvm-prebuilt
python x.py doctor
python x.py dev --release
python x.py test --release
```

The staged toolchain is under `out/stage/host-release/`. `python x.py dist` creates a distribution. See the [Build Guide](../../dev/build.md) for system LLVM selection and platform prerequisites.

## Troubleshooting

| Symptom | Action |
|---------|--------|
| Command not found | Check PATH, open a new terminal, or use the full executable path |
| An unexpected compiler version runs | Inspect `where.exe kinal` on Windows or `command -v kinal` on Unix; check installer current version |
| Missing LLVM DLL/shared library | Restore the complete matching bundle and its directory layout |
| Compiler starts, native link fails | Check selected linker, runtime files, target libraries and `--show-link` |
| New `.knc` rejected | Update the VM with the compiler; current writer emits format 3 |
| Optional HTTP/UI package fails | Check that package's platform support and native dependency notes |
| Editor differs from CLI | Check executable paths, LSP profile and project source scope |

## Next Steps

- [Hello World](hello-world.md)
- [Project Structure](project-structure.md)
- [Compiler CLI](../cli/compiler.md)
