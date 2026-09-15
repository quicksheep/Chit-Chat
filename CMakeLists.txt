cmake_minimum_required(VERSION 3.20)
project(ChitChat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT AESDK_ROOT)
	if(DEFINED ENV{AESDK_ROOT})
		set(AESDK_ROOT "$ENV{AESDK_ROOT}")
	endif()
endif()

if(NOT AESDK_ROOT)
	message(FATAL_ERROR
		"Set AESDK_ROOT to the Adobe After Effects SDK folder (the directory that contains Headers/ or Examples/Headers/).\n"
		"Download the SDK from https://developer.adobe.com/console/ (Downloads tab).")
endif()

set(_AE_HEADER_CANDIDATES
	"${AESDK_ROOT}/Headers"
	"${AESDK_ROOT}/Examples/Headers"
	"${AESDK_ROOT}/AfterEffectsSDK/Headers")
set(_AE_UTIL_CANDIDATES
	"${AESDK_ROOT}/Util"
	"${AESDK_ROOT}/Examples/Util"
	"${AESDK_ROOT}/AfterEffectsSDK/Util")
set(_AE_RES_CANDIDATES
	"${AESDK_ROOT}/Resources"
	"${AESDK_ROOT}/Examples/Resources"
	"${AESDK_ROOT}/AfterEffectsSDK/Resources")
set(_AE_SP_CANDIDATES
	"${AESDK_ROOT}/Headers/SP"
	"${AESDK_ROOT}/Examples/Headers/SP"
	"${AESDK_ROOT}/AfterEffectsSDK/Headers/SP")

foreach(c IN LISTS _AE_HEADER_CANDIDATES)
	if(EXISTS "${c}/AE_Effect.h")
		set(AE_HEADERS "${c}")
		break()
	endif()
endforeach()
foreach(c IN LISTS _AE_UTIL_CANDIDATES)
	if(EXISTS "${c}/AEGP_SuiteHandler.h" OR EXISTS "${c}/Param_Utils.h")
		set(AE_UTIL "${c}")
		break()
	endif()
endforeach()
foreach(c IN LISTS _AE_RES_CANDIDATES)
	if(IS_DIRECTORY "${c}")
		set(AE_RES "${c}")
		break()
	endif()
endforeach()
foreach(c IN LISTS _AE_SP_CANDIDATES)
	if(IS_DIRECTORY "${c}")
		set(AE_SP "${c}")
		break()
	endif()
endforeach()

if(NOT AE_HEADERS)
	message(FATAL_ERROR "Could not find AE_Effect.h under ${AESDK_ROOT}")
endif()

set(_AE_UTIL_SOURCES "")
if(EXISTS "${AE_UTIL}/AEGP_SuiteHandler.cpp")
	list(APPEND _AE_UTIL_SOURCES "${AE_UTIL}/AEGP_SuiteHandler.cpp")
endif()
if(EXISTS "${AE_UTIL}/AEFX_SuiteHelper.cpp")
	list(APPEND _AE_UTIL_SOURCES "${AE_UTIL}/AEFX_SuiteHelper.cpp")
endif()
if(EXISTS "${AE_UTIL}/Smart_Utils.cpp")
	list(APPEND _AE_UTIL_SOURCES "${AE_UTIL}/Smart_Utils.cpp")
endif()
if(EXISTS "${AE_UTIL}/MissingSuiteError.cpp")
	list(APPEND _AE_UTIL_SOURCES "${AE_UTIL}/MissingSuiteError.cpp")
endif()

add_library(ChitChat SHARED
	src/core/MessageParse.cpp
	src/core/ChatLayout.cpp
	src/core/AppearTimeline.cpp
	src/core/ChatArb.cpp
	src/win/ChatRenderer.cpp
	src/win/TranscriptDialog.cpp
	src/plugin/ChitChat.cpp
	src/plugin/Composite.cpp
	src/plugin/ChitChatPiPL.rc
	src/plugin/ChitChat.def
	${_AE_UTIL_SOURCES}
)

target_include_directories(ChitChat PRIVATE
	src/core
	src/win
	src/plugin
	${AE_HEADERS}
	${AE_UTIL}
	${AE_RES}
	${AE_SP}
)

target_compile_definitions(ChitChat PRIVATE
	WINDOWS
	MSWindows
	WIN32
	_WINDOWS
	_USRDLL
)

if(MSVC)
	target_compile_options(ChitChat PRIVATE /W4 /EHsc /utf-8)
	target_link_options(ChitChat PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/ChitChat.def")
endif()

target_link_libraries(ChitChat PRIVATE d2d1 dwrite windowscodecs ole32 comdlg32)

set_target_properties(ChitChat PROPERTIES
	OUTPUT_NAME "ChitChat"
	SUFFIX ".aex"
	PREFIX ""
)

message(STATUS "After Effects SDK headers: ${AE_HEADERS}")
message(STATUS "After building, copy ChitChat.aex into the After Effects Plug-ins/Effects folder and restart AE.")
