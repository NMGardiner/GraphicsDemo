#ifndef CORELIB_LOGGING_HPP
#define CORELIB_LOGGING_HPP

// System Headers
#include <chrono>
#include <format>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>



namespace Core
{

// This can't be made constexpr in a way that actually guarantees
// evaluation at compile time due to the nature of std::source_location
// being passed as a function argument. 
static std::string_view TrimFileName(const std::string_view name)
{
	return name.substr(name.find_last_of("/\\") + 1);
}



template <typename... Args>
void LogPrint(
	const char* level,
	const std::source_location& loc,
	std::format_string<Args...> fmt,
	Args&&... args)
{
	const std::string_view trimmedName = TrimFileName(loc.file_name());
	const std::string formattedString = std::format(fmt, std::forward<Args>(args)...);
	std::ostream_iterator<char> output(std::cout);

	std::format_to(
		output,
		"{:%T} [{}] {}:{} - {}\n",
		std::chrono::system_clock::now(),
		level,
		trimmedName,
		loc.line(),
		formattedString);
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
};

template <typename... Args>
LogInfo(std::format_string<Args...>, Args&&...) -> LogInfo<Args...>;

template <typename... Args>
LogInfo(const std::source_location&, std::format_string<Args...>, Args&&...) -> LogInfo<Args...>;



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
};

template <typename... Args>
LogWarning(std::format_string<Args...>, Args&&...) -> LogWarning<Args...>;

template <typename... Args>
LogWarning(const std::source_location&, std::format_string<Args...>, Args&&...) -> LogWarning<Args...>;



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
};

template <typename... Args>
LogError(std::format_string<Args...>, Args&&...) -> LogError<Args...>;

template <typename... Args>
LogError(const std::source_location&, std::format_string<Args...>, Args&&...) -> LogError<Args...>;

} // namespace Core

#endif // CORELIB_LOGGING_HPP