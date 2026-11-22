# Kinal performance benchmarks

Run these commands from the compiler repository root on Windows with Python 3. The runner uses Python's standard library. It builds the same [Kinal program](bench.kn) with the frozen stage0 seed and the self-hosted stage3 compiler, using each compiler's default settings and explicit full LTO.

```powershell
python -X utf8 benchmarks/suite.py
python -X utf8 benchmarks/suite.py --cases all
python -X utf8 benchmarks/suite.py --only kinal-c-lto,kinal-self-lto --cases hash_collisions
```

The default command times four workloads covering hash collisions, dependent memory reads, sliding-window maxima and an integer Haar transform. The full suite has 22 workloads. Each build includes all 22.

The default compiler paths are `out/selfhost/stage0-host/kinal.exe` and `out/selfhost/stage3/kinal.exe`. To measure a particular release and its packaged runtime, set `KINAL_STAGE0` to that bundle's executable. Set `KINAL_STAGE3` to select another self-hosted executable. Each run records the resolved paths and SHA-256 digests. Missing compilers appear as skipped configurations.

Each run writes a fresh directory under `benchmarks/results/runs/`. Use `--out <new-directory>` for a different location. It contains the source, reference values and harness snapshot, individual samples, a Markdown report and a comparison CSV. Check an existing run with:

```powershell
python -X utf8 benchmarks/verify_suite.py benchmarks/results/runs/<run>
```

- Build time is the median of three full application builds after one warm-up. Startup includes spawning a fresh process, collecting its output and waiting for exit, with nine samples after one warm-up.
- Runtime is wall time per invocation, measured in three independently warmed workers with three batches each. Batch size is calibrated in multiples of seven so each seed has equal weight. A pipe round trip is shared across the batch.
- Fixed-work peak working set is measured in a separate process over seven individual calls. This includes the runtime and all resident process pages.
- [cases.json](cases.json) lists cases in worker ID order. The expected values for seeds 7 through 13 at half and full input size were calculated with the Python specification for the original 18 cases and independent checksum models for the four extended cases. Every single call and timed batch is checked. The two floating-point cases use relative tolerance `2e-6` and absolute tolerance `1e-6`.

The [2026-11-20 Windows x64 baseline](baselines/20261120-windows-x64.csv) contains the four Kinal configurations from the verified i7-13700KF run with the published stage0 (`76a10056`) and workspace stage3 (`c72cd337`).

### Native string traversal

Two kernels in the unchanged suite call `s.Length()` in a loop over a local
string literal. With full LTO on Windows x64, stage3 f86afafe → 82b7a407
reduced median time at 1,500 iterations from 0.4054 to 0.0476 ms for
`int_parse` and from 0.2527 to 0.2072 ms for `string_scan`. Each compiler
ran six interleaved worker rounds with three measured batches per worker.
Every batch covered all seven seeds equally. Each baseline function had one
`strlen` call site in the optimized production bitcode and the new functions
had zero. Address-taken and reassigned locals still handle dynamic strings.
[Per-batch samples and binary hashes](baselines/20261122-native-string-roots.csv)

### Function calls

