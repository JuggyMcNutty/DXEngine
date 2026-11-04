cmake_minimum_required(VERSION 3.11)
project(dxwidgets)

if(WIN32)
	option(ENABLE_SDL3 "Enable SDL3 backend. Optional on all platforms" OFF)
	option(ENABLE_SDL2 "Enable SDL2 backend. Optional on all platforms" OFF)
else()
	option(ENABLE_SDL3 "Enable SDL3 backend. Optional on all platforms" ON)
	option(ENABLE_SDL2 "Enable SDL2 backend. Optional on all platforms" ON)
endif()

# An embedded target (a handheld running a vendor SDL2 over EGL/Vulkan) has no
# X11, no Wayland compositor, no dbus and no desktop GL. Turning those backends
# off lets the library build against such a sysroot; window.cpp already has
# null-returning stubs for every backend whose USE_* define is absent.
option(ENABLE_X11 "Enable the X11 display backend" ON)
option(ENABLE_WAYLAND "Enable the Wayland display backend" ON)

# SDL2's fullscreen as a borderless window covering the display, set up while
# the window is hidden, in place of SDL_SetWindowFullscreen: a vendor SDL2 (the
# TrimUI Smart Pro's mali backend) deadlocks switching a GL window that way.
# Off, it is SDL's own FULLSCREEN_DESKTOP.
option(DXE_SDL2_BORDERLESS_FULLSCREEN "SDL2 fullscreen as a borderless window, for a vendor SDL2 that deadlocks in SDL_SetWindowFullscreen" OFF)

if (UNIX)
	include(FindPkgConfig)
	if (ENABLE_X11 OR ENABLE_WAYLAND)
		pkg_check_modules(DBUS dbus-1)
		if (NOT DBUS_FOUND)
			pkg_check_modules(DBUS dbus) # Fedora Linux looks for dbus instead
		endif()
		# EGL is how the X11 and Wayland backends get a GL context. Not required:
		# an SDL2-only build never asks for one.
		find_package(OpenGL COMPONENTS EGL)
	endif()
	if (ENABLE_WAYLAND)
		pkg_check_modules(WAYLANDPP
			wayland-client
			wayland-client++
			wayland-client-extra++
			wayland-client-unstable++
			wayland-cursor++
			wayland-egl++
			xkbcommon>=0.5.0
		)
	endif()
	pkg_check_modules(GLIB glib-2.0)
	pkg_check_modules(FONTCONFIG fontconfig)
endif()

# SDL3 finding stuff
# Optional on all platforms
# If both SDL3 and SDL2 are available, only SDL3 support will be enabled.
if(ENABLE_SDL3)
	find_package(SDL3 QUIET)
	if(NOT ${SDL3_FOUND})
		include(FindPkgConfig)
		pkg_search_module(SDL3 sdl3)
	endif()
endif()

# SDL2 finding stuff
# Optional on all platforms
if(ENABLE_SDL2)
	find_package(SDL2 QUIET)
	if(NOT ${SDL2_FOUND})
		include(FindPkgConfig)
		pkg_search_module(SDL2 sdl2)
	endif()
endif()

