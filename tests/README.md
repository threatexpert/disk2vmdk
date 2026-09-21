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

# Unicode image paths and VMDK descriptors

Build the actual writer together with the path test using the same `v140_xp`
toolset as the application. In a Visual Studio developer command prompt:

```bat
msbuild tests\VDiskWriterPathTests.vcxproj /p:Configuration=Release /p:Platform=Win32
msbuild tests\VDiskWriterPathTests.vcxproj /p:Configuration=Release /p:Platform=x64
```

Executables are placed in `build\tests\Win32` and `build\tests\x64`.
Run each executable with the absolute path to a **fresh, empty directory** as its
only argument. It creates 12 small VMDK/VHD/VDI fixtures and `expected.raw` there.
It checks ASCII paths, Chinese directories, Chinese filenames with spaces, and
characters outside the ANSI code page, including a UTF-16 surrogate pair.
VMDK checks include the exact relative UTF-8 filename and the top-level
`encoding="UTF-8"` declaration. No physical disk is read or written.

For each fixture, use an independent reader to compare its virtual contents:

```text
qemu-img compare -f vmdk -F raw <fixture.vmdk> <expected.raw>
qemu-img compare -f vpc  -F raw <fixture.vhd>  <expected.raw>
qemu-img compare -f vdi  -F raw <fixture.vdi>  <expected.raw>
```

Some Windows QEMU builds cannot accept all Unicode command-line paths. For
content-only verification with such builds, copy fixtures to unique ASCII paths;
do not mistake that result for verification of the reader's Unicode path support.
For VMDK path compatibility, VMware's `vmware-vdiskmanager -e <fixture.vmdk>`
checks the original path. Reading/converting it with `-r <fixture.vmdk> -t 0
<fresh-output.vmdk>` and comparing the converted contents also exercises the
original Unicode path and its descriptor. Never run repair operations on a
user's image as part of these tests.

Fixtures are retained for further VMware/VirtualBox compatibility checks.
