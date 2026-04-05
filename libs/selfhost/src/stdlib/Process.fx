Unit SelfHost.Std.Process;

Get IO.System;

Safe Function string Quote(string text) {
    Return [string]([char](34)) + text + [string]([char](34));
}

Safe Function int Exec(string commandLine) {
    Return IO.System.Exec(commandLine);
}
