#include "Logging.hpp"

// System Headers
#include <chrono>
#include <format>
#include <iostream>
#include <iterator>
#include <source_location>
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



void LogOutput(
	const char* level,
	const std::source_location& loc,
	std::string_view formattedString)
{
	const std::string_view trimmedName = TrimFileName(loc.file_name());

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

} // namespace Core