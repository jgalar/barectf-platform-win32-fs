#ifndef _BARECTF_PLATFORM_WIN32_FS_SCOPED_RESOURCE_H
#define _BARECTF_PLATFORM_WIN32_FS_SCOPED_RESOURCE_H

#include <functional>

/*
 * Run a function when an instance goes out of scope.
 * Meant to provide minimal RAII when interfacing with C APIs.
 */
template <typename T>
class ScopedResource final
{
public:
	explicit ScopedResource(const T &value,
				std::function<void(const T &)> scopeExitFunction) :
		_value{value}, _scopeExitFunction{scopeExitFunction}
	{
	}

	~ScopedResource()
	{
		_scopeExitFunction(_value);
	}

public:
	T &value() noexcept
	{
		return _value;
	}

private:
	T _value;
	std::function<void(const T &)> _scopeExitFunction;
};

#endif // _BARECTF_PLATFORM_WIN32_FS_SCOPED_RESOURCE_H