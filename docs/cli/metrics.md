# Project complexity (selfhost compiler)

`kinal metrics` analyzes the selected project's source, syntax trees and bound
semantic model. It does not generate machine code, invoke a linker or execute the
project. This command is available only in the selfhost compiler.

```sh
kinal metrics --project ./kinal.knproj
kinal metrics --project . --profile release --all
kinal metrics --project . -o reports/metrics.json
kinal metrics --project . -o reports/metrics.html
kinal metrics Main.kn --no-module-discovery --format json
kinal metrics --project . --max-cyclomatic 20 --max-cognitive 25 --max-nesting 5
```

Without arguments, `metrics` selects `./kinal.knproj` if present. `--format`
accepts `text`, `json` and `html`; otherwise the output extension selects JSON or
HTML, falling back to text. Omit `-o` or use `-o -` to write the report to stdout.
Diagnostics and timings stay on stderr. Output directories are created as needed.
JSON/HTML include every record; `--top N` limits only the text hotspot list
(default 10), and `--all` / `--top 0` shows every body.

The HTML report is self-contained, works offline, supports filtering and sorting,
and exposes function details without loading project source or running its code.

## Scope and layers

- **Project:** physical/code/comment/blank lines, tokens, declarations, logical
  statements, summed metrics, maxima, nearest-rank CC p95, recursion components,
  and semantic coverage information.
- **File:** source unit, profile participation, imports and line partitions,
  function/type counts, statement and complexity totals.
- **Type:** declared fields/methods, weighted methods (sum of direct method CC),
  class inheritance depth, direct descendants, interfaces and resolved coupling.
- **Callable:** source location, kind, parent for nested functions, parameters,
  locals, returns, throws, loops, catches, cases, logical operators, jumps,
  records, unsafe blocks, explicit allocation syntax, awaits, captures, direct
  and dynamic/virtual calls, fan-in/out, Halstead and maintainability measures.

The selected profile's `Source.Mode` determines participation. Unreachable source
files are excluded in `ReachableUnits`; choose an `AllSources` profile for a whole
source set. Participating package/runtime files are excluded by default;
`--include-dependencies` includes them. `--pkg-root`, `--stdpkg-root`, `--target`
and diagnostic options work as with a build.

Source definitions are counted once, even if multiple generic instances are
compiled. Function/Block literals and local functions have separate records;
their body decisions are not charged again to the enclosing function. Global and
field initializers have separate structural records. Their lexical measures and
maintainability index are unavailable. Bodyless declarations have CC 0 and no MI.

## Definitions and interpretation

The report includes versioned definitions; JSON schema identifier is
`kinal.metrics.v1`. Paths inside the project are relative to the project root.
IDs combine source path, line and column, distinguishing overloads and anonymous
functions. They identify a location in this report, not a permanent identity
across source edits. No timestamps are added, so unchanged inputs are reproducible.

**Structured cyclomatic complexity:** one per body, plus each `If`/`ElseIf`,
conditional expression, loop, non-default `Case`, `Catch`, and `&&`/`||` operator.
This source metric counts syntactic decisions, including constant conditions.
Kinal `Jump`/`Record` are reported separately: CC is not an exact CFG rank for
unstructured flow and is not the number of all possible execution paths.

**Cognitive complexity (`kinal-cognitive-v1`):** control constructs add one plus
their current nesting depth. `ElseIf` and `Else` add one without the nesting
penalty; a `Switch` adds once, rather than once per case. Each in-order logical
operator sequence adds one when its operator changes. `Jump` adds one. A function
participating in a direct-call recursive component adds one. Braces, `Try`,
`Case`, `Return`, `Break` and `Continue` do not add cognitive points. Nested
callables start at depth zero. This is a documented Kinal adaptation inspired by
[SonarSource's model](https://www.sonarsource.com/docs/CognitiveComplexity.pdf),
not a claim of identical scores across languages/tools.

**Halstead (Kinal lexical v1):** identifiers and literals (including `this`,
`base`, `default`) are operands, distinct by token kind and spelling. Other
keywords/operators count by token kind. Parentheses, braces, commas and
semicolons are excluded. Parser declaration spans count; top-level modifiers are included, while member
spans start at Function/Constructor or the accessor keyword. Nested callable
ranges do not count. Interpolated strings are one lexical operand, although their embedded
expressions contribute structural decisions. With distinct/total operators
`n1/N1` and operands `n2/N2`, vocabulary `n=n1+n2`, length `N=N1+N2`, volume
`V=N*log2(n)`, difficulty `D=(n1/2)*(N2/n2)`, effort `E=D*V`. Zero denominators
produce zero, not NaN.

**Maintainability index:** rounded, clamped to 0–100:

```text
(171 - 5.2*ln(max(V,1)) - 0.23*CC - 16.2*ln(max(code_lines,1))) * 100 / 171
```

Higher is easier by this heuristic. There is no comment bonus. This follows the
normalized MI family used in [Visual Studio metrics](https://learn.microsoft.com/en-us/visualstudio/code-quality/code-metrics-values),
with Kinal-specific token/line definitions; scores are not directly comparable.
Use maxima, percentiles and individual hotspots instead of judging a project by
total CC or a single composite quality score. HTML hotspot markers use advisory
CC > 10, cognitive > 15, or nesting > 4; these do not fail the command.

**Coupling/call graph:** uses resolved semantic types and call targets, unpacking
array/pointer/package types and excluding primitives and a type's own owner.
Counts include signature, field/base/interface, local, expression and callee-owner
types. Attribute dependencies are not included. Generic-instance information is
unioned without multiplying source counts. An unused generic body can have exact
syntax metrics and incomplete semantic metrics; inspect `bound` and
`bound_source_calls`. Calls are matched by AST identity so default argument copies
and lowering helpers are not mistaken for source calls. Fan-in counts unique
callers within scope; fan-out includes known external/builtin targets. Virtual
targets name the declared method, not every runtime implementation. Dynamic
targets are not guessed. Recursive components use only scoped direct edges and
therefore form a lower bound.

Physical lines exclude a phantom line after a trailing newline. Code/comment
counts overlap on mixed lines:
`lines = code_lines + comment_only_lines + blank_lines`.
Strings cannot create fake comments. These are source metrics: there are no
claims about runtime speed, actual allocation frequency, test coverage, defect
probability, duplication detection or complete dead-code analysis.

## CI and failure behavior

The optional `--max-cyclomatic`, `--max-cognitive`, `--max-nesting` caps are
nonnegative integers and fail only when a body's value **exceeds** the cap.
The report is still written and identifies violating source locations.

| Exit | Meaning |
| --- | --- |
| 0 | Analysis and export succeeded; configured caps passed |
| 1 | Project/parser/semantic or output I/O error |
| 2 | Invalid metrics argument |
| 3 | Analysis succeeded; at least one configured cap was exceeded |

Analysis errors do not replace an existing report. Source, manifest and package
paths are rejected as export destinations. A report always describes the selected
profile and available static evidence; missing semantic information is not
silently presented as a known zero.
