Unit SelfHost.Codegen;

Get IO.Text;
Get IO.File;
Get IO.Type.list;
Get SelfHost.Source;
Get SelfHost.Diag;
Get SelfHost.Frontend.Token;
Get SelfHost.Frontend.TokenType;

Class Cur {
    Public IO.Type.list T;
    Public int P;

    Public Safe Constructor(IO.Type.list tokens) {
        This.T = tokens;
        This.P = 0;
    }
}

Class Expr {
    Public string K;
    Public string V;

    Public Safe Constructor(string kind, string value) {
        This.K = kind;
        This.V = value;
    }
}

Class St {
    Public IO.Type.list G;
    Public IO.Type.list F;
    Public IO.Type.list VN;
    Public IO.Type.list VK;
    Public IO.Type.list VS;
    Public IO.Type.list FN;
    Public IO.Type.list FR;
    Public IO.Type.list FA;
    Public IO.Type.list SK;
    Public IO.Type.list SN;
    Public IO.Type.list SS;
    Public int TID;
    Public int LID;
    Public bool Term;

    Public Safe Constructor() {
        This.G = IO.Type.list.Create();
        This.F = IO.Type.list.Create();
        This.VN = IO.Type.list.Create();
        This.VK = IO.Type.list.Create();
        This.VS = IO.Type.list.Create();
        This.FN = IO.Type.list.Create();
        This.FR = IO.Type.list.Create();
        This.FA = IO.Type.list.Create();
        This.SK = IO.Type.list.Create();
        This.SN = IO.Type.list.Create();
        This.SS = IO.Type.list.Create();
        This.TID = 0;
        This.LID = 0;
        This.Term = false;

        string q = [string]([char](34));
        string bs = [string]([char](92));
        This.G.Add("@.fx.empty = private unnamed_addr constant [1 x i8] c" + q + bs + "00" + q + ", align 1");
        This.G.Add("@.fx.nl = private unnamed_addr constant [3 x i8] c" + q + bs + "0D" + bs + "0A" + bs + "00" + q + ", align 1");
        This.G.Add("@.fx.minus = private unnamed_addr constant [2 x i8] c" + q + "-" + bs + "00" + q + ", align 1");
        This.G.Add("@.fx.true = private unnamed_addr constant [5 x i8] c" + q + "true" + bs + "00" + q + ", align 1");
        This.G.Add("@.fx.false = private unnamed_addr constant [6 x i8] c" + q + "false" + bs + "00" + q + ", align 1");
    }
}

Safe Function int Count(Cur c) {
    Return c.T.Count();
}

Safe Function string RawAt(Cur c, int i) {
    If (i < 0 || i >= Count(c)) {
        Return "";
    }
    Return [string](c.T.Fetch(i));
}

Safe Function int KAt(Cur c, int i) {
    string r = RawAt(c, i);
    If (IO.Text.IsEmpty(r)) {
        Return [int](SelfHost.Frontend.TokenType.TokenType.Eof);
    }
    Return SelfHost.Frontend.Token.Kind(r);
}

Safe Function string TAt(Cur c, int i) {
    string r = RawAt(c, i);
    If (IO.Text.IsEmpty(r)) {
        Return "";
    }
    Return SelfHost.Frontend.Token.Text(r);
}

