// .NET smoke test for Wine Mono under proton-wine: generics, LINQ,
// exceptions, threads, P/Invoke into kernel32.  Prints TAP; exit code is
// the number of failures.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

static class Hello
{
    [DllImport("kernel32.dll")]
    static extern uint GetCurrentProcessId();

    static int failures, n;

    static void Ok(bool cond, string what)
    {
        n++;
        if (!cond) failures++;
        Console.WriteLine((cond ? "ok " : "not ok ") + n + " - " + what);
    }

    static int Main()
    {
        var squares = Enumerable.Range(1, 10).Select(i => i * i).ToList();
        Ok(squares.Sum() == 385, "LINQ sum of squares");

        var dict = new Dictionary<string, int> { { "a", 1 }, { "b", 2 } };
        Ok(dict["b"] == 2, "generic dictionary");

        bool caught = false;
        try { throw new InvalidOperationException("x"); }
        catch (InvalidOperationException) { caught = true; }
        Ok(caught, "exception caught");

        int counter = 0;
        var t = new Thread(() => Interlocked.Add(ref counter, 42));
        t.Start(); t.Join();
        Ok(counter == 42, "thread");

        Ok(GetCurrentProcessId() == (uint)System.Diagnostics.Process.GetCurrentProcess().Id,
           "P/Invoke GetCurrentProcessId");

        Ok(Environment.Is64BitProcess, "64-bit process");
        return failures;
    }
}
