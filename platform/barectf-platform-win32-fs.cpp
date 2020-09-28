#include <windows.h>
#include <iostream>
#include <strsafe.h>
#include <filesystem>
#include <memory>
#include <system_error>
#include <limits>
#include <cassert>

#include "platform/ScopedResource.hpp"
#include "barectf-platform-win32-fs.h"
#include "../barectf.h"

using std::string_literals::operator""s;

namespace WinRAII {

namespace impl {
struct HANDLEDeleter
{
	typedef HANDLE pointer;

	void operator()(HANDLE handle)
	{
		if (handle != INVALID_HANDLE_VALUE) {
			CloseHandle(handle);
		}
	}
};

struct ViewOfFileDeleter
{
	typedef LPVOID pointer;

	void operator()(LPVOID viewOfFile)
	{
		if (viewOfFile) {
			UnmapViewOfFile(viewOfFile);
		}
	}
};
}

using UniqueHandle = std::unique_ptr<HANDLE, impl::HANDLEDeleter>;
using UniqueViewOfFile = std::unique_ptr<HANDLE, impl::ViewOfFileDeleter>;

}

class BarectfWin32FSPlatform {
public:
	BarectfWin32FSPlatform();
	~BarectfWin32FSPlatform();

	// Returns false on error. Can only be used once.
	bool openStreamFile(LPCWSTR traceFolderPath);
	uint64_t getClockValue();
	bool isBackendFull();
	void openPacket();
	void closePacket();
	struct barectf_default_ctx &getBarectfContext();

private:
	bool mapNextPacket();

private:
	WinRAII::UniqueHandle _streamFile;
	WinRAII::UniqueHandle _fileMapping;
	WinRAII::UniqueViewOfFile _fileView;
	uint64_t _clockFrequency;
	uint64_t _fileSize;
	struct barectf_default_ctx _barectfCtx;
	uint64_t _packetSize;
};

namespace {

uint64_t perf_counter_clock_get_value(void *user_data);
int is_backend_full(void *user_data);
void open_packet(void *user_data);
void close_packet(void *user_data);

const struct barectf_platform_callbacks barectf_win32_fs_callbacks = {
	perf_counter_clock_get_value,
	is_backend_full,
	open_packet,
	close_packet,
};

void printError(LPCTSTR opDescription)
{
	const auto errorCode = GetLastError();

	ScopedResource<LPTSTR> errorMsg{nullptr, LocalFree};

	auto fmtRet = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR) &errorMsg.value(),
		0,
		nullptr);
	if (fmtRet == 0) {
		std::wcerr << "Failed to " << opDescription << ": Unknown Error (formatting failed with error code " << GetLastError() << ")" << std::endl;
		return;
	}

	std::wstringstream formattedMsg;
	formattedMsg << "Failed to " << opDescription << ": " << errorMsg.value();
	std::wcerr << formattedMsg.str();
}

uint64_t perf_counter_clock_get_value(void *user_data)
{
	auto platform = static_cast<BarectfWin32FSPlatform *>(user_data);
	return platform->getClockValue();
}

int is_backend_full(void *user_data)
{
	const auto platform = static_cast<BarectfWin32FSPlatform *>(user_data);
	return platform->isBackendFull();
}

void open_packet(void *user_data)
{
	auto platform = static_cast<BarectfWin32FSPlatform *>(user_data);
	platform->openPacket();
}

void close_packet(void *user_data)
{
	auto platform = static_cast<BarectfWin32FSPlatform *>(user_data);
	platform->closePacket();
}

}

BarectfWin32FSPlatform::BarectfWin32FSPlatform() :
	_fileSize {0},
	_barectfCtx {}
{
	LARGE_INTEGER perfFreq;
	const auto freqRet = QueryPerformanceFrequency(&perfFreq);
	if (!freqRet) {
		printError(TEXT("query performance counter frequency"));
		throw std::system_error();
	}

	_clockFrequency = perfFreq.QuadPart;

	SYSTEM_INFO sysInfo;
	GetSystemInfo(static_cast<LPSYSTEM_INFO>(&sysInfo));
	_packetSize = sysInfo.dwAllocationGranularity;

	barectf_init(&_barectfCtx, nullptr, 0, barectf_win32_fs_callbacks, this);
}

BarectfWin32FSPlatform::~BarectfWin32FSPlatform()
{
	if (barectf_packet_is_open(&getBarectfContext())) {
		closePacket();
	}
}