[calls.kn](calls.kn) uses the same integer worker protocol. Case 0 runs a nested helper chain, case 1 uses mutual tail recursion, and case 2 calls a function that can throw. The [Windows stage3 baseline](baselines/20261121-stage3-function-calls.csv) records default and Full LTO medians for these workloads and Fibonacci. The following PowerShell example runs a 50,000-iteration nested chain with seed 7 and prints `60935` after `READY`.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/calls.kn --no-module-discovery --lto=full -o out/calls.exe
'1','7','0' | & ./out/calls.exe 0 50000
```

### KinalVM interpreter performance

The VM comparison compiles `calls.kn` and the 22-kernel `bench.kn` once each.
Every runner receives the same bytecode. A fresh worker prints `READY` before
timing starts. Each timed batch includes sending the input and reading the
result over pipes. The harness verifies every result, discards the first
round, and shuffles the order of cases and runners in each round.

```powershell
python benchmarks/vm_dispatch.py --compiler out/selfhost/stage3/kinal.exe --runner before=C:/old-sdk/kinalvm.exe --runner after=out/selfhost/stage3/kinalvm.exe --output out/vm-dispatch.csv
```

Windows x64 stage3-built runner, median milliseconds per batch from seven
samples. Size gives the iteration count for loops and the call depth for mutual
recursion. Each row uses the same size, repetition count and seeds on both runners.

| Workload | Size | Repetitions | Previous (ms) | Updated (ms) | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| Nested calls | 4,500 | 2 | 115.798 | 53.155 | 2.18× |
| Mutual recursion | 32 | 380 | 105.542 | 47.957 | 2.20× |
| Checked calls | 9,000 | 2 | 96.352 | 45.315 | 2.13× |
| lcg | 50,000 | 1 | 90.465 | 38.344 | 2.36× |
| branch | 50,000 | 1 | 181.346 | 78.566 | 2.31× |
| float_series | 50,000 | 1 | 93.087 | 40.998 | 2.27× |
| sliding_max | 16,384 | 1 | 177.154 | 74.305 | 2.38× |

Ordinary calls transfer tagged results directly between frames. The interpreter
reuses the current function's code and register arrays across instructions and
refreshes them when execution switches frames or returns from a host call.
On nested calls, median peak process commit fell from 11.68 to 9.41 MiB.
On the integer recurrence (`lcg`) it measured 10.66 and 10.65 MiB respectively.
[Stage3 raw samples](baselines/20261122-vm-dispatch-self.csv) · [C-built runner samples](baselines/20261122-vm-dispatch-c.csv)

The Windows x64 selfhost build uses ThinLTO for the interpreter. On the same bytecode, the previous packaged runner and the ThinLTO runner took 36.94 and 10.54 ms for `lcg`, 78.60 and 21.51 ms for `branch`, 39.46 and 11.70 ms for `float_series`, and 79.42 and 22.01 ms for `sliding_max`. Each figure is the median of seven shuffled samples after warm-up. Across all seven workloads, including three call patterns, speedups ranged from 3.37× to 4.51×.
[Windows x64 runner samples](baselines/20261122-vm-runner-thin.csv)

## Editor diagnostics and completion

Run the same edit sequence against each language server with its matching SDK packages available. This example uses the Studio MochaGallery template.

```powershell
python benchmarks/lsp_edits.py --server ../Kinal-Studio/out/studio/kinal-lsp-server.exe --project ../Kinal-Studio/templates/MochaGallery --member form.Window.Run --output gallery.csv
```

The benchmark opens the source in memory, queues diagnostics on one edit, then inserts a newline and asks for a specific member. It checks the completion and diagnostics, times a second completion on the idle server, and discards the first round. The reported times cover the language-server protocol round trip.

On Windows x64, indexing package manifests and Unit declarations once per root per analysis reduced median queued completion from 85.81 to 73.89 ms in ConnectionDesk and from 90.38 to 78.89 ms in MochaGallery. Idle completion measured 0.33 and 0.45 ms respectively on both servers. Each comparison used 18 measured rounds per version. [Baseline and server hashes](baselines/20261122-lsp-package-resolution.csv)

## Compiler build times

Run the [compile-speed benchmark](compile_speed.py) to measure Hello, the 22-kernel program and a generated 1,000-function source on Windows x64. Add an earlier compiler in its toolchain directory for a three-way comparison. The optional project workloads build the current compiler sources to a COFF object (`--projects`) and link a supplied application with full LTO (`--project`).

```powershell
python -X utf8 benchmarks/compile_speed.py
python -X utf8 benchmarks/compile_speed.py --previous <previous-stage3.exe> --projects --project <application.knproj>
```

Use `--frontend-only` to isolate semantic checking from LLVM and linking.

`property-access-120` declares 120 classes with the same property name. Each class has a function that performs 16 property reads and writes interleaved with field reads. The C frontend check verifies all 3,960 accessor calls, including the final read in each function. Across eight warm Windows x64 runs per compiler with alternating order, the stage3 semantic check took a median of 169 ms with the accessor index and 180 ms with the preceding compiler. [Timings and peak working sets](baselines/20261122-selfhost-property-access.csv)
It checks a file with 13,000 small functions and a chain of 6,500 direct
calls, verifies the resulting `.kcheck` reports and records the compiler's
`--time` phase measurements in `samples.jsonl`. On Windows x64, indexing
functions reduced median end-to-end C frontend time from 2,215 to 170 ms for
declarations and from 1,504 to 166 ms for the call chain (seven warm samples).
See the [compiler identities, input hashes and sample ranges](baselines/20261122-frontend-index.csv).

The frontend suite also checks a two-file project with 64 module aliases and
1,800 qualified calls. On Windows x64, median selfhost check time for that
workload fell from 120.79 to 109.75 ms across seven warm runs. Checking the
whole compiler took 1.079 s and 1.072 s respectively on the same machine.
See the [module alias measurements](baselines/20261122-selfhost-symbol-aliases.csv).

Add `--projects` to check the selfhost compiler project, or `--project <path.knproj>`
to check an application with the same frontend-only settings. Checking the
selfhost project with the C compiler took a median of 90.33 s before the
type lookup change and 1.90 s after it (three warm samples per compiler).
Observed peak working set fell from about 2,996 MiB to 173 MiB.
See the [compiler-project measurements](baselines/20261122-compiler-type-lookup.csv).

To measure how Native emission scales with object-valued call arguments:

```powershell
python benchmarks/compiler_roots.py --compiler previous=C:/old-sdk/kinal.exe --compiler new=out/selfhost/stage3/kinal.exe --identical-artifacts --output out/compiler-roots.csv
```

Each generated function calls a helper with two freshly allocated objects at
256, 1,024 or 4,096 sites. The harness runs each executable, checks its sum,
records deterministic check output and IR hashes, and takes seven shuffled
measurements after warm-up. Use `--identical-artifacts` when comparing changes
expected to preserve the emitted IR. Windows x64 stage3 2782a7c0 → 82b7a407
reduced median IR wall time at 4,096 sites from 1,390 to 477 ms with identical
IR. Checking that source took 259 and 262 ms. On the full compiler, check time
was 1,078 and 1,073 ms, and COFF build time was 5,271 and 5,210 ms. Building
the 22-kernel program with full LTO took 418 and 400 ms (five samples each).
[Root scaling samples](baselines/20261122-compiler-roots.csv) · [Project samples](baselines/20261122-compiler-native-projects.csv)

The named-type workload declares 2,400 classes and 2,400 functions using those
classes in parameter and return types. On Windows x64, indexing declarations
by their final identifier reduced the median C frontend check from 972 to
164 ms over five warm samples. On the selfhost compiler project, the C frontend
went from 1,820 to 742 ms over seven samples, with an observed peak working set
of about 174 MiB on both versions. Stage3 checked the same project in 1,070 ms.
Studio's ConnectionDesk queued member completion went from 74.04 to 50.31 ms
across 18 protocol samples per server.
[Timings, memory measurements and binary hashes](baselines/20261122-named-type-index.csv)

Each source and mode gets two warm-ups and nine timed rebuilds. Each project gets one warm-up and three timed builds. The runner shuffles compiler order per round, preserves build failures and checks generated headers. After timing each single-file build, it runs the executable to check the greeting, the edited startup result, or the 1,000-function output against a Python reference. It records compiler and source SHA-256 digests. For a project, `project-inputs.json` covers the manifest and all `.kn` files below its directory, with a second check after timing. OS and package caches stay warm. The [2026-11-20 compile-time baseline](baselines/20261120-compiler-speed.csv) records medians, ranges, sample counts and exact compiler identities.

## JSON array parsing

Build [json_parse.kn](json_parse.kn) against the IO.Json package you want to measure.
The executable accepts an element count (default 16,000). It constructs a JSON
array of repeating digits before starting the timer, measures `ParseValue` and
`AsList`, and verifies every element before printing elapsed nanoseconds.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/json_parse.kn --no-module-discovery -o out/json-parse.exe
./out/json-parse.exe 16000
```

The [Windows x64 comparison](baselines/20261122-json-parse.csv) uses the same
compiler for each before/after pair. Each sample comes from a fresh process.
One warm-up preceded five measured rounds with shuffled execution order.

## JSON formatting