set(DXWIDGETS_SOURCES
	src/core/canvas.cpp
	src/core/font.cpp
	src/core/font_impl.h
	src/core/image.cpp
	src/core/layout.cpp
	src/core/span_layout.cpp
	src/core/timer.cpp
	src/core/widget.cpp
	src/core/theme.cpp
	src/core/theme_default_stylesheet.h
	src/core/theme_stylesheet_document.cpp
	src/core/theme_stylesheet_document.h
	src/core/theme_stylesheet_tokenizer.cpp
	src/core/theme_stylesheet_tokenizer.h
	src/core/border_image_renderer.cpp
	src/core/border_image_renderer.h
	src/core/utf8reader.cpp
	src/core/pathfill.cpp
	src/core/truetypefont.cpp
	src/core/truetypefont.h
	src/core/picopng/picopng.cpp
	src/core/picopng/picopng.h
	src/core/nanosvg/nanosvg.cpp
	src/core/nanosvg/nanosvg.h
	src/core/nanosvg/nanosvgrast.h
	src/widgets/dialog/dialog.cpp
	src/widgets/dialog/messagebox.cpp
	src/widgets/dialog/textinputdialog.cpp
	src/widgets/lineedit/lineedit.cpp
	src/widgets/mainwindow/mainwindow.cpp
	src/widgets/menubar/menubar.cpp
	src/widgets/scrollbar/scrollbar.cpp
	src/widgets/scrollwidget/scrollwidget.cpp
	src/widgets/slider/slider.cpp
	src/widgets/statusbar/statusbar.cpp
	src/widgets/textedit/textedit.cpp
	src/widgets/toolbar/toolbar.cpp
	src/widgets/toolbar/toolbarbutton.cpp
	src/widgets/imagebox/imagebox.cpp
	src/widgets/textlabel/textlabel.cpp
	src/widgets/pushbutton/pushbutton.cpp
	src/widgets/checkboxlabel/checkboxlabel.cpp
	src/widgets/radiobuttonlabel/radiobuttonlabel.cpp
	src/widgets/dropdown/dropdown.cpp
	src/widgets/listview/listview.cpp
	src/widgets/treeview/treeview.cpp
	src/widgets/tabwidget/tabwidget.cpp
	src/widgets/layout/hboxlayout.cpp
	src/widgets/layout/vboxlayout.cpp
	src/window/window.cpp
	src/window/stub/stub_open_folder_dialog.cpp
	src/window/stub/stub_open_folder_dialog.h
	src/window/stub/stub_open_file_dialog.cpp
	src/window/stub/stub_open_file_dialog.h
	src/window/stub/stub_save_file_dialog.cpp
	src/window/stub/stub_save_file_dialog.h
	src/systemdialogs/open_folder_dialog.cpp
	src/systemdialogs/open_file_dialog.cpp
	src/systemdialogs/save_file_dialog.cpp
)

set(DXWIDGETS_INCLUDES
	include/dxwidgets/core/canvas.h
	include/dxwidgets/core/colorf.h
	include/dxwidgets/core/font.h
	include/dxwidgets/core/image.h
	include/dxwidgets/core/rect.h
	include/dxwidgets/core/orientation.h
	include/dxwidgets/core/pathfill.h
	include/dxwidgets/core/span_layout.h
	include/dxwidgets/core/timer.h
	include/dxwidgets/core/widget.h
	include/dxwidgets/core/theme.h
	include/dxwidgets/core/utf8reader.h
	include/dxwidgets/core/resourcedata.h
	include/dxwidgets/widgets/dialog/dialog.h
	include/dxwidgets/widgets/dialog/messagebox.h
	include/dxwidgets/widgets/dialog/textinputdialog.h
	include/dxwidgets/widgets/lineedit/lineedit.h
	include/dxwidgets/widgets/mainwindow/mainwindow.h
	include/dxwidgets/widgets/menubar/menubar.h
	include/dxwidgets/widgets/scrollbar/scrollbar.h
	include/dxwidgets/widgets/scrollwidget/scrollwidget.h
	include/dxwidgets/widgets/slider/slider.h
	include/dxwidgets/widgets/statusbar/statusbar.h
	include/dxwidgets/widgets/textedit/textedit.h
	include/dxwidgets/widgets/toolbar/toolbar.h
	include/dxwidgets/widgets/toolbar/toolbarbutton.h
	include/dxwidgets/widgets/imagebox/imagebox.h
	include/dxwidgets/widgets/textlabel/textlabel.h
	include/dxwidgets/widgets/pushbutton/pushbutton.h
	include/dxwidgets/widgets/checkboxlabel/checkboxlabel.h
	include/dxwidgets/widgets/radiobuttonlabel/radiobuttonlabel.h
	include/dxwidgets/widgets/listview/listview.h
	include/dxwidgets/widgets/treeview/treeview.h
	include/dxwidgets/widgets/tabwidget/tabwidget.h
	include/dxwidgets/widgets/layout/hboxlayout.h
	include/dxwidgets/widgets/layout/vboxlayout.h
	include/dxwidgets/window/window.h
	include/dxwidgets/window/x11nativehandle.h
	include/dxwidgets/window/waylandnativehandle.h
	include/dxwidgets/window/win32nativehandle.h
	include/dxwidgets/window/sdlnativehandle.h
	include/dxwidgets/systemdialogs/open_folder_dialog.h
	include/dxwidgets/systemdialogs/open_file_dialog.h
	include/dxwidgets/systemdialogs/save_file_dialog.h
)

