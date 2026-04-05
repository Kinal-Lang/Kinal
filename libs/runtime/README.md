# Runtime Layout

`runtime/` contains the hosted C runtime shim that compiled Kinal programs link against.

## Directory

- `runtime/src/`
  - `kn_runtime.c`
  - `kn_chkstk.c`
- `runtime/include/kn/`
  - `platform.h`
  - `freestanding.h`

## Packaging

Host bundles mirror this layout under `runtime/` and add one host-specific directory:

- `runtime/src/`
- `runtime/include/`
- `runtime/<host>/`

`runtime/<host>/` contains the prebuilt object files and import libraries used by the host linker flow.

## Freestanding

Kinal now has a generic freestanding mode:

- `--env freestanding --runtime none`
- `--env freestanding --runtime alloc`
- `--env freestanding --runtime gc`

`runtime/include/kn/freestanding.h` documents the ABI expected by custom freestanding runtimes.

Current status:

- `runtime none`
  works directly with compiler-emitted core helpers
- `runtime alloc`
  expects the target to provide allocator and string helpers
- `runtime gc`
  expects the target to provide GC/runtime hooks