[json_format.kn](json_format.kn) times `IO.Json.Format(value, false)` and
checks the complete output against an independently constructed string. Pass
a count and a mode. `array` counts numeric elements, `plain` counts ASCII
characters, and `escaped` counts repetitions of a UTF-8 string containing
quotes, control characters and a backslash. `objects` and `floats` count
elements in an array. An optional third argument selects `pretty` or `compact`.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/json_format.kn --no-module-discovery -o out/json-format.exe
./out/json-format.exe 8192 array
```

The [Windows x64 comparison](baselines/20261122-json-format.csv) records five
fresh-process samples per input and compiler, following one warm-up.

[json_format_profile.py](json_format_profile.py) compares the Native and VM
versions from two complete Windows SDKs. Each process checks every output byte.
The timer surrounds the formatting call. Peak working set and peak commit
cover the entire process, including input construction.

```powershell
python benchmarks/json_format_profile.py --sdk before=C:/old/kinal.exe --sdk after=C:/new/kinal.exe --samples 9 --output json-format.csv
```

On Windows x64, formatting the escaped-string workload fell from 7.69 to
0.71 ms on Native and from 7.34 to 0.73 ms on KinalVM. The 8,192-element
integer array fell from 3.53 to 1.43 ms on Native. Peak working set on Native
fell from 13.34 to 12.32 MiB for 800,000 ordinary characters. Pretty arrays
of objects changed little on the VM. These are medians over nine fresh
processes after one warm-up. [Measurements and binary hashes](baselines/20261122-json-value-writer.csv)

## Text prefix and slice

[text_ranges.kn](text_ranges.kn) alternates two inputs and checks how often the
result matches. Pass the number of padding bytes, the operation (`prefix`,
`early` or `late`), and optionally the number of calls (default 401). The
reported nanoseconds cover the entire loop.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/text_ranges.kn --no-module-discovery -o out/text-ranges.exe
./out/text-ranges.exe 524288 early
```

The [Windows x64 timings](baselines/20261122-text-ranges.csv) cover all three
operations with 16 and 524,288 bytes of padding. The early slice reads the
first four bytes. The late slice starts at the final byte and requests four.

## Substring search, replacement, and splitting

[text_search.kn](text_search.kn) takes an operation, a block count, and a
repetition count. Most cases repeat the five-byte block `abc::`. The
`periodic` and `reverse` cases use one `a` per block and search for 127
copies of `a` followed by `b`. Input construction and output validation are
outside the timed loop. The replacement case includes reading the output
length and checks the final string. The split case checks the final list
element by element, including the empty trailing field.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/text_search.kn --no-module-discovery -o out/text-search.exe
./out/text-search.exe periodic 32768 3
./out/text-search.exe replace 8192 5
```

The [Windows x64 baseline](baselines/20261122-text-search.csv) records five
fresh-process samples after one discarded warm-up per case. The same native
compiler built both library versions, and their runs were interleaved in
shuffled order. Times cover all repetitions. On stage3, three unsuccessful
searches of 32 KiB with a nearly matching 128-byte pattern took 17.42 ms
before and 0.286 ms after. Five replacements of 8,192 delimiters took
8.71 ms and 2.72 ms respectively. For short inputs the change can be within
measurement noise.

## Path normalization

[path_normalize.kn](path_normalize.kn) takes the number of path segments and
the number of repetitions. Every seventh segment is `..`. The benchmark times
`IO.Path.Normalize` and reading the output length. It checks the checksum and
the complete final path outside the timed loop.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/path_normalize.kn --no-module-discovery -o out/path-normalize.exe
./out/path-normalize.exe 600 50
```

On Windows x64, normalizing a six-segment path 5,000 times took 25.89 ms
before and 4.89 ms after the change. A 600-segment path normalized 50 times
took 184.40 ms and 3.46 ms respectively. Both native executables were built
with the same stage3 compiler. These are medians from seven fresh processes
per version, alternating run order after one discarded warm-up.
[Raw samples and package hashes](baselines/20261122-path-normalize.csv)

## Appending to existing files

[file_append.kn](file_append.kn) appends a 16-byte ASCII record and one LF per
iteration. It takes an existing disposable file and a repetition count, checks
the final byte count and prints the time spent in the append loop in nanoseconds.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/file_append.kn --no-module-discovery -o out/file-append.exe
$sample = New-TemporaryFile
try {
    [IO.File]::WriteAllText($sample.FullName, ('x' * 1048576))
    ./out/file-append.exe $sample.FullName 64
} finally {
    Remove-Item -LiteralPath $sample.FullName
}
```

On Windows x64, appending 64 records to a 1 MiB file took 151.67 ms before
and 9.40 ms after the change with stage3 Native, and 159.20 ms and 9.52 ms
with KinalVM. For an initially empty file the corresponding times were
14.83 ms and 9.27 ms for Native, and 14.74 ms and 9.26 ms for KinalVM.
Each figure is the median of seven fresh processes after one warm-up per
configuration. Runs were interleaved and the full resulting file was checked.
The same stage3 compiler built both native programs. Both VM runners executed
the same bytecode. [Raw samples and process memory](baselines/20261122-file-append.csv)

## Reading the first line

[file_first_line.kn](file_first_line.kn) takes a file path and a repetition
count. It checks that every read returns `kinal-header` and prints the elapsed
nanoseconds for the whole loop. This example reads a short header followed by
4 MiB of text.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/file_first_line.kn --no-module-discovery -o out/file-first-line.exe
$sample = New-TemporaryFile
try {
    [IO.File]::WriteAllText($sample.FullName, "kinal-header`r`n" + ('x' * 4MB))
    ./out/file-first-line.exe $sample.FullName 20
} finally {
    Remove-Item -LiteralPath $sample.FullName
}
```

On Windows x64, 20 reads of that file took 82.52 ms before and 0.96 ms
after with stage3 Native, and 86.29 ms and 0.95 ms with KinalVM. The new
native process peaked at 1.02 MiB of committed memory with either a 4 MiB
tail or an empty tail. These are medians of seven fresh processes per case,
with one warm-up per case and randomized run order. The same compiler built
both native versions and both runners executed the same bytecode.
[Samples and peak process memory](baselines/20261122-file-first-line.csv)

## Removing dictionary keys and set members

