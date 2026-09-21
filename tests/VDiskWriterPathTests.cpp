#include "../core/VDiskWriter.h"
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>

// Use a fresh, empty output directory. Fixtures are retained for reader tests.
static void Require(bool ok, const char* message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s (Win32 error %lu)\n", message, GetLastError());
        exit(1);
    }
}

static std::vector<unsigned char> ReadAt(HANDLE file, uint64_t offset, DWORD size)
{
    LARGE_INTEGER pos;
    pos.QuadPart = offset;
    Require(SetFilePointerEx(file, pos, NULL, FILE_BEGIN) != FALSE, "seek fixture");
    std::vector<unsigned char> bytes(size);
    DWORD read = 0;
    Require(ReadFile(file, &bytes[0], size, &read, NULL) && read == size, "read fixture");
    return bytes;
}

static std::string Utf8(const std::wstring& value)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, NULL, 0, NULL, NULL);
    Require(count > 0, "UTF-8 size");
    std::string result(count, '\0');
    Require(WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], count, NULL, NULL) != 0,
        "UTF-8 conversion");
    result.resize(count - 1);
    return result;
}

int wmain(int argc, wchar_t** argv)
{
    Require(argc == 2, "usage: VDiskWriterPathTests <fresh empty directory>");
    std::wstring root = argv[1];
    const uint64_t capacity = 4 * 1024 * 1024;
    std::vector<unsigned char> data(1024 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = (unsigned char)(i % 251 + 1);

    // Reference disk: 1 MiB of nonzero data followed by 3 MiB of zeros.
    HANDLE raw = CreateFileW((root + L"\\expected.raw").c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    Require(raw != INVALID_HANDLE_VALUE, "create reference disk");
    DWORD written = 0;
    Require(WriteFile(raw, &data[0], (DWORD)data.size(), &written, NULL) && written == data.size(),
        "write reference data");
    std::vector<unsigned char> zeros((size_t)capacity - data.size(), 0);
    Require(WriteFile(raw, &zeros[0], (DWORD)zeros.size(), &written, NULL) && written == zeros.size(),
        "write reference zeros");
    CloseHandle(raw);

    struct TestPath { const wchar_t* dir; const wchar_t* name; } paths[] = {
        { L"ascii", L"plain" },
        { L"\u65b0\u5efa\u6587\u4ef6\u5939", L"L" },
        { L"\u4e2d\u6587 \u8def\u5f84", L"\u955c\u50cf \u6587\u4ef6" },
        { L"unicode_\xD83D\xDE80", L"\u78c1\u76d8_\xD83D\xDE80" }
    };
    const char* formats[] = { "VMDK", "VHD", "VDI" };
    const wchar_t* extensions[] = { L".vmdk", L".vhd", L".vdi" };
    for (size_t c = 0; c < _countof(paths); ++c) {
        std::wstring dir = root + L"\\" + paths[c].dir;
        Require(CreateDirectoryW(dir.c_str(), NULL) != FALSE, "create test directory");
        for (size_t f = 0; f < _countof(formats); ++f) {
            std::wstring filename = std::wstring(paths[c].name) + extensions[f];
            std::wstring path = dir + L"\\" + filename;
            CVDiskWriter writer;
            Require(writer.CreateImage(formats[f], path.c_str(), capacity), "create image");
            Require(writer.Write(0, &data[0], data.size()), "write image data");
            Require(writer.WriteZero(data.size(), capacity - data.size()), "write image zeros");
            Require(writer.Close(), "close image");
            Require(GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES, "exact output path");

            HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            Require(file != INVALID_HANDLE_VALUE, "reopen Unicode image path");
            if (f == 0) {
                std::vector<unsigned char> header = ReadAt(file, 0, 512);
                Require(memcmp(&header[0], "KDMV", 4) == 0, "VMDK magic");
                uint64_t descOffset = 0, descSize = 0;
                memcpy(&descOffset, &header[28], 8);
                memcpy(&descSize, &header[36], 8);
                Require(descSize > 0 && descSize <= 20, "descriptor bounds");
                std::vector<unsigned char> bytes = ReadAt(file, descOffset * 512, (DWORD)descSize * 512);
                Require(bytes.back() == 0, "descriptor padding");
                std::string desc((const char*)&bytes[0]);
                Require(desc.find("\nencoding=\"UTF-8\"\n") != std::string::npos, "top-level UTF-8 declaration");
                Require(desc.find("ddb.encoding") == std::string::npos && desc.find("GBK") == std::string::npos,
                    "no conflicting encoding declaration");
                Require(desc.find("RW 8192 SPARSE \"" + Utf8(filename) + "\"\n") != std::string::npos,
                    "UTF-8 relative extent filename");
                Require(desc.find(Utf8(root)) == std::string::npos, "no absolute path in descriptor");
            }
            CloseHandle(file);
        }
    }
    puts("VDiskWriter paths: 12 images passed; fixtures retained for external reader verification.");
    return 0;
}
