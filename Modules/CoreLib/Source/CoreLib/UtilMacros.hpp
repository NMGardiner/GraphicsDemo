#ifndef CORELIB_UTILMACROS_HPP
#define CORELIB_UTILMACROS_HPP

// Assertions

#ifdef NDEBUG
	// The sizeof trick is taken from here:
	// https://web.archive.org/web/20201129200055/http://cnicholson.net/2009/02/stupid-c-tricks-adventures-in-assert/
	// TLDR: Avoids evaluation entirely, unlike (void)condition.
	#define CORE_ASSERT(condition) do { (void)sizeof(condition); } while(0)
#else
	#include "Logging.hpp"

	#if _MSC_VER
		#define CORE_HALT() __debugbreak()
	#else
		#include <cassert>

		#define CORE_HALT() assert(false)
	#endif // _MSV_VER

	#define CORE_ASSERT(condition)									\
		do															\
		{															\
			if (!(condition))										\
			{														\
				Core::LogError("Assertion failed: " #condition);	\
				CORE_HALT();										\
			}														\
		}															\
		while(0)
#endif // NDEBUG

#define CORE_ASSERT_NULL(ptr) CORE_ASSERT(ptr == nullptr)
#define CORE_ASSERT_NOT_NULL(ptr) CORE_ASSERT(ptr != nullptr)



// Class Constructor Helpers

#define CORE_COPY_CONSTRUCTOR_DEFAULT(className) className(const className&) = default
#define CORE_MOVE_CONSTRUCTOR_DEFAULT(className) className(className&&) = default
#define CORE_COPY_ASSIGN_CONSTRUCTOR_DEFAULT(className) className& operator=(const className&) = default
#define CORE_MOVE_ASSIGN_CONSTRUCTOR_DEFAULT(className) className& operator=(className&&) = default

#define CORE_CLASS_CONSTRUCTORS_DEFAULT(className)		\
	CORE_COPY_CONSTRUCTOR_DEFAULT(className);			\
	CORE_MOVE_CONSTRUCTOR_DEFAULT(className);			\
	CORE_COPY_ASSIGN_CONSTRUCTOR_DEFAULT(className);	\
	CORE_MOVE_ASSIGN_CONSTRUCTOR_DEFAULT(className)

#define CORE_COPY_CONSTRUCTOR_DELETE(className) className(const className&) = delete
#define CORE_MOVE_CONSTRUCTOR_DELETE(className) className(className&&) = delete
#define CORE_COPY_ASSIGN_CONSTRUCTOR_DELETE(className) className& operator=(const className&) = delete
#define CORE_MOVE_ASSIGN_CONSTRUCTOR_DELETE(className) className& operator=(className&&) = delete

#define CORE_CLASS_CONSTRUCTORS_DELETE(className)	\
	CORE_COPY_CONSTRUCTOR_DELETE(className);		\
	CORE_MOVE_CONSTRUCTOR_DELETE(className);		\
	CORE_COPY_ASSIGN_CONSTRUCTOR_DELETE(className);	\
	CORE_MOVE_ASSIGN_CONSTRUCTOR_DELETE(className)

#endif // CORELIB_UTILSMACROS_HPP