Safe Function int LAt(Cur c, int i) {
    string r = RawAt(c, i);
    If (IO.Text.IsEmpty(r)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Line(r);
}

Safe Function int CAt(Cur c, int i) {
    string r = RawAt(c, i);
    If (IO.Text.IsEmpty(r)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Col(r);
}

Safe Function int K(Cur c) { Return KAt(c, c.P); }
Safe Function string Tx(Cur c) { Return TAt(c, c.P); }
Safe Function int Peek(Cur c, int off) { Return KAt(c, c.P + off); }
Safe Function bool End(Cur c) { Return K(c) == [int](SelfHost.Frontend.TokenType.TokenType.Eof); }

Safe Function void Adv(Cur c) {
    If (c.P < Count(c)) {
        c.P = c.P + 1;
    }
}

Safe Function bool M(Cur c, int kind) {
    If (K(c) == kind) {
        Adv(c);
        Return true;
    }
    Return false;
}

Safe Function int Span(Cur c) {
    int n = IO.Text.Length(Tx(c));
    If (n < 1) Return 1;
    Return n;
}

Safe Function void D(IO.Type.list ds, SourceFile s, Cur c, string t, string d) {
    SelfHost.Diag.Add(ds, s, "Codegen", t, d, Tx(c), LAt(c, c.P), CAt(c, c.P), Span(c));
}

Safe Function bool E(Cur c, IO.Type.list ds, SourceFile s, int kind, string t, string d) {
    If (K(c) == kind) {
        Adv(c);
        Return true;
    }
    D(ds, s, c, t, d);
    Return false;
}

Safe Function string Tmp(St st) {
    string n = "%t" + [string](st.TID);
    st.TID = st.TID + 1;
    Return n;
}

Safe Function string Lab(St st, string p) {
    string n = p + "_" + [string](st.LID);
    st.LID = st.LID + 1;
    Return n;
}

Safe Function void I(St st, string line) {
    st.F.Add("  " + line);
}

Safe Function void Label(St st, string name) {
    st.F.Add(name + ":");
    st.Term = false;
}

Safe Function void Br(St st, string name) {
    I(st, "br label %" + name);
    st.Term = true;
}

Safe Function void Ret(St st, string v) {
    I(st, "ret i64 " + v);
    st.Term = true;
}

Safe Function int FindVar(St st, string n) {
    int i = 0;
    While (i < st.VN.Count()) {
        If ([string](st.VN.Fetch(i)) == n) Return i;
        i = i + 1;
    }
    Return -1;
}

Safe Function void ResetFunctionState(St st) {
    st.VN = IO.Type.list.Create();
    st.VK = IO.Type.list.Create();
    st.VS = IO.Type.list.Create();
    st.TID = 0;
    st.LID = 0;
    st.Term = false;
}

Safe Function int FindFunction(St st, string n) {
    int i = 0;
    While (i < st.FN.Count()) {
        If ([string](st.FN.Fetch(i)) == n) Return i;
        i = i + 1;
    }
    Return -1;
}

Safe Function void RegisterFunction(St st, string n, string retKind, int argc) {
    int at = FindFunction(st, n);
    If (at >= 0) {
        st.FR.Set(at, retKind);
        st.FA.Set(at, [string](argc));
        Return;
    }
    st.FN.Add(n);
    st.FR.Add(retKind);
    st.FA.Add([string](argc));
}

Safe Function string NormalizeValueType(string raw) {
    If (raw == "string" || raw == "IO.Type.string") Return "string";
    If (raw == "bool" || raw == "IO.Type.bool") Return "bool";
    If (raw == "int" || raw == "i8" || raw == "i16" || raw == "i32" || raw == "i64" ||
        raw == "u8" || raw == "u16" || raw == "u32" || raw == "u64" ||
        raw == "isize" || raw == "usize" || raw == "byte" || raw == "char" ||
        raw == "IO.Type.int" || raw == "IO.Type.byte" || raw == "IO.Type.char") Return "int";
    Return "";
}

Safe Function string BuildTypeText(Cur c, int start, int endExclusive) {
    string out = "";
    int i = start;
    While (i < endExclusive) {
        out = out + TAt(c, i);
        i = i + 1;
    }
    Return out;
}

Safe Function void SetVar(St st, string n, string k, string s) {
    int i = FindVar(st, n);
    If (i < 0) {
        st.VN.Add(n);
        st.VK.Add(k);
        st.VS.Add(s);
        Return;
    }
    st.VK.Set(i, k);
    st.VS.Set(i, s);
}

Safe Function string Hex(int v) {
    If (v < 10) Return [string]([char](48 + v));
    Return [string]([char](65 + v - 10));
}

Safe Function string Esc(string x) {
    char[] cs = x.ToChars();
    int i = 0;
    string out = "";
    While (i < cs.Length()) {
        int c = [int](cs.Index(i));
        If (c == 34 || c == 92 || c < 32 || c > 126) {
            out = out + [string]([char](92)) + Hex((c / 16) % 16) + Hex(c % 16);
        } Else {
            out = out + [string]([char](c));
        }
        i = i + 1;
    }
    Return out;
}

Safe Function string DecodeStringBody(string tok) {
    int n = IO.Text.Length(tok);
    string body = "";
    If (n >= 2) {
        body = IO.Text.Slice(tok, 1, n - 2);
    }

    char[] cs = body.ToChars();
    int i = 0;
    string out = "";
    While (i < cs.Length()) {
        int c = [int](cs.Index(i));
        If (c == 92 && i + 1 < cs.Length()) {
            int e = [int](cs.Index(i + 1));
            If (e == 48) out = out + [string]([char](0));
            Else If (e == 110) out = out + [string]([char](10));
            Else If (e == 114) out = out + [string]([char](13));
            Else If (e == 116) out = out + [string]([char](9));
            Else If (e == 92) out = out + [string]([char](92));
            Else If (e == 34) out = out + [string]([char](34));
            Else If (e == 39) out = out + [string]([char](39));
            Else out = out + [string]([char](e));
            i = i + 2;
            Continue;
        }
        out = out + [string]([char](c));
        i = i + 1;
    }
    Return out;
}

Safe Function string Ptr(string name, int size) {
    Return "getelementptr inbounds ([" + [string](size) + " x i8], ptr @" + name + ", i64 0, i64 0)";
}

Safe Function string StrLit(St st, string tok) {
    int i = 0;
    While (i < st.SK.Count()) {
        If ([string](st.SK.Fetch(i)) == tok) {
            Return Ptr([string](st.SN.Fetch(i)), [int]([string](st.SS.Fetch(i))));
        }
        i = i + 1;
    }

    string body = DecodeStringBody(tok);
    string e = Esc(body);
    int sz = IO.Text.Length(body) + 1;
    string name = ".fx.s" + [string](st.SK.Count());

    st.SK.Add(tok);
    st.SN.Add(name);
    st.SS.Add([string](sz));
    string q = [string]([char](34));
    string bs = [string]([char](92));
    st.G.Add("@" + name + " = private unnamed_addr constant [" + [string](sz) + " x i8] c" + q + e + bs + "00" + q + ", align 1");

    Return Ptr(name, sz);
}

Safe Function Expr Bad() { Return New Expr("bad", ""); }
Safe Function bool IsBad(Expr e) { Return e.K == "bad"; }

Safe Function string AsI64(Expr e, St st, SourceFile s, Cur c, IO.Type.list ds) {
    If (e.K == "int") Return e.V;
    If (e.K == "bool") {
        string t = Tmp(st);
        I(st, t + " = zext i1 " + e.V + " to i64");
        Return t;
    }
    D(ds, s, c, "Type Mismatch", "Expected int/bool");
    Return "0";
}

Safe Function string AsI1(Expr e, St st, SourceFile s, Cur c, IO.Type.list ds) {
    If (e.K == "bool") Return e.V;
    If (e.K == "int") {
        string t = Tmp(st);
        I(st, t + " = icmp ne i64 " + e.V + ", 0");
        Return t;
    }
    D(ds, s, c, "Type Mismatch", "Expected bool/int condition");
    Return "0";
}

Safe Function Expr EmitUserCall(Cur c, St st, SourceFile s, IO.Type.list ds, string name) {
    int fnAt = FindFunction(st, name);
    If (fnAt < 0) {
        D(ds, s, c, "Unknown Identifier", "Function is not defined");
        Return Bad();
    }

    Adv(c);
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "Function call must start with '('")) Return Bad();

    IO.Type.list args = IO.Type.list.Create();
    If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
        While (true) {
            Expr arg = PExpr(c, st, s, ds);
            If (IsBad(arg)) Return Bad();
            args.Add(AsI64(arg, st, s, c, ds));
            If (!M(c, [int](SelfHost.Frontend.TokenType.TokenType.Comma))) {
                Break;
            }
        }
    }

    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "Function call must end with ')'")) Return Bad();

    int expectCount = [int]([string](st.FA.Fetch(fnAt)));
    If (args.Count() != expectCount) {
        D(ds, s, c, "Argument Count", "Call argument count does not match function definition");
        Return Bad();
    }

    string callArgs = "";
    int i = 0;
    While (i < args.Count()) {
        If (i > 0) callArgs = callArgs + ", ";
        callArgs = callArgs + "i64 " + [string](args.Fetch(i));
        i = i + 1;
    }

    string ret = [string](st.FR.Fetch(fnAt));
    If (ret == "string") {
        D(ds, s, c, "Unsupported Call", "String-returning user functions are not implemented in self LLVM backend");
        Return Bad();
    }

    string tmp = Tmp(st);
    I(st, tmp + " = call i64 @__fx_fn_" + name + "(" + callArgs + ")");
    If (ret == "bool") Return New Expr("bool", tmp);
    Return New Expr("int", tmp);
}

