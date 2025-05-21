project(
  'blur',
  ['c', 'cpp'],
  version: '1.0.0',
  # meson_version: '>1.1',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=release',
    'b_ndebug=if-release',

    'cmake_prefix_path='
    + run_command(
      'python3',
      '-c', '''
import os
from pathlib import Path

triplet = 'arm64-osx'
vcpkg_path = Path('./vcpkg_installed').resolve()
prefix_path = vcpkg_path / triplet
print(str(prefix_path))
''',
    ).stdout().strip(),
  ],
)

# # For Windows, enforce static libraries
# if host_machine.system() == 'windows'
#   set_option('default_library', 'static')
#   if get_option('buildtype') == 'debug'
#     add_project_arguments('/MTd', language: ['c', 'cpp'])
#   else
#     add_project_arguments('/MT', language: ['c', 'cpp'])
#   endif
# endif

# Platform defines
add_project_arguments(
  '-DNOMINMAX',
  # '-DBOOST_FILESYSTEM_NO_LIB',
  # '-DBOOST_FILESYSTEM_STATIC_LINK=1',
  language: 'cpp',
)

# Dependencies
deps_common = [
  dependency('nlohmann_json'),
  dependency('cpr'),
  dependency('boost', modules: ['filesystem', 'system']),
]

deps_cli = [dependency('cli11')]

deps_gui = [
  dependency('sdl3'),
  dependency('sdl3_image'),
  dependency('freetype2'),
  dependency('zlib'),
  dependency('libjpeg'),
]

# Platform-specific dependencies
if host_machine.system() == 'darwin'
  deps_gui += [
    dependency(
      'appleframeworks',
      modules: [
        'CoreFoundation',
        'CoreVideo',
        'CoreGraphics',
        'Security',
        'SystemConfiguration',
      ],
    ),
  ]
elif host_machine.system() == 'linux'
  deps_gui += [dependency('x11'), dependency('xext'), dependency('xrandr')]
elif host_machine.system() == 'windows'
  compiler = meson.get_compiler('cpp')
  deps_gui += [compiler.find_library('shcore')]
endif

# Include directories
common_include_dirs = [include_directories('src')]
cli_include_dirs = []
gui_include_dirs = [
  include_directories('dependencies/imgui'),
  include_directories('dependencies/stb'),
  include_directories('dependencies/glad/include'),
]

# Source files
common_src = [
  'src/common/blur.cpp',
  'src/common/config_app.cpp',
  'src/common/config_base.cpp',
  'src/common/config_blur.cpp',
  'src/common/config_presets.cpp',
  'src/common/rendering.cpp',
  'src/common/rendering_frame.cpp',
  'src/common/updates.cpp',
  'src/common/utils.cpp',
]

cli_src = [
  'src/cli/cli.cpp',
  'src/cli/cli_pch.cpp',
  'src/cli/main.cpp',
]

gui_src = [
  'src/gui/gui.cpp',
  'src/gui/gui_pch.cpp',
  'src/gui/main.cpp',
  'src/gui/render/font/font.cpp',
  'src/gui/render/primitives/color.cpp',
  'src/gui/render/primitives/rect.cpp',
  'src/gui/render/render.cpp',
  'src/gui/renderer.cpp',
  'src/gui/sdl.cpp',
  'src/gui/tasks.cpp',
  'src/gui/ui/elements/bar.cpp',
  'src/gui/ui/elements/button.cpp',
  'src/gui/ui/elements/checkbox.cpp',
  'src/gui/ui/elements/dropdown.cpp',
  'src/gui/ui/elements/image.cpp',
  'src/gui/ui/elements/notification.cpp',
  'src/gui/ui/elements/separator.cpp',
  'src/gui/ui/elements/slider.cpp',
  'src/gui/ui/elements/text.cpp',
  'src/gui/ui/elements/text_input.cpp',
  'src/gui/ui/keys.cpp',
  'src/gui/ui/ui.cpp',

  # dependencies
  'dependencies/imgui/imgui.cpp',
  'dependencies/imgui/imgui_demo.cpp',
  'dependencies/imgui/imgui_draw.cpp',
  'dependencies/imgui/imgui_tables.cpp',
  'dependencies/imgui/imgui_widgets.cpp',
  'dependencies/imgui/backends/imgui_impl_sdl3.cpp',
  'dependencies/imgui/backends/imgui_impl_opengl3.cpp',
  'dependencies/imgui/misc/freetype/imgui_freetype.cpp',
  'dependencies/glad/src/glad.c',
]

# Resources
resources = []
if host_machine.system() == 'windows'
  windows = import('windows')
  resources = [windows.compile_resources('resources/resources_win32.rc')]
endif

# CLI executable
executable(
  'blur-cli',
  common_src + cli_src + resources,
  include_directories: common_include_dirs + cli_include_dirs,
  dependencies: deps_common + deps_cli,
  cpp_pch: 'src/cli/cli_pch.h',
  install: true,
)

# GUI executable
executable(
  'blur',
  common_src + gui_src + resources,
  include_directories: common_include_dirs + gui_include_dirs,
  dependencies: deps_common + deps_gui,
  cpp_pch: 'src/gui/gui_pch.h',
  cpp_args: ['-DIMGUI_IMPL_OPENGL_LOADER_GLAD'],
  install: true,
  win_subsystem: host_machine.system() == 'windows' ? 'windows' : 'console',
)

# Install vapoursynth scripts
install_subdir('src/vapoursynth', install_dir: get_option('libdir') / 'blur')

# macOS bundle support for release builds
if host_machine.system() == 'darwin' and get_option('buildtype') != 'debug'
  bundle_resources = get_option('datadir') / 'blur'
  install_data('resources/blur.icns', install_dir: bundle_resources)

  # Use existing Info.plist
  install_data('src/Info.plist', install_dir: get_option('libdir') / 'blur')
endif
