Unit SelfHost.Pipeline;

Get Console By IO.Console;
Get IO.File;
Get IO.Text;
Get IO.Type.list;
Get SelfHost.Source;
Get SelfHost.Diag;
Get SelfHost.Lexer;
Get SelfHost.Parser;
Get SelfHost.Sema;
Get SelfHost.Codegen;
Get SelfHost.Link;
Get SelfHost.Backend.Native.HostApi;

Safe Function bool EmitTokenDump(IO.Type.list tokens, string outputPath) {
    IO.Type.list lines = IO.Type.list.Create();
    int i = 0;
    While (i < tokens.Count()) {
        string token = [string](tokens.Fetch(i));
        lines.Add(SelfHost.Lexer.TokenDumpLine(token));
        i = i + 1;
    }

    If (IO.File.WriteLines(outputPath, lines)) {
        Console.PrintLine("[SelfHost] token dump written: " + outputPath);
        Return true;
    }

    Console.PrintLine("[SelfHost] failed to write token dump: " + outputPath);
    Return false;
}

Safe Function void PrintTokenDump(IO.Type.list tokens) {
    int i = 0;
    While (i < tokens.Count()) {
        string token = [string](tokens.Fetch(i));
        Console.PrintLine(SelfHost.Lexer.TokenDumpLine(token));
        i = i + 1;
    }
}

Safe Function int Run(string inputPath, string outputPath, string phase, string backend, bool dumpTokens, string dumpPath) {
    If (!IO.File.Exists(inputPath)) {
        Console.PrintLine("[SelfHost] input not found: " + inputPath);
        Return 1;
    }

    IO.Type.list diagnostics = IO.Type.list.Create();

    string useBackend = backend;
    If (IO.Text.IsEmpty(useBackend)) {
        useBackend = "host";
    }

    If (useBackend == "host" && phase == "full") {
        If (dumpTokens) {
            Console.PrintLine("[SelfHost] --dump-tokens is not available in host backend full mode");
            Return 1;
        }
        If (!SelfHost.Backend.Native.HostApi.Compile(inputPath, outputPath)) {
            diagnostics.Add("[Backend] host backend compile failed");
            SelfHost.Diag.PrintAll(diagnostics);
            Return 1;
        }
        Return 0;
    }

    If (useBackend != "self" && useBackend != "host") {
        diagnostics.Add("[Backend] Unsupported backend: " + useBackend + " (expected host or self)");
        SelfHost.Diag.PrintAll(diagnostics);
        Return 1;
    }

    SourceFile source = SelfHost.Source.Load(inputPath);
    IO.Type.list tokens = SelfHost.Lexer.Tokenize(source, diagnostics);

    If (SelfHost.Diag.HasAny(diagnostics)) {
        SelfHost.Diag.PrintAll(diagnostics);
        Return 1;
    }

    If (dumpTokens) {
        If (IO.Text.IsEmpty(dumpPath)) {
            PrintTokenDump(tokens);
        } Else {
            If (!EmitTokenDump(tokens, dumpPath)) {
                Return 1;
            }
        }
    }

    If (phase == "lex") {
        Return 0;
    }

    If (!SelfHost.Parser.Run(source, tokens, diagnostics)) {
        SelfHost.Diag.PrintAll(diagnostics);
        Return 1;
    }
    If (phase == "parse") {
        Return 0;
    }

    If (!SelfHost.Sema.Run(source, tokens, diagnostics)) {
        SelfHost.Diag.PrintAll(diagnostics);
        Return 1;
    }
    If (phase == "sema") {
        Return 0;
    }

    If (phase == "codegen" || phase == "link" || phase == "full") {
        If (useBackend != "self") {
            diagnostics.Add("[Backend] phase '" + phase + "' currently requires --backend self");
            SelfHost.Diag.PrintAll(diagnostics);
            Return 1;
        }

        string llPath = outputPath + ".self.ll";
        If (!SelfHost.Codegen.Run(source, tokens, llPath, diagnostics)) {
            SelfHost.Diag.PrintAll(diagnostics);
            Return 1;
        }

        If (phase == "codegen") {
            Console.PrintLine("[SelfHost] self backend LLVM IR generated: " + llPath);
            Return 0;
        }

        If (!SelfHost.Link.Run(llPath, outputPath, diagnostics)) {
            SelfHost.Diag.PrintAll(diagnostics);
            Return 1;
        }
        Return 0;
    }

    Console.PrintLine("[SelfHost] unknown phase: " + phase);
    Return 1;
}
