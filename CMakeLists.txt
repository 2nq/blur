cmake_minimum_required(VERSION 3.20)

set(VCPKG_LIBRARY_LINKAGE static)
if(WIN32)
  set(VCPKG_TARGET_TRIPLET
      "x64-windows-static"
      CACHE STRING "")
endif()

# Force static runtime on Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(blur LANGUAGES C CXX)

# global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_BINARY_DIR ${PROJECT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE})

# packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(CLI11 CONFIG REQUIRED)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
find_package(Freetype REQUIRED)
find_package(glad CONFIG REQUIRED)

# source files
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cpp" "src/common/*.hpp"
     "src/common/*.h")

file(GLOB_RECURSE CLI_SOURCES "src/cli/*.cpp" "src/cli/*.hpp" "src/cli/*.h")

file(GLOB_RECURSE GUI_SOURCES "src/gui/*.cpp" "src/gui/*.hpp" "src/gui/*.h")

# collect imgui sources
set(IMGUI_SOURCES
    dependencies/imgui/imgui.cpp
    dependencies/imgui/imgui_demo.cpp
    dependencies/imgui/imgui_draw.cpp
    dependencies/imgui/imgui_tables.cpp
    dependencies/imgui/imgui_widgets.cpp
    dependencies/imgui/backends/imgui_impl_sdl3.cpp
    dependencies/imgui/backends/imgui_impl_opengl3.cpp
    dependencies/imgui/misc/freetype/imgui_freetype.cpp)

file(GLOB_RECURSE RESOURCES "resources/*")

# common settings
function(setup_target target)
  target_include_directories(${target} PRIVATE src)
  target_link_libraries(
    ${target} PRIVATE nlohmann_json::nlohmann_json cpr::cpr Boost::system
                      Boost::filesystem Threads::Threads)
  target_compile_definitions(${target} PRIVATE NOMINMAX BOOST_FILESYSTEM_NO_LIB
                                               BOOST_FILESYSTEM_STATIC_LINK=1)
  if(WIN32)
    set(${target} resources/resources_win32.rc)
  endif()

  if(APPLE)
    target_link_libraries(${target} PRIVATE "-framework CoreFoundation")

    # use bundled libunwind (fixes cli11 exception issue doesn't happen in
    # github runners. idk. maybe it's my pc remove this and do ./blur-cli -h and
    # you get libc++abi: terminating due to uncaught exception of type
    # CLI::CallForHelp: This should be caught in your main function, see
    # examples) taken from https://formulae.brew.sh/formula/llvm todo: remove
    # when this is fixed or find a better solution. maybe this is just my macos
    # version??
    if(DEFINED ENV{HOMEBREW_PREFIX})
      set(LLVM_UNWIND_PATH "$ENV{HOMEBREW_PREFIX}/opt/llvm/lib/unwind")
      if(EXISTS ${LLVM_UNWIND_PATH})
        target_link_libraries(${target} PRIVATE -L${LLVM_UNWIND_PATH} -lunwind)
      endif()
    endif()

    get_target_property(is_bundle ${target} MACOSX_BUNDLE)

    if(is_bundle)
      set(BUILD_RESOURCES_DIR
          "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources")
    else()
      set(BUILD_RESOURCES_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
  else()
    set(BUILD_RESOURCES_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  endif()

  # copy vapoursynth scripts
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUILD_RESOURCES_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/src/vapoursynth" ${BUILD_RESOURCES_DIR}/lib)
endfunction()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DDEBUGMODE -D_DEBUG)
else()
  add_definitions(-DNDEBUG)
endif()

# cli
add_executable(
  blur-cli
  ${COMMON_SOURCES}
  ${CLI_SOURCES}
  ${RESOURCES})
target_link_libraries(
  blur-cli
  PRIVATE CLI11::CLI11)
target_precompile_headers(blur-cli PRIVATE src/cli/cli_pch.h)
setup_target(blur-cli)

# gui
add_executable(
  blur-gui WIN32
  ${COMMON_SOURCES}
  ${GUI_SOURCES}
  ${IMGUI_SOURCES}
  ${RESOURCES})

# imgui
target_include_directories(blur-gui PRIVATE ${PROJECT_SOURCE_DIR}/dependencies/imgui)
target_compile_definitions(blur-gui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# stb
target_include_directories(blur-gui PRIVATE ${PROJECT_SOURCE_DIR}/dependencies/stb)

target_link_libraries(
  blur-gui
  PRIVATE ZLIB::ZLIB SDL3::SDL3 Freetype::Freetype glad::glad)

target_precompile_headers(blur-gui PRIVATE src/gui/gui_pch.h)

if(APPLE)
  target_link_libraries(blur-gui PRIVATE "-framework CoreVideo"
                                         "-framework CoreGraphics")

  # Only create a bundle for Release and RelWithDebInfo builds
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL
                                            "RelWithDebInfo")
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/Info.plist")

    set_target_properties(
      blur-gui PROPERTIES MACOSX_BUNDLE TRUE MACOSX_BUNDLE_INFO_PLIST
                                             ${CMAKE_SOURCE_DIR}/src/Info.plist)

    # Specify the .icns file as the app icon
    set(MACOSX_BUNDLE_ICON_FILE blur.icns)

    # Ensure the .icns file is included in the app bundle's Resources folder
    set_source_files_properties(${CMAKE_SOURCE_DIR}/resources/blur.icns
                                PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    # Link the .icns file to the target
    target_sources(blur-gui PRIVATE ${CMAKE_SOURCE_DIR}/resources/blur.icns)

    # Add a post-build step that will run after the bundle is created
    add_custom_command(
      TARGET blur-gui
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Copying resources to bundle..."
      # Make sure the destination directories exist
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_BUNDLE_DIR:blur-gui>/Contents/Resources/"
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_BUNDLE_DIR:blur-gui>/Contents/Resources/vapoursynth"
      # Copy ci/out directory if it exists
      COMMAND ${CMAKE_COMMAND} -E echo "Copying ci/out files..."
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/ci/out"
              "$<TARGET_BUNDLE_DIR:blur-gui>/Contents/Resources/"
      # Copy vapoursynth scripts
      COMMAND ${CMAKE_COMMAND} -E echo "Copying vapoursynth files..."
      COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src/vapoursynth"
        "$<TARGET_BUNDLE_DIR:blur-gui>/Contents/Resources/lib"
      # Verify the copy worked
      COMMAND ${CMAKE_COMMAND} -E echo "Resource copying complete"
      # The VERBATIM flag ensures command arguments are correctly escaped
      VERBATIM)
  else()
    # For Debug and other build types, disable bundling
    set_target_properties(blur-gui PROPERTIES MACOSX_BUNDLE FALSE)
  endif()
elseif(UNIX)
  target_link_libraries(blur-gui PRIVATE X11 Xext Xrandr)
elseif(WIN32)
  target_link_libraries(blur-gui PRIVATE Shcore)
endif()

set_target_properties(blur-gui PROPERTIES OUTPUT_NAME "blur")
setup_target(blur-gui)
