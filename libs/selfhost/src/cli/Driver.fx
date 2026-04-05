Unit SelfHost.Driver;

Get Console By IO.Console;
Get IO.File;
Get IO.Path;
Get IO.System;
Get IO.Text;
Get IO.Type.list;
Get SelfHost.Pipeline;
Get SelfHost.Backend.Native.HostApi;

Safe Function void PrintHelp() {
    Console.PrintLine("Kinal SelfHost");
    Console.PrintLine("Usage:");
    Console.PrintLine("  kinal-selfhost.exe --in <input.fx> [options]");
    Console.PrintLine("");
    Console.PrintLine("Options:");
    Console.PrintLine("  -o, --out <file>        output executable path (default: SelfHost.exe)");
    Console.PrintLine("  --phase <name>          lex | parse | sema | codegen | link | full");
    Console.PrintLine("  --backend <name>        host | self (default: host)");
    Console.PrintLine("  io <file>               alias of -o (compat)");
    Console.PrintLine("  --dump-tokens [file]    print tokens or write token dump to file");
    Console.PrintLine("  -h, --help              show this help");
}

Safe Function string TrimQuotes(string text) {
    string s = IO.Text.Trim(text);
    int n = IO.Text.Length(s);
    If (n >= 2 && IO.Text.StartsWith(s, [string]([char](34))) && IO.Text.EndsWith(s, [string]([char](34)))) {
        Return IO.Text.Slice(s, 1, n - 2);
    }
    Return s;
}

Safe Function IO.Type.list SplitCommandLine(string cmd) {
    IO.Type.list out = IO.Type.list.Create();
    char[] cs = cmd.ToChars();
    int n = cs.Length();
    int i = 0;

    While (i < n) {
        While (i < n) {
            int c0 = [int](cs.Index(i));
            If (c0 != 32 && c0 != 9) {
                Break;
            }
            i = i + 1;
        }
        If (i >= n) {
            Break;
        }

        bool quoted = false;
        If ([int](cs.Index(i)) == 34) {
            quoted = true;
            i = i + 1;
        }

        string tok = "";
        While (i < n) {
            int c = [int](cs.Index(i));
            If (quoted) {
                If (c == 34) {
                    i = i + 1;
                    Break;
                }
                tok = tok + [string]([char](c));
                i = i + 1;
                Continue;
            }

            If (c == 32 || c == 9) {
                Break;
            }
            tok = tok + [string]([char](c));
            i = i + 1;
        }

        tok = IO.Text.Trim(tok);
        If (!IO.Text.IsEmpty(tok)) {
            out.Add(tok);
        }
    }
    Return out;
}

Safe Function IO.Type.list CollectArgs(string[] args) {
    IO.Type.list out = IO.Type.list.Create();

    If (args.Length() > 0) {
        int i = 0;
        While (i < args.Length()) {
            string p = TrimQuotes([string](args.Index(i)));
            If (!IO.Text.IsEmpty(p)) {
                out.Add(p);
            }
            i = i + 1;
        }
        Return out;
    }

    string cmd = IO.System.CommandLine();
    IO.Type.list parts = SplitCommandLine(cmd);
    int j = 0;
    While (j < parts.Count()) {
        string p = TrimQuotes([string](parts.Fetch(j)));
        If (!IO.Text.IsEmpty(p)) {
            out.Add(p);
        }
        j = j + 1;
    }
    Return out;
}

Safe Function string Quote(string text) {
    Return [string]([char](34)) + text + [string]([char](34));
}

Safe Function int Run(string[] args) {
    string outputPath = "SelfHost.exe";
    string phase = "full";
    string backend = "host";
    bool dumpTokens = false;
    string dumpPath = "";
    bool autoLink = true;
    IO.Type.list inputPaths = IO.Type.list.Create();
    IO.Type.list linkPaths = IO.Type.list.Create();

    IO.Type.list argv = CollectArgs(args);
    int argc = argv.Count();
    int i = 0;

    If (argc > 0) {
        string first = [string](argv.Fetch(0));
        If (IO.Text.EndsWith(first, ".exe") || IO.Text.EndsWith(first, ".EXE")) {
            i = 1;
        }
    }

    While (i < argc) {
        string arg = [string](argv.Fetch(i));

        If (arg == "-h" || arg == "--help") {
            PrintHelp();
            Return 0;
        }

        If ((arg == "-o" || arg == "--out" || arg == "io") && (i + 1) < argc) {
            outputPath = TrimQuotes([string](argv.Fetch(i + 1)));
            i = i + 2;
            Continue;
        }

        If (arg == "--in" && (i + 1) < argc) {
            inputPaths.Add(TrimQuotes([string](argv.Fetch(i + 1))));
            i = i + 2;
            Continue;
        }

        If (arg == "--phase" && (i + 1) < argc) {
            phase = [string](argv.Fetch(i + 1));
            i = i + 2;
            Continue;
        }

        If (arg == "--backend" && (i + 1) < argc) {
            backend = [string](argv.Fetch(i + 1));
            i = i + 2;
            Continue;
        }

        If (arg == "--dump-tokens") {
            dumpTokens = true;
            If ((i + 1) < argc) {
                string next = [string](argv.Fetch(i + 1));
                If (!IO.Text.StartsWith(next, "-")) {
                    dumpPath = TrimQuotes(next);
                    i = i + 2;
                    Continue;
                }
            }
            i = i + 1;
            Continue;
        }

        If (arg == "--no-auto-link") {
            autoLink = false;
            i = i + 1;
            Continue;
        }

        If (arg == "--link" && (i + 1) < argc) {
            linkPaths.Add(TrimQuotes([string](argv.Fetch(i + 1))));
            i = i + 2;
            Continue;
        }

        If (IO.Text.StartsWith(arg, "-")) {
            Console.PrintLine("[SelfHost] unknown option: " + arg);
            PrintHelp();
            Return 1;
        }

        inputPaths.Add(TrimQuotes(arg));
        i = i + 1;
    }

    If (inputPaths.Count() <= 0) {
        PrintHelp();
        Return 1;
    }

    If (backend == "host" && phase == "full") {
        If (dumpTokens) {
            Console.PrintLine("[SelfHost] --dump-tokens is not available in host backend full mode");
            Return 1;
        }
        If (!SelfHost.Backend.Native.HostApi.CompileMany(inputPaths, outputPath, autoLink, linkPaths)) {
            Console.PrintLine("[SelfHost] host backend compile failed");
            Return 1;
        }
        Return 0;
    }

    string inputPath = [string](inputPaths.Fetch(0));
    int extraAt = 1;
    While (extraAt < inputPaths.Count()) {
        Console.PrintLine("[SelfHost] extra input ignored: " + [string](inputPaths.Fetch(extraAt)));
        extraAt = extraAt + 1;
    }

    Return SelfHost.Pipeline.Run(inputPath, outputPath, phase, backend, dumpTokens, dumpPath);
}

