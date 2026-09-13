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

include(FetchContent)

# Add NiForge as a subdirectory
add_subdirectory(external/NiForge)

# Add GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Add GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# Add ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw opengl32)

# Add PugiXML
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
)
FetchContent_MakeAvailable(pugixml)

# Add zLib
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.2
)
FetchContent_MakeAvailable(zlib)

set(HEADER_FILES
	"src/NiViewer.h"

    "src/Core/SceneTypes.hpp"
    "src/Core/Camera.hpp"
    "src/Core/Framebuffer.hpp"
    "src/Core/Shader.hpp"
    "src/Core/Zone.hpp"

    "src/Rendering/SceneRenderer.hpp"

    "src/UI/ViewerUI.hpp"
    "src/UI/IViewerWindow.hpp"
    "src/UI/ViewerContext.hpp"
    "src/UI/Views/BlockListView.hpp"
    "src/UI/Views/SceneHierarchyView.hpp"
    "src/UI/Views/ViewerControlsView.hpp"
    "src/UI/Views/ZoneExplorerView.hpp"
    "src/UI/Views/ViewportView.hpp"

    "src/AssetExtraction/MeshExtractor.hpp"
    "src/AssetExtraction/TextureManager.hpp"
    "src/AssetExtraction/ZoneParser.hpp"
    "src/AssetExtraction/FileMapping.hpp"
    "src/AssetExtraction/Wad/Archive.hpp"
    "src/AssetExtraction/Wad/FileEntry.hpp"
    "src/AssetExtraction/Wad/AssetData.hpp"
    "src/AssetExtraction/Wad/ByteReader.hpp"
)

set(SOURCE_FILES
	"src/NiViewer.cpp"
    "src/glad_impl.cpp"

    "src/Core/Camera.cpp"
    "src/Core/Framebuffer.cpp"
    "src/Core/Shader.cpp"

    "src/Rendering/SceneRenderer.cpp"

    "src/UI/ViewerUI.cpp"
    "src/UI/Views/BlockListView.cpp"
    "src/UI/Views/SceneHierarchyView.cpp"
    "src/UI/Views/ViewerControlsView.cpp"
    "src/UI/Views/ZoneExplorerView.cpp"
    "src/UI/Views/ViewportView.cpp"

    "src/AssetExtraction/MeshExtractor.cpp"
    "src/AssetExtraction/TextureManager.cpp"
    "src/AssetExtraction/ZoneParser.cpp"
    "src/AssetExtraction/FileMapping.cpp"
)

add_executable (NiViewer ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(NiViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${CMAKE_CURRENT_SOURCE_DIR}/external/NiForge/src"
)

# Link GLFW, GLM, and ImGui to the NiViewer target
target_include_directories(NiViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/viewer ${imgui_SOURCE_DIR}/backends ${glfw_SOURCE_DIR}/deps) # ??
target_link_libraries(NiViewer PRIVATE
    NiForge
    imgui
    glm::glm
    opengl32
    pugixml::pugixml
    ZLIB::ZLIB
)

# Copy DLL on Windows post-build
if(WIN32)
    add_custom_command(TARGET NiViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:NiForge>
        $<TARGET_FILE_DIR:NiViewer>
    )
    add_custom_command(TARGET NiViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/external/NiForge/tests/data"
        $<TARGET_FILE_DIR:NiViewer>/data
        COMMENT "Copying sample data to NiViewer output directory..."
    )
endif()

# TODO: Add tests and install targets if needed.