Safe Function Expr PPrimary(Cur c, St st, SourceFile s, IO.Type.list ds) {
    int k = K(c);
    If (k == [int](SelfHost.Frontend.TokenType.TokenType.Number)) {
        string t = Tx(c);
        Adv(c);
        If (IO.Text.StartsWith(t, "0x") || IO.Text.StartsWith(t, "0X")) {
            Return New Expr("int", [string]([int](t)));
        }
        If (IO.Text.Contains(t, ".") || IO.Text.EndsWith(t, "f") || IO.Text.EndsWith(t, "F")) {
            D(ds, s, c, "Unsupported Literal", "Float literal is not supported in self LLVM backend");
            Return Bad();
        }
        Return New Expr("int", [string]([int](t)));
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.String)) {
        string p = StrLit(st, Tx(c));
        Adv(c);
        Return New Expr("string", p);
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwTrue)) {
        Adv(c);
        Return New Expr("bool", "1");
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwFalse)) {
        Adv(c);
        Return New Expr("bool", "0");
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.Identifier)) {
        If (Peek(c, 1) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            Return EmitUserCall(c, st, s, ds, Tx(c));
        }
        If (Peek(c, 1) == [int](SelfHost.Frontend.TokenType.TokenType.Dot)) {
            D(ds, s, c, "Unsupported Expression", "Qualified/call expression not implemented in self LLVM backend");
            Return Bad();
        }
        string n = Tx(c);
        int at = FindVar(st, n);
        If (at < 0) {
            D(ds, s, c, "Unknown Identifier", "Variable is not defined");
            Return Bad();
        }
        string kind = [string](st.VK.Fetch(at));
        string slot = [string](st.VS.Fetch(at));
        Adv(c);
        string t2 = Tmp(st);
        If (kind == "string") {
            I(st, t2 + " = load ptr, ptr " + slot);
            Return New Expr("string", t2);
        }
        I(st, t2 + " = load i64, ptr " + slot);
        Return New Expr("int", t2);
    }

    If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.LParen))) {
        Expr x = PExpr(c, st, s, ds);
        If (IsBad(x)) Return x;
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "Expression must close with ')'")) Return Bad();
        Return x;
    }

    D(ds, s, c, "Expected Expression", "Unable to parse expression");
    Return Bad();
}

Safe Function Expr PMul(Cur c, St st, SourceFile s, IO.Type.list ds) {
    Expr l = PPrimary(c, st, s, ds);
    If (IsBad(l)) Return l;

    While (true) {
        int op = K(c);
        bool isMul = op == [int](SelfHost.Frontend.TokenType.TokenType.Star) ||
                     op == [int](SelfHost.Frontend.TokenType.TokenType.Slash) ||
                     op == [int](SelfHost.Frontend.TokenType.TokenType.Percent);
        If (!isMul) Break;

        Adv(c);
        Expr r = PPrimary(c, st, s, ds);
        If (IsBad(r)) Return r;
        If (l.K == "string" || r.K == "string") {
            D(ds, s, c, "Type Mismatch", "Arithmetic operators only support integer operands");
            Return Bad();
        }

        string li = AsI64(l, st, s, c, ds);
        string ri = AsI64(r, st, s, c, ds);
        string t = Tmp(st);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Star)) I(st, t + " = mul i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Slash)) I(st, t + " = sdiv i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Percent)) I(st, t + " = srem i64 " + li + ", " + ri);
        l = New Expr("int", t);
    }

    Return l;
}

