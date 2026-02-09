#ifndef CORELIB_LOGGING_HPP
#define CORELIB_LOGGING_HPP

// Project
#include "StringUtils.hpp"

// System Headers
#include <format>
#include <source_location>
#include <string_view>



namespace Core
{

void LogOutput(
	const char* level,
	const std::source_location& loc,
	std::string_view formattedString);



template <typename... Args>
void LogPrint(
	const char* level,
	const std::source_location& loc,
	std::format_string<Args...> fmt,
	Args&&... args)
{
	LogOutput(
		level,
		loc,
		std::format(fmt, std::forward<Args>(args)...));
}



template <typename... Args>
void LogPrint(
	const char* level,
	const std::source_location& loc,
	std::wformat_string<Args...> fmt,
	Args&&... args)
{
	LogOutput(
		level,
		loc,
		Core::WideStringToNarrow(std::format(fmt, std::forward<Args>(args)...)));
}



// The following log functions rely on a deduction guide trick to allow
// variadic arguments while also having a default parameter for loc.
// See: https://stackoverflow.com/a/57548488
//
// A second explicit variant is provided for cases where the caller
// needs to forward a source_location evaluated further up the stack.



// Info

template <typename... Args>
struct LogInfo
{
	LogInfo(
		std::format_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogInfo(loc, fmt, std::forward<Args>(args)...);
	}

	LogInfo(
		const std::source_location& loc,
		std::format_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("INFO", loc, fmt, std::forward<Args>(args)...);
	}

	LogInfo(
		std::wformat_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogInfo(loc, fmt, std::forward<Args>(args)...);
	}

	LogInfo(
		const std::source_location& loc,
		std::wformat_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("INFO", loc, fmt, std::forward<Args>(args)...);
	}
};

template <typename... Args>
LogInfo(std::format_string<Args...>, Args&&...) -> LogInfo<Args...>;

template <typename... Args>
LogInfo(std::wformat_string<Args...>, Args&&...) -> LogInfo<Args...>;



// Warning

template <typename... Args>
struct LogWarning
{
	LogWarning(
		std::format_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogWarning(loc, fmt, std::forward<Args>(args)...);
	}

	LogWarning(
		const std::source_location& loc,
		std::format_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("WARN", loc, fmt, std::forward<Args>(args)...);
	}

	LogWarning(
		std::wformat_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogWarning(loc, fmt, std::forward<Args>(args)...);
	}

	LogWarning(
		const std::source_location& loc,
		std::wformat_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("WARN", loc, fmt, std::forward<Args>(args)...);
	}
};

template <typename... Args>
LogWarning(std::format_string<Args...>, Args&&...) -> LogWarning<Args...>;

template <typename... Args>
LogWarning(std::wformat_string<Args...>, Args&&...) -> LogWarning<Args...>;



// Error

template <typename... Args>
struct LogError
{
	LogError(
		std::format_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogError(loc, fmt, std::forward<Args>(args)...);
	}

	LogError(
		const std::source_location& loc,
		std::format_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("ERR ", loc, fmt, std::forward<Args>(args)...);
	}

	LogError(
		std::wformat_string<Args...> fmt,
		Args&&... args,
		const std::source_location& loc = std::source_location::current())
	{
		LogError(loc, fmt, std::forward<Args>(args)...);
	}

	LogError(
		const std::source_location& loc,
		std::wformat_string<Args...> fmt,
		Args&&... args)
	{
		LogPrint("ERR ", loc, fmt, std::forward<Args>(args)...);
	}
};

template <typename... Args>
LogError(std::format_string<Args...>, Args&&...) -> LogError<Args...>;

template <typename... Args>
LogError(std::wformat_string<Args...>, Args&&...) -> LogError<Args...>;

} // namespace Core

#endif // CORELIB_LOGGING_HPP