set(DXWIDGETS_WIN32_SOURCES
	src/window/win32/win32_display_backend.cpp
	src/window/win32/win32_display_backend.h
	src/window/win32/win32_display_window.cpp
	src/window/win32/win32_display_window.h
	src/window/win32/win32_open_folder_dialog.cpp
	src/window/win32/win32_open_folder_dialog.h
	src/window/win32/win32_open_file_dialog.cpp
	src/window/win32/win32_open_file_dialog.h
	src/window/win32/win32_save_file_dialog.cpp
	src/window/win32/win32_save_file_dialog.h
	src/window/win32/win32_util.h
	src/window/win32/opengl_creation_helper.cpp
	src/window/win32/opengl_creation_helper.h
	src/core/resourcedata_win.cpp
)

set(DXWIDGETS_DBUS_SOURCES
	src/window/dbus/dbus_open_folder_dialog.cpp
	src/window/dbus/dbus_open_folder_dialog.h
	src/window/dbus/dbus_open_file_dialog.cpp
	src/window/dbus/dbus_open_file_dialog.h
	src/window/dbus/dbus_save_file_dialog.cpp
	src/window/dbus/dbus_save_file_dialog.h
)

set(DXWIDGETS_UNIX_SOURCES
	src/core/resourcedata_unix.cpp
)

set(DXWIDGETS_SDL2_SOURCES
	src/window/sdl2/sdl2_display_backend.cpp
	src/window/sdl2/sdl2_display_backend.h
	src/window/sdl2/sdl2_display_window.cpp
	src/window/sdl2/sdl2_display_window.h
)

set(DXWIDGETS_SDL3_SOURCES
	src/window/sdl3/sdl3_display_backend.cpp
	src/window/sdl3/sdl3_display_backend.h
	src/window/sdl3/sdl3_display_window.cpp
	src/window/sdl3/sdl3_display_window.h
)

set(DXWIDGETS_X11_SOURCES
	src/window/x11/x11_display_backend.cpp
	src/window/x11/x11_display_backend.h
	src/window/x11/x11_display_window.cpp
	src/window/x11/x11_display_window.h
	src/window/x11/x11_connection.cpp
	src/window/x11/x11_connection.h
)

set(DXWIDGETS_WAYLAND_SOURCES
	src/window/wayland/wayland_display_backend.cpp
	src/window/wayland/wayland_display_backend.h
	src/window/wayland/wayland_display_window.cpp
	src/window/wayland/wayland_display_window.h
	src/window/wayland/wl_fractional_scaling_protocol.cpp
	src/window/wayland/wl_fractional_scaling_protocol.hpp
	src/window/wayland/wl_cursor_shape.cpp
	src/window/wayland/wl_cursor_shape.hpp
	src/window/wayland/wl_xdg_dialog_v1.cpp
	src/window/wayland/wl_xdg_dialog_v1.hpp
	src/window/wayland/wl_xdg_toplevel_icon.cpp
	src/window/wayland/wl_xdg_toplevel_icon.hpp
)

