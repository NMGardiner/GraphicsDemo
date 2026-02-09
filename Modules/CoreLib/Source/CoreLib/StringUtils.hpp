#ifndef CORELIB_STRINGUTILS_HPP
#define CORELIB_STRINGUTILS_HPP

// System Headers
#include <string>
#include <string_view>



namespace Core
{

// Convert a narrow string to a wide string.
std::wstring NarrowStringToWide(std::string_view narrow);
std::wstring NarrowStringToWide(const std::string& narrow);

// Convert a wide string to a narrow string.
std::string WideStringToNarrow(std::wstring_view wide);
std::string WideStringToNarrow(const std::wstring& wide);

} // namespace Core

#endif // CORELIB_STRINGUTILS_HPP