Safe Function Expr PAdd(Cur c, St st, SourceFile s, IO.Type.list ds) {
    Expr l = PMul(c, st, s, ds);
    If (IsBad(l)) Return l;

    While (true) {
        int op = K(c);
        bool isAdd = op == [int](SelfHost.Frontend.TokenType.TokenType.Plus) ||
                     op == [int](SelfHost.Frontend.TokenType.TokenType.Minus);
        If (!isAdd) Break;

        Adv(c);
        Expr r = PMul(c, st, s, ds);
        If (IsBad(r)) Return r;
        If (l.K == "string" || r.K == "string") {
            D(ds, s, c, "Type Mismatch", "Arithmetic operators only support integer operands");
            Return Bad();
        }

        string li = AsI64(l, st, s, c, ds);
        string ri = AsI64(r, st, s, c, ds);
        string t = Tmp(st);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Plus)) I(st, t + " = add i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Minus)) I(st, t + " = sub i64 " + li + ", " + ri);
        l = New Expr("int", t);
    }

    Return l;
}

Safe Function Expr PCmp(Cur c, St st, SourceFile s, IO.Type.list ds) {
    Expr l = PAdd(c, st, s, ds);
    If (IsBad(l)) Return l;

    While (true) {
        int op = K(c);
        bool isCmp = op == [int](SelfHost.Frontend.TokenType.TokenType.Eq) || op == [int](SelfHost.Frontend.TokenType.TokenType.Ne) ||
                     op == [int](SelfHost.Frontend.TokenType.TokenType.Lt) || op == [int](SelfHost.Frontend.TokenType.TokenType.Le) ||
                     op == [int](SelfHost.Frontend.TokenType.TokenType.Gt) || op == [int](SelfHost.Frontend.TokenType.TokenType.Ge);
        If (!isCmp) Break;

        Adv(c);
        Expr r = PAdd(c, st, s, ds);
        If (IsBad(r)) Return r;

        If (l.K == "string" || r.K == "string") {
            If (l.K != "string" || r.K != "string") {
                D(ds, s, c, "Type Mismatch", "String comparison requires two string operands");
                Return Bad();
            }
            string cmp = Tmp(st);
            I(st, cmp + " = call i32 @fx_strcmp(ptr " + l.V + ", ptr " + r.V + ")");
            string t = Tmp(st);
            If (op == [int](SelfHost.Frontend.TokenType.TokenType.Eq)) I(st, t + " = icmp eq i32 " + cmp + ", 0");
            If (op == [int](SelfHost.Frontend.TokenType.TokenType.Ne)) I(st, t + " = icmp ne i32 " + cmp + ", 0");
            If (op != [int](SelfHost.Frontend.TokenType.TokenType.Eq) && op != [int](SelfHost.Frontend.TokenType.TokenType.Ne)) {
                D(ds, s, c, "Type Mismatch", "Only == and != are allowed for string");
                Return Bad();
            }
            l = New Expr("bool", t);
            Continue;
        }

        string li = AsI64(l, st, s, c, ds);
        string ri = AsI64(r, st, s, c, ds);
        string t2 = Tmp(st);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Eq)) I(st, t2 + " = icmp eq i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Ne)) I(st, t2 + " = icmp ne i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Lt)) I(st, t2 + " = icmp slt i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Le)) I(st, t2 + " = icmp sle i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Gt)) I(st, t2 + " = icmp sgt i64 " + li + ", " + ri);
        If (op == [int](SelfHost.Frontend.TokenType.TokenType.Ge)) I(st, t2 + " = icmp sge i64 " + li + ", " + ri);
        l = New Expr("bool", t2);
    }

    Return l;
}

Safe Function Expr PAnd(Cur c, St st, SourceFile s, IO.Type.list ds) {
    Expr l = PCmp(c, st, s, ds);
    If (IsBad(l)) Return l;

    While (M(c, [int](SelfHost.Frontend.TokenType.TokenType.AndAnd))) {
        string lb = AsI1(l, st, s, c, ds);
        string rhsLabel = Lab(st, "land.rhs");
        string falseLabel = Lab(st, "land.false");
        string mergeLabel = Lab(st, "land.merge");
        string slot = Tmp(st);
        I(st, slot + " = alloca i1");
        I(st, "br i1 " + lb + ", label %" + rhsLabel + ", label %" + falseLabel);

        Label(st, falseLabel);
        I(st, "store i1 0, ptr " + slot);
        Br(st, mergeLabel);

        Label(st, rhsLabel);
        Expr r = PCmp(c, st, s, ds);
        If (IsBad(r)) Return r;
        string rb = AsI1(r, st, s, c, ds);
        I(st, "store i1 " + rb + ", ptr " + slot);
        Br(st, mergeLabel);

        Label(st, mergeLabel);
        string t = Tmp(st);
        I(st, t + " = load i1, ptr " + slot);
        l = New Expr("bool", t);
    }

    Return l;
}

