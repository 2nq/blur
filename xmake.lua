-- Set minimum xmake version
set_xmakever("2.7.0")

-- Project configuration
set_project("blur")
set_languages("c++20")
set_version("1.0.0")

-- Global settings
add_rules("mode.debug", "mode.release")

-- Set output directories
set_targetdir("$(projectdir)/bin/$(mode)")
set_objectdir("$(projectdir)/build/$(mode)/.objs")

-- For Windows static runtime
if is_plat("windows") then
    add_defines("NOMINMAX")
    add_cxflags("/MT", {force = true})
    if is_mode("debug") then
        add_cxflags("/MTd", {force = true})
    end
end

-- Add include directories
add_includedirs("src")

-- Debug specific settings
if is_mode("debug") then
    add_defines("DEBUGMODE", "_DEBUG")
    set_symbols("debug")
    set_optimize("none")
else
    add_defines("NDEBUG")
    set_optimize("fastest")
end

-- Packages
add_requires("nlohmann_json")
add_requires("cpr")
add_requires("boost 1.86", {configs = {process = true, asio = true, context = true, coroutine = true}})
add_requires("cli11")
add_requires("libsdl3")
add_requires("libsdl3_image")
add_requires("freetype")

-- Function to get source files using os.files (xmake's globbing)
local function get_sources(pattern)
    return os.files(pattern)
end

-- sources
local imgui_sources = {
    "dependencies/imgui/imgui.cpp",
    "dependencies/imgui/imgui_demo.cpp",
    "dependencies/imgui/imgui_draw.cpp",
    "dependencies/imgui/imgui_tables.cpp",
    "dependencies/imgui/imgui_widgets.cpp",
    "dependencies/imgui/backends/imgui_impl_sdl3.cpp",
    "dependencies/imgui/backends/imgui_impl_opengl3.cpp",
    "dependencies/imgui/misc/freetype/imgui_freetype.cpp"
}

-- Register common rules and configurations to be applied to targets
local function common_target_config()
    set_kind("binary")

    add_files("src/common/*.cpp")
    add_defines("NOMINMAX") -- , "BOOST_FILESYSTEM_NO_LIB", "BOOST_FILESYSTEM_STATIC_LINK=1")

    add_includedirs("src")

    add_packages("nlohmann_json", "cpr", "boost")

    if is_plat("windows") then
        add_files("resources/resources_win32.rc")
    elseif is_plat("macosx") then
        add_frameworks("CoreFoundation", "CoreVideo", "CoreGraphics")
    end

    -- -- Copy vapoursynth scripts - use on_build rule to access target correctly
    -- on("build_after", function (target)
    --     local build_resources_dir = target:targetdir()
    --     if is_plat("macosx") and target:kind() == "binary" and is_mode("release", "releasedbg") then
    --         -- For macOS bundles
    --         build_resources_dir = path.join(target:targetdir(), target:name() .. ".app/Contents/Resources")
    --     end

    --     os.mkdir(path.join(build_resources_dir, "lib"))
    --     os.cp("$(projectdir)/src/vapoursynth", path.join(build_resources_dir, "lib"))
    -- end)
end

-- CLI target
target("blur-cli")
    common_target_config()

    add_files("src/cli/*.cpp")

    add_packages("cli11")
    set_pcxxheader("src/cli/cli_pch.h")
target_end()

-- GUI target
target("blur-gui")
    common_target_config()

    add_files("src/gui/*.cpp")
    add_files(imgui_sources)
    add_files("src/dependencies/glad/src/*.c")

    set_basename("blur") -- would use `blur-gui` otherwise
    add_includedirs("dependencies/imgui", "dependencies/stb", "dependencies/glad/include")
    set_pcxxheader("src/gui/gui_pch.h")
    add_defines("IMGUI_IMPL_OPENGL_LOADER_GLAD")
    add_packages("libsdl3", "libsdl3_image", "freetype")

    if is_plat("windows") then
        add_syslinks("Shcore")
    elseif is_plat("linux") then
        add_syslinks("X11", "Xext", "Xrandr")
    elseif is_plat("macosx") then
        -- Only create a bundle for Release builds
        if is_mode("release") or is_mode("releasedbg") then
            add_rules("xcode.bundle")
            -- add_files("resources/blur.icns")
            set_values("xcode.bundle.identifier", "com.blur.app")
            set_values("xcode.bundle.infoplist", "src/Info.plist")
            set_values("xcode.bundle.icon", "resources/blur.icns")

            after_install(function (target)
                local bundle_dir = path.join("$(buildir)/$(plat)/$(arch)/$(mode)/blur.app")
                local resources_dir = path.join(bundle_dir, "Contents/Resources")

                -- Create directories
                os.mkdir(resources_dir)
                os.mkdir(path.join(resources_dir, "vapoursynth"))

                -- Copy CI output if it exists
                if os.isdir("ci/out") then
                    os.cp("ci/out", resources_dir)
                end

                -- Copy vapoursynth scripts
                os.cp("src/vapoursynth", path.join(resources_dir, "lib"))
            end)
        end
    end
target_end()
