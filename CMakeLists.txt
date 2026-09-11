cmake_minimum_required (VERSION 3.20)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("NiViewer")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external/NiForge)


set(HEADER_FILES
	"src/NiViewer.h"
)

set(SOURCE_FILES
	"src/NiViewer.cpp"
)

add_executable (NiViewer ${SOURCE_FILES} ${HEADER_FILES})

# Link NiForge to the NiViewer target
target_link_libraries(NiViewer PRIVATE NiForge)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NiViewer PROPERTY CXX_STANDARD 20)
endif()

# Copy DLL on Windows post-build
if(WIN32)
    add_custom_command(TARGET NiViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:NiForge>
        $<TARGET_FILE_DIR:NiViewer>
    )
endif()

# TODO: Add tests and install targets if needed.