Safe Function Expr PExpr(Cur c, St st, SourceFile s, IO.Type.list ds) {
    Expr l = PAnd(c, st, s, ds);
    If (IsBad(l)) Return l;

    While (M(c, [int](SelfHost.Frontend.TokenType.TokenType.OrOr))) {
        string lb = AsI1(l, st, s, c, ds);
        string rhsLabel = Lab(st, "lor.rhs");
        string trueLabel = Lab(st, "lor.true");
        string mergeLabel = Lab(st, "lor.merge");
        string slot = Tmp(st);
        I(st, slot + " = alloca i1");
        I(st, "br i1 " + lb + ", label %" + trueLabel + ", label %" + rhsLabel);

        Label(st, trueLabel);
        I(st, "store i1 1, ptr " + slot);
        Br(st, mergeLabel);

        Label(st, rhsLabel);
        Expr r = PAnd(c, st, s, ds);
        If (IsBad(r)) Return r;
        string rb = AsI1(r, st, s, c, ds);
        I(st, "store i1 " + rb + ", ptr " + slot);
        Br(st, mergeLabel);

        Label(st, mergeLabel);
        string t = Tmp(st);
        I(st, t + " = load i1, ptr " + slot);
        l = New Expr("bool", t);
    }

    Return l;
}

Safe Function bool EmitPrint(St st, Expr e, bool line, SourceFile s, Cur c, IO.Type.list ds) {
    string lb = "0";
    If (line) lb = "1";

    If (e.K == "string") {
        I(st, "call void @__fx_print_str(ptr " + e.V + ", i1 " + lb + ")");
        Return true;
    }

    If (e.K == "bool") {
        string t = Tmp(st);
        I(st, t + " = select i1 " + e.V + ", ptr " + Ptr(".fx.true", 5) + ", ptr " + Ptr(".fx.false", 6));
        I(st, "call void @__fx_print_str(ptr " + t + ", i1 " + lb + ")");
        Return true;
    }

    string iv = AsI64(e, st, s, c, ds);
    I(st, "call void @__fx_print_i64(i64 " + iv + ", i1 " + lb + ")");
    Return true;
}

Safe Function bool ParsePrintCall(Cur c, St st, SourceFile s, IO.Type.list ds) {
    int save = c.P;
    If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.Identifier)) Return false;

    string q = Tx(c);
    Adv(c);
    While (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Dot))) {
        If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.Identifier)) {
            D(ds, s, c, "Expected Identifier", "Missing identifier after '.'");
            Return false;
        }
        q = q + "." + Tx(c);
        Adv(c);
    }

    If (!M(c, [int](SelfHost.Frontend.TokenType.TokenType.LParen))) {
        c.P = save;
        Return false;
    }

    bool line = q == "PrintLine" || q == "Console.PrintLine" || q == "IO.Console.PrintLine";
    bool okName = line || q == "Print" || q == "Console.Print" || q == "IO.Console.Print";
    If (!okName) {
        D(ds, s, c, "Unsupported Call", "Only Console.Print/PrintLine are supported in self LLVM backend");
        Return false;
    }

    Expr e = New Expr("string", Ptr(".fx.empty", 1));
    bool hasArg = false;
    If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
        e = PExpr(c, st, s, ds);
        If (IsBad(e)) Return false;
        hasArg = true;
        If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Comma))) {
            D(ds, s, c, "Unsupported Call", "Variadic Print is not implemented in self LLVM backend");
            Return false;
        }
    }

    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "Call must end with ')'")) Return false;
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after call")) Return false;

    If (!hasArg) {
        e = New Expr("string", Ptr(".fx.empty", 1));
    }
    Return EmitPrint(st, e, line, s, c, ds);
}

Safe Function bool ParseVar(Cur c, St st, SourceFile s, IO.Type.list ds) {
    bool infer = M(c, [int](SelfHost.Frontend.TokenType.TokenType.KwVar));
    string dt = "";
    If (!infer) {
        dt = Tx(c);
        Adv(c);
    }

    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Variable declaration requires a name")) Return false;
    string name = TAt(c, c.P - 1);

    Expr init = Bad();
    bool hasInit = false;
    If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Assign))) {
        hasInit = true;
        init = PExpr(c, st, s, ds);
        If (IsBad(init)) Return false;
    }

    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after variable declaration")) Return false;

    string vk = "int";
    If (infer) {
        If (hasInit && init.K == "string") vk = "string";
        If (hasInit && init.K == "bool") vk = "bool";
    } Else {
        vk = NormalizeValueType(dt);
        If (IO.Text.IsEmpty(vk)) {
            D(ds, s, c, "Unsupported Type", "Only int/string declarations are supported in self LLVM backend");
            Return false;
        }
    }

    If (vk == "string") {
        string slot = Tmp(st);
        I(st, slot + " = alloca ptr");
        string vv = Ptr(".fx.empty", 1);
        If (hasInit) {
            If (init.K != "string") {
                D(ds, s, c, "Type Mismatch", "String variable needs string initializer");
                Return false;
            }
            vv = init.V;
        }
        I(st, "store ptr " + vv + ", ptr " + slot);
        SetVar(st, name, "string", slot);
        Return true;
    }

    string slot2 = Tmp(st);
    I(st, slot2 + " = alloca i64");
    string iv = "0";
    If (hasInit) iv = AsI64(init, st, s, c, ds);
    I(st, "store i64 " + iv + ", ptr " + slot2);
    SetVar(st, name, "int", slot2);
    Return true;
}

