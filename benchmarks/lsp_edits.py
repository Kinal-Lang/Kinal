"""Time member completion while an earlier edit is waiting for full diagnostics."""
import argparse
import csv
import hashlib
from pathlib import Path
import statistics
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests/lsp"))
from run_lsp_tests import LspClient, assert_no_error_diags, initialize, line_col, path_to_uri, text_document


def benchmark(server, project, source, member, rounds):
    original = source.read_text(encoding="utf-8")
    line_start = original.rfind("\n", 0, original.index(member)) + 1
    receiver, dot, expected = member.rpartition(".")
    if not (receiver and dot and expected):
        raise ValueError("member must be a receiver followed by a member name, e.g. form.Run")
    uri = path_to_uri(source)
    document = {"textDocument": {"uri": uri}}
    with tempfile.TemporaryDirectory() as tmp, LspClient(server, project, Path(tmp) / "lsp.jsonl", 60) as client:
        initialize(client, project)
        version = 1
        client.notify("textDocument/didOpen", {"textDocument": text_document(source, original, version)})

        def validate_result(seq=0):
            ok, detail = assert_no_error_diags(client.wait_for_diagnostics(uri, seq))
            if not ok:
                raise ValueError(detail)

        def change(text):
            nonlocal version
            version += 1
            client.notify("textDocument/didChange", {
                "textDocument": {"uri": uri, "version": version},
                "contentChanges": [{"text": text}], "kinalDeferDiagnostics": True,
            })

        def validate():
            client.notify("kinal/validateDocument", {"uri": uri, "version": version})

        def complete(position):
            result, _ = client.request("textDocument/completion", {**document, "position": position})
            if not any(item["label"] == expected for item in result["items"]):
                raise ValueError(f"missing completion {expected!r} at {position}")

        validate_result()
        for i in range(rounds + 1):
            prefix = original[:line_start] + f"// edit {i}\n"
            first = prefix + original[line_start:]
            second = prefix + "\n" + original[line_start:]
            line, col = line_col(second, member, len(receiver) + 1)
            position = {"line": line, "character": col}
            change(first)
            client.request("textDocument/semanticTokens/full", document)
            start = time.perf_counter()
            validate()
            change(second)
            complete(position)
            queued = time.perf_counter()
            complete(position)
            idle = time.perf_counter()
            seq = client.diag_seq.get(uri, 0)
            validate()
            validate_result(seq)
            end = time.perf_counter()
            if i:
                yield [i, 1000 * (queued - start), 1000 * (idle - queued), 1000 * (end - idle)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--file", type=Path, default=Path("src/Main.kn"), help="relative to the project")
    parser.add_argument("--member", default="form.Run", help="first occurrence, with the cursor immediately after the last dot")
    parser.add_argument("--rounds", type=int, default=9)
    parser.add_argument("--output", type=Path, help="write individual samples to CSV")
    args = parser.parse_args()
    if args.rounds < 1:
        parser.error("--rounds must be positive")
    server, project = args.server.resolve(), args.project.resolve()
    rows = list(benchmark(server, project, project / args.file, args.member, args.rounds))
    names = ["round", "queued_completion_ms", "idle_completion_ms", "full_diagnostics_ms"]
    digest = hashlib.sha256(server.read_bytes()).hexdigest()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", newline="", encoding="utf-8") as out:
            writer = csv.writer(out)
            writer.writerow(["server_sha256", *names])
            writer.writerows([digest, *row] for row in rows)
    print(f"{server} ({digest})")
    for name, samples in zip(names[1:], list(zip(*rows))[1:]):
        print(f"{name}: {statistics.median(samples):.2f}")


if __name__ == "__main__":
    main()