bool BarectfWin32FSPlatform::mapNextPacket()
{
	const auto packetOffset = _fileSize;

	_fileSize += _packetSize;
	LARGE_INTEGER fileSize;
	fileSize.QuadPart = _fileSize;
	_fileMapping = WinRAII::UniqueHandle {
		CreateFileMappingW(
			_streamFile.get(),
			nullptr,
			PAGE_READWRITE,
			fileSize.HighPart,
			fileSize.LowPart,
			nullptr)
	};

	if (!_fileMapping) {
		printError(TEXT("create file mapping"));
		return false;
	}

	LARGE_INTEGER fileOffset;
	fileOffset.QuadPart = packetOffset;
	_fileView = WinRAII::UniqueViewOfFile {
		MapViewOfFile(
			_fileMapping.get(),
			FILE_MAP_READ | FILE_MAP_WRITE,
			fileOffset.HighPart,
			fileOffset.LowPart,
			0)
	};

	if (!_fileView) {
		printError(TEXT("map view of file"));
		return false;
	}

	return !!_fileMapping;
}

bool BarectfWin32FSPlatform::openStreamFile(LPCWSTR traceFolderPath)
{
	auto fileAttributes = GetFileAttributesW(traceFolderPath);
	if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
		printError(TEXT("create stream file"));
		return false;
	} else if (!(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		std::wcerr << "Failed to create stream file: '" << traceFolderPath << "' is not a directory" << std::endl;
		return false;
	}

	auto streamFilePath = std::filesystem::path(traceFolderPath) / "stream";
	_streamFile = WinRAII::UniqueHandle {
		CreateFileW(
			streamFilePath.native().c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr)
	};

	if (!_streamFile) {
		printError(TEXT("create stream file"));
		return false;
	}

	if (!mapNextPacket()) {
		return false;
	}

	openPacket();
	return true;
}

uint64_t BarectfWin32FSPlatform::getClockValue()
{
	FILETIME sample;
	GetSystemTimePreciseAsFileTime(static_cast<LPFILETIME>(&sample));

	LARGE_INTEGER now;
	now.HighPart = sample.dwHighDateTime;
	now.LowPart = sample.dwLowDateTime;

	// offset between EPOCH and January 1, 1601 (UTC) (in us)
	const auto offsetWinUnix = 11644473600000ULL * 10000ULL;
	now.QuadPart -= offsetWinUnix;
	return now.QuadPart;
}

bool BarectfWin32FSPlatform::isBackendFull()
{
	// assumes this is only called before opening a packet
	return !mapNextPacket();
}

void BarectfWin32FSPlatform::openPacket()
{
	auto *ctx = &getBarectfContext();
	assert(_fileView);
	barectf_packet_set_buf(ctx, static_cast<uint8_t *>(_fileView.get()), _packetSize);
	barectf_default_open_packet(ctx);
}

void BarectfWin32FSPlatform::closePacket()
{
	auto *ctx = &getBarectfContext();
	barectf_default_close_packet(ctx);
	// causes a crash with Barectf 3.0.0, fix incoming.
	barectf_packet_set_buf(ctx, nullptr, 0);
	_fileView.reset();
	_fileMapping.reset();
}

struct barectf_default_ctx & BarectfWin32FSPlatform::getBarectfContext()
{
	return _barectfCtx;
}

// Exposed Win32 FS C API follows.

struct barectf_platform_win32_fs *barectf_platform_win32_fs_create(LPCWSTR trace_folder_path)
{
	std::unique_ptr<BarectfWin32FSPlatform> platform;

	try {
		platform = std::make_unique<BarectfWin32FSPlatform>();
	} catch (...) {
		return nullptr;
	}

	if (!platform->openStreamFile(trace_folder_path)) {
		return nullptr;
	}

	if (!trace_folder_path) {
		return nullptr;
	}

	return (struct barectf_platform_win32_fs *) platform.release();
}

struct barectf_default_ctx *barectf_platform_win32_fs_get_barectf_ctx(struct barectf_platform_win32_fs *platform)
{
	auto cppPlatform = (BarectfWin32FSPlatform *) platform;
	return &cppPlatform->getBarectfContext();
}

void barectf_platform_win32_fs_destroy(struct barectf_platform_win32_fs *platform)
{
	auto cppPlatform = (BarectfWin32FSPlatform *) platform;
	delete cppPlatform;
}