Safe Function bool ParseAssign(Cur c, St st, SourceFile s, IO.Type.list ds) {
    If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.Identifier) || Peek(c, 1) != [int](SelfHost.Frontend.TokenType.TokenType.Assign)) Return false;

    string n = Tx(c);
    int at = FindVar(st, n);
    If (at < 0) {
        D(ds, s, c, "Unknown Identifier", "Assignment target variable is not defined");
        Return false;
    }

    Adv(c);
    Adv(c);
    Expr e = PExpr(c, st, s, ds);
    If (IsBad(e)) Return false;
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after assignment")) Return false;

    string kind = [string](st.VK.Fetch(at));
    string slot = [string](st.VS.Fetch(at));
    If (kind == "string") {
        If (e.K != "string") {
            D(ds, s, c, "Type Mismatch", "String assignment requires string value");
            Return false;
        }
        I(st, "store ptr " + e.V + ", ptr " + slot);
        Return true;
    }

    I(st, "store i64 " + AsI64(e, st, s, c, ds) + ", ptr " + slot);
    Return true;
}

Safe Function bool ParseParamList(Cur c, SourceFile s, IO.Type.list ds, IO.Type.list outNames, IO.Type.list outKinds) {
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "Function parameter list must start with '('")) Return false;
    If (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
        Adv(c);
        Return true;
    }

    While (true) {
        int start = c.P;
        int nameAt = -1;
        While (!End(c) && K(c) != [int](SelfHost.Frontend.TokenType.TokenType.Comma) && K(c) != [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
            If (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.Identifier)) {
                nameAt = c.P;
            }
            Adv(c);
        }

        If (nameAt <= start) {
            D(ds, s, c, "Expected Identifier", "Parameter list requires '<type> <name>'");
            Return false;
        }

        string typeText = BuildTypeText(c, start, nameAt);
        string typeKind = NormalizeValueType(typeText);
        If (IO.Text.IsEmpty(typeKind)) {
            D(ds, s, c, "Unsupported Type", "Parameter type is not implemented in self LLVM backend");
            Return false;
        }

        outNames.Add(TAt(c, nameAt));
        outKinds.Add(typeKind);

        If (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.Comma)) {
            Adv(c);
            Continue;
        }
        Break;
    }

    Return E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "Function parameter list must end with ')'");
}

Safe Function void EmitParamPrologue(St st, IO.Type.list paramNames, IO.Type.list paramKinds) {
    int i = 0;
    While (i < paramNames.Count()) {
        string name = [string](paramNames.Fetch(i));
        string kind = [string](paramKinds.Fetch(i));
        string slot = Tmp(st);
        If (kind == "string") {
            I(st, slot + " = alloca ptr");
            I(st, "store ptr %p" + [string](i) + ", ptr " + slot);
            SetVar(st, name, "string", slot);
        } Else {
            I(st, slot + " = alloca i64");
            I(st, "store i64 %p" + [string](i) + ", ptr " + slot);
            SetVar(st, name, "int", slot);
        }
        i = i + 1;
    }
}

Safe Function bool ParseUserFunction(Cur c, St st, SourceFile s, IO.Type.list ds, string fn, string retKind, IO.Type.list paramNames, IO.Type.list paramKinds) {
    If (retKind == "string") {
        D(ds, s, c, "Unsupported Type", "String-returning functions are not implemented in self LLVM backend");
        Return false;
    }
    If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        D(ds, s, c, "Expected Left Brace", "Function body must start with '{'");
        Return false;
    }

    ResetFunctionState(st);

    string sig = "";
    int i = 0;
    While (i < paramNames.Count()) {
        If (i > 0) sig = sig + ", ";
        string kind = [string](paramKinds.Fetch(i));
        If (kind == "string") sig = sig + "ptr %p" + [string](i);
        Else sig = sig + "i64 %p" + [string](i);
        i = i + 1;
    }

    st.F.Add("define i64 @__fx_fn_" + fn + "(" + sig + ") {");
    Label(st, "entry");
    EmitParamPrologue(st, paramNames, paramKinds);

    If (!ParseBlock(c, st, s, ds)) Return false;
    If (!st.Term) Ret(st, "0");
    st.F.Add("}");
    Return true;
}
Safe Function bool ParseBlock(Cur c, St st, SourceFile s, IO.Type.list ds) {
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LBrace), "Expected Left Brace", "Block must start with '{'")) Return false;

    While (!End(c) && K(c) != [int](SelfHost.Frontend.TokenType.TokenType.RBrace)) {
        If (st.Term) {
            string d = Lab(st, "dead");
            Label(st, d);
        }
        If (!ParseStmt(c, st, s, ds)) Return false;
    }

    Return E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RBrace), "Expected Right Brace", "Block is not closed");
}

Safe Function bool ParseIfTail(Cur c, St st, SourceFile s, IO.Type.list ds) {
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "If condition must start with '('")) Return false;
    Expr cond = PExpr(c, st, s, ds);
    If (IsBad(cond)) Return false;
    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "If condition must end with ')'")) Return false;

    string b = AsI1(cond, st, s, c, ds);
    string lt = Lab(st, "if_then");
    string le = Lab(st, "if_else");
    string lx = Lab(st, "if_end");

    I(st, "br i1 " + b + ", label %" + lt + ", label %" + le);
    st.Term = true;

    Label(st, lt);
    If (!ParseStmt(c, st, s, ds)) Return false;
    bool tt = st.Term;
    If (!tt) Br(st, lx);

    Label(st, le);
    bool te = false;
    If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.KwElse))) {
        If (!ParseStmt(c, st, s, ds)) Return false;
        te = st.Term;
    } Else If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf))) {
        If (!ParseIfTail(c, st, s, ds)) Return false;
        te = st.Term;
    }

    If (!te) Br(st, lx);
    Label(st, lx);
    Return true;
}

