#include <windows.h>
#include <strsafe.h>
#include <filesystem>

#include <string>
#include <functional>
#include <iostream>
#include <memory>

#include "platform/ScopedResource.hpp"
#include "platform/barectf-platform-win32-fs.h"
#include "barectf.h"

using std::string_literals::operator""s;

void PrintErrorAndDie(LPCTSTR opDescription)
{
	const auto errorCode = GetLastError();

	ScopedResource<LPVOID> errorMsg{nullptr, LocalFree};
	auto fmtRet = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &errorMsg.value(),
		0,
		nullptr);
	if (fmtRet == 0) {
		/* Okay, just die already... */
		std::wcerr << "Failed to " << opDescription << ": Unknown Error (formatting failed with error code " << GetLastError() << ")" << std::endl;
		ExitProcess(GetLastError());
	}

	std::wstringstream formattedMsg;
	formattedMsg << "Failed to " << opDescription << ": " << errorMsg.value();
	std::wcerr << formattedMsg.str() << std::endl;
	ExitProcess(errorCode);
}

void PrintUsageAndDie()
{
	std::wcerr << L"Usage: win32-fs-simple.exe TRACE_FOLDER" << std::endl;
	ExitProcess(ERROR_INVALID_PARAMETER);
}

struct BarectfWin32FsDeleter
{
	typedef struct barectf_platform_win32_fs* pointer;

	void operator()(pointer platform)
	{
		barectf_platform_win32_fs_destroy(platform);
	}
};

using BarectfWin32Fs = std::unique_ptr<struct barectf_platform_win32_fs, BarectfWin32FsDeleter>;

enum state_t {
	NEW,
	TERMINATED,
	READY,
	RUNNING,
	WAITING,
};

void TraceStuff(struct barectf_default_ctx * const ctx)
{
	const char *str = "hello there";

	for (int i = 0; i < 50000; ++i) {
		barectf_trace_simple_uint32(ctx, i * 1500);
		barectf_trace_simple_int16(ctx, -i * 2);
		barectf_trace_simple_float(ctx, (float) i / 1.23);
		barectf_trace_simple_string(ctx, str);
		barectf_trace_context_no_payload(ctx, i, "ctx");
		barectf_trace_simple_enum(ctx, RUNNING);
		barectf_trace_a_few_fields(ctx, -1, 301, -3.14159, str, NEW);
		barectf_trace_bit_packed_integers(ctx, 1, -1, 3, -2, 2, 7, 23, -55, 232);
		barectf_trace_no_context_no_payload(ctx);
		barectf_trace_simple_enum(ctx, TERMINATED);
	}
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR _unusedCommandLine, int nShowCmd)
{
	const auto commandLine = GetCommandLineW();

	int argNum;
	ScopedResource<LPWSTR *> args {
		CommandLineToArgvW(commandLine, &argNum),
		LocalFree
	};

	if (!args.value()) {
		PrintErrorAndDie(TEXT("CommandLineToArgvW"));
	} else if (argNum != 2) {
		PrintUsageAndDie();
	}

	BarectfWin32Fs tracer(barectf_platform_win32_fs_create(args.value()[1]));
	if (!tracer.get()) {
		return ERROR_GEN_FAILURE;
	}

	TraceStuff(barectf_platform_win32_fs_get_barectf_ctx(tracer.get()));

	return NO_ERROR;
}
