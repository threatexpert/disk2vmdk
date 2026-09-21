# Progress estimate regression tests

From the repository root in a Visual Studio C++ developer command prompt:

```bat
cl /nologo /EHsc /W4 tests\ProgressEstimateTests.cpp /Fo"%TEMP%\disk2vmdk-progress.obj" /Fe"%TEMP%\disk2vmdk-progress.exe"
if errorlevel 1 exit /b 1
"%TEMP%\disk2vmdk-progress.exe"
```

Run with both x86 and x64 compiler environments. Do not define `NDEBUG`:
these tests use assertions. They exercise the same header used by the GUI
and CLI, without opening disks or creating snapshots.

Coverage: sparse vs. DD output, free-space tails, empty source, finalization,
missing/stalled samples, slow transfers, rounding, counter overshoot,
invalid speed and estimate overflow. VSS snapshot/live-volume consistency
still requires an integration test; these tests do not certify image contents.
