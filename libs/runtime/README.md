# Native Runtime Layout

This directory supplies the C ABI leaves linked into hosted native Kinal programs. It is separate from KinalVM and from the Kinal source packages under `libs/std`.

## Source and Distribution

- `src/kn_runtime.c`: hosted allocation/GC, strings, collections, process and host-service support.
- `src/kn_chkstk.c`: target stack-probe support.
- `src/kn_request_civet.c`, `src/kn_web_civet.c`: HTTP client/server bridges to bundled CivetWeb.
- `include/kn/platform.h`: platform definitions.
- `include/kn/freestanding.h`: hook ABI for custom freestanding runtimes.

Host bundles retain `runtime/src` and `runtime/include`, and stage native objects/libraries under `runtime/<platform>`. Standard packages may carry additional native assets. Keep the full bundle layout; copying only `kinal.exe` loses compiler/linker/runtime dependencies. Use `python x.py dev --release` to rebuild the local toolchain, or `python x.py dist` for distribution output; see [build](../../dev/build.md) and [release](../../dev/releasing.md) instructions.

## Hosted Ownership

Managed strings, arrays, class instances and closure environments use the hosted runtime's GC. Generated code registers roots for locals and process-wide globals/static fields; GC also conservatively scans active native stack memory. GC is not a resource-disposal protocol: close process handles and dynamic libraries explicitly, and follow FFI allocation ownership. Do not free GC storage with a C allocator.

`IO.Memory` is implemented as self-contained Kinal byte loops in `IO.Core`; it must remain usable without importing the hosted GC implementation. See [memory management](../../docs/advanced/memory-management.md).

## Freestanding ABI

| Mode | Provider responsibility |
|------|-------------------------|
| `--env freestanding --runtime none` | Compiler-emitted core helpers; no managed allocation. Bounded scratch strings have temporary lifetime. |
| `--env freestanding --runtime alloc` | Allocator/string hooks and frame/root hooks; a non-collecting provider may implement frame/root operations as no-ops. |
| `--env freestanding --runtime gc` | Full allocation, collector and runtime hook implementation. |

Using collections, exceptions or async adds the corresponding `__kn_*` hook requirements. The compiler does not bundle a production bare-metal allocator or collector. The header specifies ABI contracts, not a ready-made board runtime. Startup, stack/section initialization, linker script and boot protocol belong to the target integration. See [freestanding development](../../docs/advanced/freestanding.md).

## HTTP Assets

`IO.Request` and `IO.Web` use the CivetWeb native bridges. HTTPS needs loadable OpenSSL 3 libraries matching the architecture. The current outbound client does not validate server certificates and buffers the entire response body; its [public documentation](../../docs/stdlib/request.md) records these boundaries. Native asset build/staging is handled by `infra/scripts/x/runtime_build.py` and release packaging, not by a user import downloading dependencies.

## Validation

`python x.py test --release --full` runs native/runtime/package integration checks. Select the matching `KINAL_VM_PATH` when validating both backends; see [testing](../../dev/testing.md). `tests/stdlib_coverage.json` records ownership of builtin and package behavior tests. Preserve the global-root/worker-GC regressions when changing hosted allocation or Web dispatch lifetime.