source_group("src" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("src\\core" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/.+")
source_group("src\\core\\picopng" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/picopng/.+")
source_group("src\\core\\nanosvg" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/nanosvg/.+")
source_group("src\\widgets" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/.+")
source_group("src\\widgets\\dialog" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/dialog/.+")
source_group("src\\widgets\\lineedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/lineedit/.+")
source_group("src\\widgets\\mainwindow" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/mainwindow/.+")
source_group("src\\widgets\\menubar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/menubar/.+")
source_group("src\\widgets\\scrollbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/scrollbar/.+")
source_group("src\\widgets\\scrollwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/scrollwidget/.+")
source_group("src\\widgets\\statusbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/statusbar/.+")
source_group("src\\widgets\\textedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/textedit/.+")
source_group("src\\widgets\\toolbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/toolbar/.+")
source_group("src\\widgets\\imagebox" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/imagebox/.+")
source_group("src\\widgets\\textlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/textlabel/.+")
source_group("src\\widgets\\pushbutton" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/pushbutton/.+")
source_group("src\\widgets\\checkboxlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/checkboxlabel/.+")
source_group("src\\widgets\\radiobuttonlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/radiobuttonlabel/.+")
source_group("src\\widgets\\dropdown" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/dropdown/.+")
source_group("src\\widgets\\listview" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/listview/.+")
source_group("src\\widgets\\tabwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/tabwidget/.+")
source_group("src\\window" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/.+")
source_group("src\\window\\stub" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/stub/.+")
source_group("src\\window\\win32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/win32/.+")
source_group("src\\window\\sdl2" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/sdl2/.+")
source_group("src\\window\\sdl3" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/sdl3/.+")
source_group("src\\window\\x11" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/x11/.+")
source_group("src\\window\\wayland" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/wayland/.+")
source_group("src\\window\\dbus" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/dbus/.+")
source_group("src\\systemdialogs" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/systemdialogs/.+")
source_group("include" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/.+")
source_group("include\\core" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/core/.+")
source_group("include\\widgets" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/.+")
source_group("include\\widgets\\lineedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/lineedit/.+")
source_group("include\\widgets\\mainwindow" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/mainwindow/.+")
source_group("include\\widgets\\menubar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/menubar/.+")
source_group("include\\widgets\\scrollbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/scrollbar/.+")
source_group("include\\widgets\\scrollwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/scrollwidget/.+")
source_group("include\\widgets\\statusbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/statusbar/.+")
source_group("include\\widgets\\textedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/textedit/.+")
source_group("include\\widgets\\toolbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/toolbar/.+")
source_group("include\\widgets\\imagebox" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/imagebox/.+")
source_group("include\\widgets\\textlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/textlabel/.+")
source_group("include\\widgets\\pushbutton" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/pushbutton/.+")
source_group("include\\widgets\\checkboxlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/checkboxlabel/.+")
source_group("include\\widgets\\radiobuttonlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/radiobuttonlabel/.+")
source_group("include\\widgets\\listview" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/listview/.+")
source_group("include\\widgets\\tabwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/widgets/tabwidget/.+")
source_group("include\\window" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/window/.+")
source_group("include\\systemdialogs" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/dxwidgets/systemdialogs/.+")

# Include directory to external projects using dxwidgets
include_directories(include)

# Internal include dirs for building dxwidgets
set(DXWIDGETS_INCLUDE_DIRS include/dxwidgets src)

set(DXWIDGETS_COMPILE_OPTIONS)

if(WIN32)
	list(APPEND DXWIDGETS_SOURCES ${DXWIDGETS_WIN32_SOURCES})
	set(DXWIDGETS_DEFINES -DUNICODE -D_UNICODE)
	set(DXWIDGETS_LINK_OPTIONS)
	# The Win32 window: DWM for its frame, WGL for its OpenGL context.
	set(DXWIDGETS_LIBS dwmapi opengl32)
	if(MSVC)
		# Use all cores for compilation
		set(DXWIDGETS_COMPILE_OPTIONS ${DXWIDGETS_COMPILE_OPTIONS} /MP)

		# Ignore specific warnings
		#set(DXWIDGETS_COMPILE_OPTIONS ${DXWIDGETS_COMPILE_OPTIONS} /wd4244 /wd4267 /wd4005 /wd4018)

		# Don't slow down std containers in debug builds
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES})

		# Ignore warning about legacy CRT functions
		#set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -D_CRT_SECURE_NO_WARNINGS)
	endif()
else()
	set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_UNIX_SOURCES})
	set(DXWIDGETS_LIBS ${CMAKE_DL_LIBS})
	set(DXWIDGETS_DEFINES -DUNIX -D_UNIX)
	set(DXWIDGETS_LINK_OPTIONS -pthread)
	if (ENABLE_X11)
		set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_X11_SOURCES})
		set(DXWIDGETS_LIBS ${DXWIDGETS_LIBS} -lX11 -lXi -lXext)
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DUSE_X11)
	endif()
	if (DBUS_FOUND)
		# Bazzite (and probably Fedora 42) keeps the platform-specific dbus
		# headers in this folder; a build against a sysroot has its own, by
		# pkg-config, and a compiler that refuses the machine's.
		if (NOT CMAKE_SYSROOT)
			include_directories("/usr/lib64/dbus-1.0/include")
		endif()
		set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_DBUS_SOURCES})
		set(DXWIDGETS_INCLUDE_DIRS ${DXWIDGETS_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
		set(DXWIDGETS_LIBS ${DXWIDGETS_LIBS} ${DBUS_LDFLAGS})
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DUSE_DBUS)
	endif()
	if (OpenGL_FOUND)
		set(DXWIDGETS_INCLUDE_DIRS ${DXWIDGETS_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${OPENGL_EGL_INCLUDE_DIRS})
		set(DXWIDGETS_LIBS ${DXWIDGETS_LIBS} ${OPENGL_opengl_LIBRARY} ${OPENGL_egl_LIBRARY} -lEGL)
	endif()
	if (WAYLANDPP_FOUND)
		set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_WAYLAND_SOURCES})
		set(DXWIDGETS_INCLUDE_DIRS ${DXWIDGETS_INCLUDE_DIRS} ${WAYLANDPP_INCLUDE_DIRS})
		set(DXWIDGETS_LIBS ${DXWIDGETS_LIBS} ${WAYLANDPP_LDFLAGS})
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DUSE_WAYLAND)
	endif()

	# The system-font lookup asks GSettings what the desktop UI font is and
	# resolves it with fontconfig. With neither available, resourcedata_unix.cpp
	# looks for the usual font files. (The engine's own UI fonts are
	# DXEngine.pk3's.)
	if (GLIB_FOUND AND FONTCONFIG_FOUND)
		set(DXWIDGETS_INCLUDE_DIRS ${DXWIDGETS_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ${FONTCONFIG_INCLUDE_DIRS})
		set(DXWIDGETS_LIBS ${DXWIDGETS_LIBS} ${GLIB_LDFLAGS} -lgio-2.0 ${FONTCONFIG_LDFLAGS})
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DDXWIDGETS_DESKTOP_FONTS)
	endif()
endif()

if(SDL3_FOUND)
	set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_SDL3_SOURCES})
	set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DUSE_SDL3)
endif()

if(SDL2_FOUND AND NOT SDL3_FOUND)
	set(DXWIDGETS_SOURCES ${DXWIDGETS_SOURCES} ${DXWIDGETS_SDL2_SOURCES})
	set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DUSE_SDL2)
	if(DXE_SDL2_BORDERLESS_FULLSCREEN)
		set(DXWIDGETS_DEFINES ${DXWIDGETS_DEFINES} -DDXE_SDL2_BORDERLESS_FULLSCREEN)
	endif()
endif()

add_library(dxwidgets STATIC ${DXWIDGETS_SOURCES} ${DXWIDGETS_INCLUDES})
target_compile_options(dxwidgets PRIVATE ${DXWIDGETS_COMPILE_OPTIONS})
target_compile_definitions(dxwidgets PRIVATE ${DXWIDGETS_DEFINES})
target_include_directories(dxwidgets PRIVATE ${DXWIDGETS_INCLUDE_DIRS})
target_link_options(dxwidgets PRIVATE ${DXWIDGETS_LINK_OPTIONS})
target_link_libraries(dxwidgets PRIVATE ${DXWIDGETS_LIBS})
# SDL can arrive either from find_package (which defines the imported target
# and SDLn_LIBRARY) or from pkg-config (which defines SDLn_LIBRARIES instead).
# Cross builds take the pkg-config path, where SDLn_LIBRARY is empty -- so try
# all three rather than only the first two.
if(SDL3_FOUND)
	target_include_directories(dxwidgets PRIVATE ${SDL3_INCLUDE_DIRS})
	if(TARGET SDL3::SDL3)
		target_link_libraries(dxwidgets PRIVATE SDL3::SDL3)
	elseif(SDL3_LIBRARIES)
		target_link_libraries(dxwidgets PRIVATE ${SDL3_LIBRARIES})
		target_link_directories(dxwidgets PRIVATE ${SDL3_LIBRARY_DIRS})
	else()
		target_link_libraries(dxwidgets PRIVATE ${SDL3_LIBRARY})
	endif()
endif()
if(SDL2_FOUND AND NOT SDL3_FOUND)
	target_include_directories(dxwidgets PRIVATE ${SDL2_INCLUDE_DIRS})
	if(TARGET SDL2::SDL2)
		target_link_libraries(dxwidgets PRIVATE SDL2::SDL2)
	elseif(SDL2_LIBRARIES)
		target_link_libraries(dxwidgets PRIVATE ${SDL2_LIBRARIES})
		target_link_directories(dxwidgets PRIVATE ${SDL2_LIBRARY_DIRS})
	else()
		target_link_libraries(dxwidgets PRIVATE ${SDL2_LIBRARY})
	endif()
endif()
set_target_properties(dxwidgets PROPERTIES CXX_STANDARD 20)
target_compile_options(dxwidgets PRIVATE ${CXX_WARNING_FLAGS})
# picoPNG, third-party: GCC takes its decoder's header, read after a decode
# that failed early, for one that may be unset
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_property(SOURCE src/core/picopng/picopng.cpp APPEND PROPERTY COMPILE_OPTIONS -Wno-maybe-uninitialized)
endif()
