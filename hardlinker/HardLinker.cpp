#include <Windows.h>
#include <strsafe.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include <vector>

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } DUMMYUNIONNAME;

    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _FILE_INFORMATION_CLASS {
    // ...
    FileLinkInformation = 11
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

EXTERN_C NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
    _In_ HANDLE FileHandle,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_reads_bytes_(Length) PVOID FileInformation,
    _In_ ULONG Length,
    _In_ FILE_INFORMATION_CLASS FileInformationClass
);

typedef struct _FILE_LINK_INFORMATION {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_LINK_INFORMATION, *PFILE_LINK_INFORMATION;

#pragma comment(lib, "ntdll")

int wmain(int argc, const wchar_t* argv[])
{
    if (argc != 3) {
        printf("Usage: hardlinker <src> <dest>\n");
        return EXIT_FAILURE;
    }

    const wchar_t* srcFile = argv[1];
    const wchar_t* destFile = argv[2];

    HANDLE hFile = CreateFile(srcFile, SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error 0x%08x opening source file.\n", GetLastError());
        return EXIT_FAILURE;
    }

    size_t destFileNameSize = (wcslen(destFile) + 1) * sizeof(WCHAR); 
    size_t fileLinkInfoBufferSize = FIELD_OFFSET(FILE_LINK_INFORMATION, FileName) + destFileNameSize;
    assert(fileLinkInfoBufferSize < ULONG_MAX);
    std::vector<uint8_t> fileLinkInfoBuffer(fileLinkInfoBufferSize);
    auto fileLinkInfo = reinterpret_cast<PFILE_LINK_INFORMATION>(fileLinkInfoBuffer.data());

    fileLinkInfo->ReplaceIfExists = FALSE;
    fileLinkInfo->RootDirectory = nullptr;
    assert(wcslen(destFile) < ULONG_MAX);
    fileLinkInfo->FileNameLength = static_cast<ULONG>(wcslen(destFile) * sizeof(WCHAR));
    HRESULT hr = StringCbCopy(fileLinkInfo->FileName, destFileNameSize, destFile);
    if (FAILED(hr)) {
        fprintf(stderr, "StringCbCopy failed with HRESULT 0x%08x.\n", hr);
        CloseHandle(hFile);
        return EXIT_FAILURE;
    }

    IO_STATUS_BLOCK iosb{};
    NTSTATUS status = NtSetInformationFile(
        hFile,
        &iosb,
        fileLinkInfo,
        static_cast<ULONG>(fileLinkInfoBufferSize),
        FileLinkInformation);
    if (status != 0) {
        fprintf(stderr, "NtSetInformationFile FileLinkInformation failed with NTSTATUS 0x%08x.\n", status);
        CloseHandle(hFile);
        return EXIT_FAILURE;
    }

    printf("OK.\n");

    CloseHandle(hFile);

    return 0;
}