Safe Function bool ParseStmt(Cur c, St st, SourceFile s, IO.Type.list ds) {
    int k = K(c);

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) Return ParseBlock(c, st, s, ds);

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwIf)) {
        Adv(c);
        Return ParseIfTail(c, st, s, ds);
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf)) {
        Adv(c);
        Return ParseIfTail(c, st, s, ds);
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwElse)) {
        Adv(c);
        Return ParseStmt(c, st, s, ds);
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwWhile)) {
        Adv(c);
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "While condition must start with '('")) Return false;

        string condLabel = Lab(st, "while.cond");
        string bodyLabel = Lab(st, "while.body");
        string endLabel = Lab(st, "while.end");

        Br(st, condLabel);
        Label(st, condLabel);
        Expr cond = PExpr(c, st, s, ds);
        If (IsBad(cond)) Return false;
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "While condition must end with ')'")) Return false;
        I(st, "br i1 " + AsI1(cond, st, s, c, ds) + ", label %" + bodyLabel + ", label %" + endLabel);
        st.Term = true;

        Label(st, bodyLabel);
        If (!ParseStmt(c, st, s, ds)) Return false;
        If (!st.Term) Br(st, condLabel);

        Label(st, endLabel);
        Return true;
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwFor)) {
        Adv(c);
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "For statement must start with '('")) Return false;
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Only empty for-init is supported in self LLVM backend")) Return false;

        string condLabel = Lab(st, "for.cond");
        string bodyLabel = Lab(st, "for.body");
        string endLabel = Lab(st, "for.end");

        Br(st, condLabel);
        Label(st, condLabel);

        string condValue = "1";
        If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
            Expr cond = PExpr(c, st, s, ds);
            If (IsBad(cond)) Return false;
            condValue = AsI1(cond, st, s, c, ds);
        }
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "For condition must end with ';'")) Return false;
        If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
            D(ds, s, c, "Unsupported Statement", "Only empty for-update is supported in self LLVM backend");
            Return false;
        }
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.RParen), "Expected Right Parenthesis", "For statement must end with ')'")) Return false;

        I(st, "br i1 " + condValue + ", label %" + bodyLabel + ", label %" + endLabel);
        st.Term = true;

        Label(st, bodyLabel);
        If (!ParseStmt(c, st, s, ds)) Return false;
        If (!st.Term) Br(st, condLabel);

        Label(st, endLabel);
        Return true;
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwReturn)) {
        Adv(c);
        If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon))) {
            Ret(st, "0");
            Return true;
        }

        Expr e = PExpr(c, st, s, ds);
        If (IsBad(e)) Return false;
        If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after Return")) Return false;
        Ret(st, AsI64(e, st, s, c, ds));
        Return true;
    }

    If (k == [int](SelfHost.Frontend.TokenType.TokenType.KwVar)) Return ParseVar(c, st, s, ds);
    If (k == [int](SelfHost.Frontend.TokenType.TokenType.Identifier) && !IO.Text.IsEmpty(NormalizeValueType(Tx(c)))) {
        Return ParseVar(c, st, s, ds);
    }

    If (ParseAssign(c, st, s, ds)) Return true;
    If (k == [int](SelfHost.Frontend.TokenType.TokenType.Identifier) && ParsePrintCall(c, st, s, ds)) Return true;

    D(ds, s, c, "Unsupported Statement", "Statement is not implemented in self LLVM backend");
    Return false;
}

Safe Function void SkipBal(Cur c, int o, int x) {
    If (K(c) != o) Return;
    int d = 1;
    Adv(c);
    While (!End(c) && d > 0) {
        If (K(c) == o) d = d + 1;
        If (K(c) == x) d = d - 1;
        Adv(c);
    }
}

Safe Function bool ParseMain(Cur c, St st, SourceFile s, IO.Type.list ds) {
    ResetFunctionState(st);
    st.F.Add("define i64 @__fx_user_main() {");
    Label(st, "entry");

    If (!ParseBlock(c, st, s, ds)) Return false;
    If (!st.Term) Ret(st, "0");

    st.F.Add("}");
    Return true;
}

