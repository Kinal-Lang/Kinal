; Test-only ABI adapter. The compiler's internal Any aggregate argument is
; not the C by-value struct ABI (notably on Windows). Pass fixed-width scalar
; words to the C hook, without putting runtime semantics in production IR.
%KnAny = type { i64, i64 }
declare ptr @kn_test_any_to_string(i64, i64)
define ptr @__kn_any_to_string(%KnAny %value) {
entry:
  %tag = extractvalue %KnAny %value, 0
  %payload = extractvalue %KnAny %value, 1
  %text = call ptr @kn_test_any_to_string(i64 %tag, i64 %payload)
  ret ptr %text
}
