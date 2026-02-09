#include "StringUtils.hpp"

// Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// System Headers
#include <cstdint>
#include <string>



namespace Core
{

std::wstring NarrowStringToWide(std::string_view narrow)
{
	if (narrow.empty())
	{
		return std::wstring();
	}

	int length = MultiByteToWideChar(
		CP_UTF8,
		0,
		narrow.data(),
		static_cast<int>(narrow.length()),
		nullptr,
		0);

	std::wstring wide = std::wstring(length, 0);

	length = MultiByteToWideChar(
		CP_UTF8,
		0,
		narrow.data(),
		static_cast<int>(narrow.length()),
		wide.data(),
		length);

	return wide;
}



std::wstring NarrowStringToWide(const std::string& narrow)
{
	return NarrowStringToWide(std::string_view(narrow));
}



std::string WideStringToNarrow(std::wstring_view wide)
{
	if (wide.empty())
	{
		return std::string();
	}

	int length = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.data(),
		static_cast<int>(wide.length()),
		nullptr,
		0,
		nullptr,
		nullptr);

	std::string narrow = std::string(length, 0);

	length = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.data(),
		static_cast<int>(wide.length()),
		narrow.data(),
		length,
		nullptr,
		nullptr);

	return narrow;
}



std::string WideStringToNarrow(const std::wstring& wide)
{
	return WideStringToNarrow(std::wstring_view(wide));
}

} // namespace Core