Safe Function bool ParseProgram(Cur c, St st, SourceFile s, IO.Type.list ds) {
    bool found = false;

    While (!End(c)) {
        bool isExtern = false;
        While (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwStatic) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwSafe) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwTrusted) ||
               K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwUnsafe) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwPublic) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwPrivate) ||
               K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwProtected) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwInternal) || K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern)) {
            If (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern)) {
                isExtern = true;
            }
            Adv(c);
        }

        If (K(c) != [int](SelfHost.Frontend.TokenType.TokenType.KwFunction)) {
            Adv(c);
            Continue;
        }
        Adv(c);
        int typeStart = c.P;

        int np = -1;
        int i = c.P;
        While (i + 1 < Count(c)) {
            If (KAt(c, i) == [int](SelfHost.Frontend.TokenType.TokenType.Identifier) && KAt(c, i + 1) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
                np = i;
                Break;
            }
            int k = KAt(c, i);
            If (k == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon) || k == [int](SelfHost.Frontend.TokenType.TokenType.LBrace) || k == [int](SelfHost.Frontend.TokenType.TokenType.Eof)) Break;
            i = i + 1;
        }

        If (np < 0) {
            D(ds, s, c, "Expected Function Name", "Cannot locate function name");
            Return false;
        }

        string fn = TAt(c, np);
        string retKind = NormalizeValueType(BuildTypeText(c, typeStart, np));
        If (fn != "Main" && IO.Text.IsEmpty(retKind)) {
            D(ds, s, c, "Unsupported Type", "Function return type is not implemented in self LLVM backend");
            Return false;
        }
        c.P = np + 1;
        IO.Type.list paramNames = IO.Type.list.Create();
        IO.Type.list paramKinds = IO.Type.list.Create();
        If (!ParseParamList(c, s, ds, paramNames, paramKinds)) Return false;

        If (!isExtern) {
            RegisterFunction(st, fn, retKind, paramNames.Count());
        }

        If (isExtern) {
            If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
                If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Extern target after By must start with identifier")) Return false;
                While (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Dot))) {
                    If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Missing identifier after '.' in extern target")) Return false;
                }
            }
            If (!E(c, ds, s, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Extern function declaration must end with ';'")) Return false;
            Continue;
        }

        If (M(c, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon))) Continue;

        If (fn == "Main" && !found) {
            found = true;
            If (!ParseMain(c, st, s, ds)) Return false;
            Continue;
        }

        If (K(c) == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
            If (!ParseUserFunction(c, st, s, ds, fn, retKind, paramNames, paramKinds)) Return false;
            Continue;
        }

        D(ds, s, c, "Unsupported Function Form", "Function body is invalid");
        Return false;
    }

    If (!found) {
        ds.Add("[Codegen] Main function not found for self LLVM backend");
        Return false;
    }
    Return true;
}

Safe Function void EmitHelpers(IO.Type.list o) {
    o.Add("define void @__fx_print_str(ptr %s, i1 %line) {");
    o.Add("entry:");
    o.Add("  call void @fx_write_str(ptr %s)");
    o.Add("  br i1 %line, label %line_true, label %line_end");
    o.Add("line_true:");
    o.Add("  %nl = getelementptr inbounds [3 x i8], ptr @.fx.nl, i64 0, i64 0");
    o.Add("  call void @fx_write_str(ptr %nl)");
    o.Add("  br label %line_end");
    o.Add("line_end:");
    o.Add("  ret void");
    o.Add("}");
    o.Add("");

    o.Add("define void @__fx_print_i64(i64 %v, i1 %line) {");
    o.Add("entry:");
    o.Add("  %isneg = icmp slt i64 %v, 0");
    o.Add("  br i1 %isneg, label %neg, label %nonneg");
    o.Add("neg:");
    o.Add("  %minus = getelementptr inbounds [2 x i8], ptr @.fx.minus, i64 0, i64 0");
    o.Add("  call void @fx_write_str(ptr %minus)");
    o.Add("  %negv = sub i64 0, %v");
    o.Add("  br label %cont");
    o.Add("nonneg:");
    o.Add("  br label %cont");
    o.Add("cont:");
    o.Add("  %abs = phi i64 [ %negv, %neg ], [ %v, %nonneg ]");
    o.Add("  %u32 = trunc i64 %abs to i32");
    o.Add("  call void @fx_write_u32(i32 %u32)");
    o.Add("  br i1 %line, label %line_true2, label %line_end2");
    o.Add("line_true2:");
    o.Add("  %nl2 = getelementptr inbounds [3 x i8], ptr @.fx.nl, i64 0, i64 0");
    o.Add("  call void @fx_write_str(ptr %nl2)");
    o.Add("  br label %line_end2");
    o.Add("line_end2:");
    o.Add("  ret void");
    o.Add("}");
}

Safe Function bool Run(SourceFile source, IO.Type.list tokens, string llPath, IO.Type.list diagnostics) {
    St st = New St();
    Cur c = New Cur(tokens);

    If (!ParseProgram(c, st, source, diagnostics)) {
        Return false;
    }

    IO.Type.list out = IO.Type.list.Create();
    out.Add("; generated by selfhost llvm backend");
    out.Add("target triple = " + [string]([char](34)) + "x86_64-pc-windows-msvc" + [string]([char](34)));
    out.Add("");
    out.Add("declare void @fx_write_str(ptr)");
    out.Add("declare void @fx_write_u32(i32)");
    out.Add("declare i32 @fx_strcmp(ptr, ptr)");
    out.Add("declare void @ExitProcess(i32)");
    out.Add("");

    int i = 0;
    While (i < st.G.Count()) {
        out.Add([string](st.G.Fetch(i)));
        i = i + 1;
    }

    out.Add("");
    EmitHelpers(out);
    out.Add("");

    int j = 0;
    While (j < st.F.Count()) {
        out.Add([string](st.F.Fetch(j)));
        j = j + 1;
    }

    out.Add("");
    out.Add("define void @mainCRTStartup() {");
    out.Add("entry:");
    out.Add("  %rc = call i64 @__fx_user_main()");
    out.Add("  %rc32 = trunc i64 %rc to i32");
    out.Add("  call void @ExitProcess(i32 %rc32)");
    out.Add("  unreachable");
    out.Add("}");

    If (!IO.File.WriteLines(llPath, out)) {
        diagnostics.Add("[Codegen] failed to write generated LLVM IR file: " + llPath);
        Return false;
    }
    Return true;
}