[collection_remove.kn](collection_remove.kn) takes `dict` or `set` and a
power-of-two key count. It creates the string keys, times removal of three
quarters of them in a fixed permutation, then checks every removed and
remaining key. The duration printed is in nanoseconds.

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/collection_remove.kn --no-module-discovery -o out/collection-remove.exe
./out/collection-remove.exe dict 4096
out/selfhost/stage3/kinal.exe vm build benchmarks/collection_remove.kn --no-module-discovery -o out/collection-remove.knc
out/selfhost/stage3/kinalvm.exe out/collection-remove.knc set 4096
```

Windows x64 median time to remove 3,072 of 4,096 keys, in milliseconds:

| Backend | Dict before | Dict after | Set before | Set after |
| --- | ---: | ---: | ---: | ---: |
| Native, selfhost compiler | 1,259.33 | 0.395 | 924.72 | 0.389 |
| KinalVM | 1,390.42 | 10.93 | 1,294.65 | 9.77 |
| Native, C compiler | 415.15 | 0.077 | 271.61 | 0.075 |

Each case used seven fresh processes after one warm-up, with randomized run
order. Native pairs used the same compiler and VM runners used identical
bytecode. The change replaces rebuilding the table on each removal with
in-place repair of the affected probe sequence.
[Samples for 1,024 and 4,096 keys and process memory](baselines/20261122-collection-remove.csv)

## Building UTF-8 strings

`text_builder.kn` appends the same 10-byte UTF-8 fragment with repeated `+=`,
a growing `IO.Text.Builder`, or a builder preallocated with `EnsureCapacity`.
The timer includes allocation, appending, and the final `Build()`. Each run
verifies every byte against a separately constructed string outside the timed loop.

```powershell
kinal build benchmarks/text_builder.kn --no-module-discovery -o out/text-builder.exe
./out/text-builder.exe 5000 builder
kinal vm build benchmarks/text_builder.kn --no-module-discovery -o out/text-builder.knc
kinalvm out/text-builder.knc 5000 reserved
```

A paired Native rerun compares stage3 `82b7a407` and `b6da3434` with each compiler’s matching runtime. Both build the same source. For each LTO setting and size, the table shows the median of seven shuffled process runs after a warm-up. Peak commit covers the entire process, including verification.

| LTO | Fragments | Previous `+=` (ms) | New `+=` (ms) | Previous peak commit (MiB) | New peak commit (MiB) |
| --- | ---: | ---: | ---: | ---: | ---: |
| off | 500 | 1.978 | 0.027 | 2.26 | 1.05 |
| off | 5,000 | 194.851 | 0.299 | 9.27 | 1.33 |
| full | 500 | 0.541 | 0.011 | 2.25 | 1.05 |
| full | 5,000 | 46.828 | 0.112 | 9.27 | 1.34 |

Reproduce with `python benchmarks/text_builder_profile.py --sdk previous=PATH_TO_OLD_COMPILER --sdk new=PATH_TO_NEW_COMPILER --output profile.csv`. The CSV includes the explicit builder controls, process working sets, source and binary hashes. [Paired runtime samples](baselines/20261122-native-local-append.csv) · [Compiler project and benchmark build samples](baselines/20261122-local-append-builds.csv).

The earlier Native and KinalVM measurements below used the preceding concatenation implementation.

| Backend | Fragments | Repeated `+=` | Builder | Preallocated builder |
| --- | ---: | ---: | ---: | ---: |
| Native | 500 | 2.049 | 0.064 | 0.058 |
| Native | 5,000 | 189.579 | 0.584 | 0.555 |
| KinalVM | 500 | 1.931 | 2.220 | 2.252 |
| KinalVM | 5,000 | 110.197 | 22.016 | 21.942 |

At 500 fragments, repeated concatenation was faster on the VM. At 5,000,
building the 50,000-byte result with a growing buffer took about one fifth of
the VM concatenation time. [Raw samples and toolchain hashes](baselines/20261122-text-builder.csv)

## Calendar formatting

`time_format.kn` takes a round count and a pattern workload (`stamp`, `short` or
`literal`). It measures formatting and a UTF-8 byte-length checksum inside the
program. It checks the complete result and checksum before printing nanoseconds.

```powershell
kinal build benchmarks/time_format.kn -o out/time-format.exe
./out/time-format.exe 30000 stamp
kinal vm build benchmarks/time_format.kn -o out/time-format.knc
kinalvm out/time-format.knc 800 stamp
```

The table reports Windows x64 medians from five interleaved samples per build after one warm-up.

| Backend | Pattern | Calls per sample | Before (ms) | After (ms) |
| --- | --- | ---: | ---: | ---: |
| Native | stamp | 30000 | 96.649 | 80.389 |
| Native | short | 60000 | 28.030 | 20.745 |
| Native | literal | 4000 | 710.934 | 4.229 |
| KinalVM | stamp | 800 | 1724.262 | 4.589 |
| KinalVM | short | 2000 | 303.532 | 6.668 |
| KinalVM | literal | 40 | 1790.264 | 0.168 |

The previous VM interpreted the formatter loop. The new VM calls the same
native-compiled Kinal formatter used by native programs. The stamp includes
milliseconds. The short pattern formats only the hour. The literal pattern
contains 304 UTF-8 bytes and no date fields. Compare timings
within each row because iteration counts differ between workloads and backends.
[Raw timings and toolchain hashes](baselines/20261122-time-format.csv)

### Lookup in wide lexical scopes

On Windows x64, stage3 `dcbdb8c2` indexes names in large scopes and native root slots by symbol. Each generated function assigns the same string argument to every local and reads every local. Both compilers produced byte-identical check output and LLVM IR, and the executable returned the expected checksum. Times are median process wall time in milliseconds across seven shuffled samples after a warm-up. The comparison compiler is `b6da3434`.

| String locals | Previous check | New check | Previous IR | New IR |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 91.1 | 85.9 | 128.3 | 127.0 |
| 1,024 | 129.1 | 119.5 | 218.0 | 201.9 |
| 4,096 | 372.8 | 255.2 | 659.0 | 459.0 |
| 8,192 | 1,075.8 | 436.0 | 2,121.7 | 1,064.4 |

At 8,192 locals the compiler process used 123.25 → 124.03 MiB peak working set while emitting IR.

On the compiler's own project, median check time was 1,108 → 1,098 ms and COFF object generation was 5.312 → 5.323 s. The 22-kernel Full LTO build took 433 → 419 ms. Its executable was byte-identical to the one used in the previous four-language runtime comparison.

```powershell
python benchmarks/compiler_roots.py --workload locals --sizes 64 1024 4096 8192 --compiler previous=C:/old-sdk/kinal.exe --compiler new=out/selfhost/stage3/kinal.exe --identical-artifacts --output out/compiler-locals.csv
```

[Per-run compiler timings, memory and artifact hashes](baselines/20261122-compiler-lexical-scopes.csv) · [Real-project build samples](baselines/20261122-lexical-projects.csv)

## Source token storage

Windows x64, i7-13700KF. Stage3 `4a34d055` → `4d32ed7b`. Six interleaved builds per workload and compiler after one warm-up, on the same source paths and SDK libraries. The reported memory is peak process commit. Every pair produced byte-identical output.

| Workload | Elapsed (ms) | Peak commit (MiB) |
| --- | ---: | ---: |
| 1,200 generated functions, AST | 44.9 → 41.8 | 12.5 → 12.3 |
| Compiler sources, AST | 299.8 → 271.3 | 79.9 → 76.0 |
| Compiler sources, semantic check | 886.7 → 825.5 | 161.2 → 155.4 |
| Compiler sources, LLVM IR | 1,719.0 → 1,560.4 | 342.8 → 250.5 |
| Compiler sources, COFF | 5,461.9 → 5,382.9 | 317.3 → 297.3 |
| ConnectionDesk, COFF | 829.2 → 840.5 | 62.3 → 53.7 |
| Parser.kn, token summary | 63.9 → 60.0 | 27.0 → 26.7 |
| Hello program, COFF | 260.1 → 250.4 | 30.2 → 28.8 |

The Native lexer copies short token spellings into 2 KiB blocks during full scans of larger sources. Each token keeps an interior string pointer. Long spellings have separate storage. Header probes and the VM use their existing paths. The CLI releases its token-list references after dependency resolution, before binding. Metrics and token dumps retain the lists they consume.

Use `python benchmarks/compile_speed.py --projects --only compiler-ir --previous C:/previous/kinal.exe --stage3 out/selfhost/stage3/kinal.exe --project-rounds 6` for the whole-compiler IR case. [Per-build elapsed time, memory and artifact hashes](baselines/20261122-lexer-storage.csv)

## Hosted collector address index

Windows x64, i7-13700KF. Stage3 `ce99734a` → `4a34d055`. During a collection of at least 1,024 blocks the hosted runtime sorts their addresses one byte at a time, using the mark stack as temporary storage. It skips address bytes shared by every block. Small collections use the existing in-place heapsort. The collector continues to recognize interior pointers.

| Build workload | Elapsed (ms) | Peak process commit (MiB) |
| --- | ---: | ---: |
| Compiler semantic check | 1017.9 → 863.3 | 161.6 → 161.2 |
| Compiler to COFF | 5668.8 → 5344.6 | 317.3 → 320.7 |
| 22-kernel program, Full LTO executable | 436.1 → 425.6 | 27.6 → 27.7 |

The build table compares complete SDKs over five measured rounds after warm-up. A second experiment compiled identical source against the same version of the standard library with the two compiler binaries. Each pair produced identical files. [Build samples](baselines/20261122-native-gc-index-projects.csv) · [Same-output phase timings](baselines/20261122-native-gc-index-phases.csv)

| Backend | Live objects | Collect live list (ms) | Collect after the frame returns (ms) | Peak process commit (MiB) |
| --- | ---: | ---: | ---: | ---: |
| native | 512 | 0.065 → 0.068 | 0.038 → 0.039 | 1.0 → 1.1 |
| native | 8,192 | 1.234 → 0.769 | 0.787 → 0.276 | 1.6 → 1.6 |
| native | 262,144 | 51.608 → 30.235 | 35.022 → 13.540 | 22.0 → 22.0 |
| vm | 512 | 0.023 → 0.024 | 0.012 → 0.012 | 4.6 → 4.7 |
| vm | 2,048 | 0.094 → 0.087 | 0.047 → 0.046 | 5.5 → 5.5 |
| vm | 16,384 | 1.007 → 0.985 | 0.369 → 0.369 | 13.6 → 13.6 |

Seven measured processes per size, backend and SDK after warm-up. The fixture validates every node between the two collections. Use `python benchmarks/heap_profile.py --workload vm_handles --sdk previous=C:/old-sdk/kinal.exe --sdk new=out/selfhost/stage3/kinal.exe --count 262144 --vm-count 16384 --repeat 7 --output objects.csv` to repeat the largest case. [Object timings and peak memory](baselines/20261122-native-gc-index-objects.csv)

## Source scanning and literal recovery

Windows x64, i7-13700KF, stage3 `b81e6e27` and `ce99734a`. Times are end-to-end medians after a warm-up, with eight measured builds per compiler on the same source paths. Compiler order alternates on each workload. Each pair produced byte-identical output.

| Workload | Previous (ms) | Current (ms) |
| --- | ---: | ---: |
| 1,200 functions with UTF-8 comments, identifiers and escaped literals, AST | 52.2 | 42.3 |
| Compiler sources, AST | 393.5 | 352.3 |
| Parser.kn, complete token dump | 66.1 | 61.4 |
| Compiler sources, semantic check | 1,106.7 | 1,021.5 |
| Compiler sources, Native COFF object | 5,396.3 | 5,661.2 |

A separate run used both compiler executables from the same SDK directory. Four measured rounds followed one warm-up. For the compiler project, the reported IR generation phase increased from 571 to 977 ms. With COFF output the code generation phase increased from 4,360 to 4,766 ms. Parsing and semantic checking both took less time. The two compilers produced identical IR and COFF files. [Phase timings](baselines/20261122-lexer-native-phases.csv)

The lexer tracks byte positions locally while reading comments and identifiers. It scans a quoted string once and keeps the first unescaped recovery delimiter until it finds a closing quote or the end of the line. Token rows include the original spelling and UTF-8 byte columns. The Native and KinalVM lexer consumers use the same source. Native tokens retain their spelling across forced collections, including when the C seed builds the consumer.

To run the parsing and semantic workloads from the compiler repository, use the two complete SDK installations and a fresh results directory.

```powershell
python benchmarks/compile_speed.py --frontend-only --projects --only lexer-spans-1200,parser-tokens,compiler-ast,compiler-check --previous C:/previous/kinal.exe --stage3 out/selfhost/stage3/kinal.exe --rounds 8 --project-rounds 8
```

[Times, compiler hashes, peak working sets and artifact hashes](baselines/20261122-lexer-spans.csv)

## Bytecode compilation and symbol lookup

Windows x64, i7-13700KF. The table measures end-to-end compiler wall time, including process startup. Each median uses five measured builds after one warm-up, interleaving the previous (`01f47735`) and current (`b81e6e27`) self-hosted compilers. The lookup harness records a timeout once and continues with the other compiler.

| Build workload | Previous (ms) | Current (ms) | Previous / current |
| --- | ---: | ---: | ---: |
| 2,500 record types, Native object | 1,068.6 | 954.2 | 1.12× |
| 512 reachable functions, KNC | 11,382.4 | 121.7 | 93.51× |
| 4,000 reachable functions, KNC | over 45,000 | 702.0 | >64.1× |
| 512 globals, Native object | 270.6 | 268.9 | 1.01× |
| 4,000 globals, Native object | 675.6 | 563.4 | 1.20× |
| 4,000 globals, KNC | 17,378.5 | 258.0 | 67.37× |
| 22-kernel benchmark, KNC | 428.5 | 101.0 | 4.24× |
| ConnectionDesk, KNC | 9,829.1 | 193.1 | 50.89× |
| Compiler project, semantic check | 1,107.1 | 1,078.2 | 1.03× |
| Compiler project, Native object | 5,305.9 | 5,224.1 | 1.02× |
| 22-kernel benchmark, full-LTO executable | 429.9 | 425.5 | 1.01× |

The KNC writer now grows its storage geometrically during lowering and serialization. Both backends use indexes for HIR definitions and global symbols. The full benchmark suite produced byte-identical Native executables, LLVM IR and KNC files with both compilers. ConnectionDesk also produced identical bytecode, with median peak compiler working set decreasing from 54.7 to 38.9 MiB. Application execution time is measured separately in the runtime suite.

To reproduce the synthetic cases on Windows, point both SDK paths at the executable in each complete compiler installation and choose a fresh output directory.

```powershell
python benchmarks/compiler_lookups.py --sdk previous=<previous-compiler.exe> --sdk current=<current-compiler.exe> --samples 5 --timeout 45 --out <new-directory>
```

[Lookup workloads and peak memory](baselines/20261122-compiler-lookups.csv) · [Application bytecode builds](baselines/20261122-compiler-vm-projects.csv) · [Native project builds and phase timings](baselines/20261122-compiler-lookup-native-projects.csv)

## Modular integer loops and byte scans

Windows x64, stage3 `c9203c86` → `01f47735`, Full LTO. Each entry is the median time in microseconds per full workload across six alternating process pairs. The second measurement pins the harness and its workers to one performance core (logical processor 8). Every batch checks all seven seed results.

| Kernel | Previous µs | Current µs | Previous pinned µs | Current pinned µs |
| --- | ---: | ---: | ---: | ---: |
| lcg | 301.55 | 104.45 | 322.90 | 107.59 |
| string_scan | 209.11 | 82.19 | 220.96 | 86.18 |
| int_parse | 52.16 | 38.10 | 55.21 | 39.54 |
| branch | 669.31 | 669.59 | 692.79 | 690.43 |
| gcd_batch | 130.13 | 100.52 | 142.54 | 103.57 |

In a unit-stride loop of the form `state = (state * a + b) % m`, Native can group up to four updates before taking the remainder. The compiler bounds the intermediate values using the accumulator type and the largest possible addend. The fast path starts from a reduced, nonnegative state. Larger or negative seeds use the source arithmetic for the current batch. Eligibility is checked again at the next batch boundary. Short tails also use scalar updates. Groups stop at the existing 1,024-iteration collection boundary. The byte-scan optimization also covers numeric casts and indexing a local string. A changing NUL terminator still affects the next length test.

The following runs start with a negative or unreduced accumulator. Each cell is the median of 12 measured batches under default scheduling, alternating the SDKs in four process pairs. Each batch checks seven consecutive signed seeds against an independent wrapping-arithmetic oracle.

| Kernel | First seed | Previous µs | Current µs |
| --- | ---: | ---: | ---: |
| lcg | -424242 | 317.73 | 238.49 |
| lcg | 4611686018427387904 | 317.84 | 111.10 |
| lcg | -4611686018427387904 | 324.20 | 233.90 |
| string_scan | -424242 | 220.54 | 227.00 |
| string_scan | 4611686018427387904 | 220.51 | 223.10 |
| string_scan | -4611686018427387904 | 218.47 | 83.61 |

Compiling the compiler itself took 1.113 → 1.131 s for its semantic check and 5.422 → 5.550 s to emit a Windows x64 COFF object. Building all 22 kernels with Full LTO took 463.4 → 488.7 ms (five measured builds per SDK).

[Paired timings, checksums and executable hashes](baselines/20261122-native-modular-kernels.csv) · [Signed seed timings](baselines/20261122-native-modular-signed.csv) · [Project build samples](baselines/20261122-native-modular-builds.csv).

## Compiler and program memory on Windows

`list_release.kn` creates 64 lists of 16 distinct strings, each with a
32 KiB prefix. It empties each list using the selected operation, adds one
integer, keeps the list alive, and checks its contents after collection.
`remove` repeatedly removes the middle element. To run the native and VM cases:

```powershell
out/selfhost/stage3/kinal.exe build benchmarks/list_release.kn --no-module-discovery -o out/list-release.exe
./out/list-release.exe clear 64
out/selfhost/stage3/kinal.exe vm build benchmarks/list_release.kn --no-module-discovery -o out/list-release.knc
out/selfhost/stage3/kinalvm.exe out/list-release.knc remove 64
```

Median peak process commit on Windows x64, in MiB:

| Operation | Selfhost Native before | After | KinalVM before | After |
| --- | ---: | ---: | ---: | ---: |
| Clear | 31.51 | 5.06 | 10.53 | 10.54 |
| Pop | 31.51 | 5.06 | 10.53 | 10.54 |
| RemoveAt | 6.79 | 5.06 | 10.54 | 10.54 |

The old native list kept discarded payload pointers in its backing storage.
The VM already released its guest list references. Each result is the median
of seven fresh processes after one warm-up, with the cases interleaved. Each
native pair used the same compiler and both VM runners used identical bytecode.
[Raw samples, including C Native and peak working set](baselines/20261122-list-release.csv)

`heap_objects.kn` allocates a linked list of live objects, forces a collection,
then traverses the list and checks every value. The Windows harness measures
the compiler process and the launched executable separately. Each recorded run
uses a fresh process. Peak working set counts resident pages, while peak commit
counts the process's peak committed private memory. Compiler counters exclude
child processes such as the linker.

`transient_arrays.kn` repeatedly allocates a local array of 4,096 integers,
checks its zero initialization, then returns a value from it. The final checksum
covers every iteration. This workload measures memory released during a run.

`nested_arrays.kn` retains three-integer rows in tiles of 128, forces a collection,
then verifies every requested row in reverse order. The count is the number of live
rows to fill and verify. Use the same count for Native and VM when comparing them.

```powershell
python benchmarks/heap_profile.py --sdk current=out/selfhost/stage3/kinal.exe --output heap.csv
python benchmarks/heap_profile.py --workload transient_arrays --count 4096 --vm-count 4096 --sdk current=out/selfhost/stage3/kinal.exe --output arrays.csv
python benchmarks/heap_profile.py --workload nested_arrays --count 16384 --vm-count 16384 --sdk current=out/selfhost/stage3/kinal.exe --output nested.csv
python benchmarks/heap_profile.py --workload typed_packages --count 16384 --vm-count 16384 --sdk current=out/selfhost/stage3/kinal.exe --output packages.csv
```

The [Windows x64 results](baselines/20261122-heap-objects.csv) contain seven
samples per configuration after a discarded warm-up. For the native program
with 300,000 live nodes, the smaller GC header reduced median peak working set
from 30.93 to 26.41 MiB and peak commit from 29.68 to 25.04 MiB. The VM sample
uses 2,048 nodes and measures the interpreter process. Compare each backend
against its own baseline because their workload sizes differ.

Stage3 `8560cd2b` can reuse the zeroed backing storage of a private fixed numeric array. The following paired Windows x64 runs compare `dcbdb8c2` with its successor. Each case has 4,096 iterations and a 4,096-element integer array. `transient_arrays.kn` declares it in a callee; `fixed_array_reentry.kn` declares it inside the loop. Both verify fresh zeros and a checksum. The table shows medians from seven shuffled runs after warm-up. VM rows are controls.

| Declaration | Backend | Previous ms | New ms | Previous commit MiB | New commit MiB | Previous working set MiB | New working set MiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Function calls | native | 43.46 | 0.94 | 9.05 | 1.07 | 13.24 | 5.68 |
| Function calls | vm | 47.39 | 47.84 | 5.45 | 5.45 | 11.87 | 11.88 |
| Loop declarations | native | 43.48 | 0.84 | 9.08 | 1.06 | 13.19 | 5.68 |
| Loop declarations | vm | 47.79 | 46.82 | 5.23 | 5.26 | 11.69 | 11.70 |

Compiling the compiler itself took 1,120 → 1,096 ms for the semantic check and 5.409 → 5.370 s for an x64 COFF object. Building the complete 22-kernel program with Full LTO took 449 → 420 ms (five measured builds per SDK).

Reproduce using the preceding `heap_profile.py` command with `--workload transient_arrays` and `--workload fixed_array_reentry`; use `--repeat 7` and pass both SDK paths. [Calls](baselines/20261122-native-fixed-array-calls.csv) · [Loop declarations](baselines/20261122-native-fixed-array-loops.csv) · [Real project builds](baselines/20261122-fixed-array-builds.csv).

On Windows x64, seven runs of the transient-array workload with the same compiler
and 4,096 iterations gave a median VM peak commit of 163.28 MiB before guest heap
collection and 6.12 MiB with collection. Median VM peak working set fell from
170.54 to 13.09 MiB. The native control used 9.05 MiB peak commit in both SDKs.
The sample table includes both runner hashes and the compiler process measurements.
[Transient-array samples](baselines/20261122-vm-transient-arrays.csv)

For 16,384 live nested rows on Windows x64, the VM workload median fell from 143.68 to 33.97 ms with the ThinLTO runner. Median peak working set was 16.25 and 16.23 MiB respectively. The paired native controls measured 3.33 and 3.35 ms with 6.76 MiB peak working set. Each sample checks all rows after collection.
[Nested-array samples](baselines/20261122-nested-arrays.csv)

`typed_packages.kn` keeps a linked list of objects alive. Each contains a typed
Package holding a struct with a three-integer array and a separate integer.
The program fills the objects, forces collection, then reads every Package
in reverse insertion order. At 16,384 entries on Windows x64, seven measured
runs gave medians of 15.85 ms for Native and 142.52 ms for KinalVM. Median
peak working sets were 9.28 MiB and 35.02 MiB respectively. [Typed Package samples](baselines/20261122-typed-packages.csv)

The phase comparison compiles one guest and interleaves old and new runners:

```powershell
python benchmarks/vm_collection_profile.py --workload packages --sizes 1024 4096 16384 --repeat 7 --compiler out/selfhost/stage3/kinal.exe --runner previous=C:/old-sdk/kinalvm.exe --runner new=out/selfhost/stage3/kinalvm.exe --output out/package-pages.csv
```

At 16,384 entries the median time to populate, collect and verify fell from
133.02 to 97.27 ms and the process peak working set from 35.13 to 28.47 MiB
on Windows x64. The explicit collection phase fell from 8.77 to 3.67 ms.
[Same-bytecode samples and runner hashes](baselines/20261122-vm-package-pages.csv)

### Paired local arrays

A function can work on two large private arrays at once, as in matrix multiplication and hash tables. KinalNative keeps up to two previously returned buffers and matches their exact byte sizes. Collection clears both weak references before tracing.

On Windows x64, `paired_fixed_arrays.kn` called a function 4,096 times with one 32 KiB and one 64 KiB array. Each call checked fresh zeros and used both arrays in a checksum. The single-array control uses `transient_arrays.kn`. The table compares stage3 `8560cd2b` and `c9203c86`, with seven measured processes per configuration after warm-up.

| Workload | Backend | Previous ms | Current ms | Previous peak commit MiB | Current peak commit MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| Two arrays (32 + 64 KiB) | native | 46.25 | 5.32 | 9.05 | 1.13 |
| Two arrays (32 + 64 KiB) | vm | 131.07 | 131.77 | 6.06 | 6.10 |
| One array (32 KiB) | native | 0.94 | 0.95 | 1.06 | 1.06 |
| One array (32 KiB) | vm | 48.37 | 47.89 | 5.43 | 5.48 |

The Full LTO binaries were also timed in six alternating process pairs. Each process discarded three warm-up batches and checked all seven seeds in every batch. [Paired kernel samples](baselines/20261122-paired-local-kernels.csv).

| Kernel | Previous µs | Current µs | Current / previous |
| --- | ---: | ---: | ---: |
| matrix_int | 28.53 | 18.58 | 0.651× |
| matrix_float | 27.35 | 17.38 | 0.636× |
| hash_table | 126.17 | 106.08 | 0.841× |
| hash_collisions | 228.26 | 215.80 | 0.945× |
| sieve | 39.00 | 38.89 | 0.997× |

With those same SDKs, the compiler's semantic check took 1.092 → 1.083 s and x64 COFF generation took 5.349 → 5.331 s. Full LTO for the 22-kernel benchmark took 429.792 → 415.301 ms (five measured builds per SDK).

To reproduce, run `heap_profile.py --workload paired_fixed_arrays --count 4096 --vm-count 4096 --repeat 7` with both `--sdk` paths and an `--output` CSV. Repeat with `--workload transient_arrays` for the single-array control. [Paired arrays](baselines/20261122-paired-locals.csv) · [Single array](baselines/20261122-single-local-control.csv) · [Compiler builds](baselines/20261122-paired-local-builds.csv).

### KinalVM list and dictionary growth

`vm_collections.kn` measures three phases in the guest process. It builds a list
of consecutive integers or a dictionary with their decimal string keys, forces
a guest collection, then checks every entry. Dictionary timings include key
conversion. The Windows harness compiles the guest once, interleaves both
runners, drops one warm-up and records fresh-process memory high-water marks.

```powershell
python benchmarks/vm_collection_profile.py --compiler out/selfhost/stage3/kinal.exe --runner previous=previous/kinalvm.exe --runner updated=out/selfhost/stage3/kinalvm.exe --repeat 7 --output collections.csv
```

Windows x64 medians for 16,384 elements, seven runs per runner:

| Collection | Build (ms, previous → updated) | Explicit guest GC (ms) | Look up all entries (ms) | Process peak commit (MiB) |
| --- | ---: | ---: | ---: | ---: |
| List | 799.23 → 17.06 | 0.787 → 0.015 | 24.75 → 26.50 | 12.30 → 6.89 |
| Dictionary | 2,216.85 → 33.29 | 0.802 → 0.015 | 32.65 → 33.30 | 13.45 → 10.44 |

The explicit GC timing covers collections containing only integers. Collections
that have held guest references can require a full traversal. Lookup results
include per-element validation. [Samples for all three sizes](baselines/20261122-vm-collections.csv)

### KinalVM fixed-size array initialization

`vm_array_init.kn` creates arrays of 8,192 elements. The sparse case checks three
default values, then writes and reads one element. The dense case checks every
default value, writes every element, and verifies the entire result. The object
case stores one shared object per array so object construction cannot dominate
the array measurement.

```powershell
python benchmarks/vm_collection_profile.py --workload array-init --compiler out/selfhost/stage3/kinal.exe --runner previous=previous/kinalvm.exe --runner updated=out/selfhost/stage3/kinalvm.exe --arrays 3 --repeat 7 --output array-init.csv
```

Windows x64 elapsed milliseconds per array, median of seven fresh processes
after one discarded warm-up. Each process times three arrays in the guest.

| Element | Sparse, previous → updated (ms) | Dense, previous → updated (ms) |
| --- | ---: | ---: |
| `float` | 104.636 → 0.029 | 122.898 → 15.659 |
| `bool` | 26.040 → 0.014 | 47.254 → 17.712 |
| `string` | 106.358 → 0.020 | 128.272 → 17.775 |
| Object reference | 200.445 → 0.023 | 220.265 → 18.964 |

For sparse object arrays, median peak process commit fell from 12.53 to
10.39 MiB. The CSV records elapsed nanoseconds for each three-array batch,
process high-water marks and the exact guest and runner hashes. [Raw samples](baselines/20261122-vm-array-init.csv)

### KinalVM object allocation and field access

`vm_handles.kn` builds a linked list with a distinct object and integer in
each node. It collects the live list, reads both fields of every node and
checks their values. A second collection runs after the frame that held the
list has returned.

```powershell
python benchmarks/vm_collection_profile.py --workload handles --compiler out/selfhost/stage3/kinal.exe --runner previous=previous/kinalvm.exe --runner updated=out/selfhost/stage3/kinalvm.exe --repeat 7 --output handles.csv
```

Windows x64 medians from seven fresh processes per runner and size, after one
discarded warm-up. Both runners executed the same bytecode. Times are in ms.

| Objects | Build, previous → updated | Collect live list | Read all fields | Collect released list | Peak process commit (MiB) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2,048 | 16.40 → 4.57 | 0.55 → 0.44 | 2.02 → 2.10 | 0.17 → 0.20 | 10.55 → 5.51 |
| 8,192 | 295.61 → 19.33 | 2.26 → 1.86 | 8.54 → 8.62 | 0.67 → 0.80 | 12.63 → 9.21 |
| 16,384 | 2,008.04 → 40.20 | 4.47 → 3.89 | 16.64 → 17.38 | 1.41 → 1.61 | 15.66 → 13.66 |

Paged handle storage reduced construction time most at the largest size.
Reading an already constructed list and releasing its nodes both took slightly
longer. The CSV includes each timing, process memory counters and executable
hashes. [Raw samples](baselines/20261122-vm-handles.csv)
