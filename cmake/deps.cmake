
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(SDL2 REQUIRED)
find_package(ALSA REQUIRED)
find_package(X11 REQUIRED)

find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_EXECUTABLE)

    pkg_check_modules(GST REQUIRED gstreamer-1.0
                                gstreamer-video-1.0
                                gstreamer-app-1.0
                                gstreamer-audio-1.0
                                gstreamer-codecparsers-1.0)

    pkg_check_modules(USB REQUIRED libusb-1.0)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(UNWIND REQUIRED libunwind)
    pkg_check_modules(UDEV REQUIRED libudev)
endif()

find_package(Git)
if(GIT_EXECUTABLE)
    execute_process(
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
	set(BUILD_HASH ${GIT_HASH})

	if (NOT REPRODUCIBLE)
		execute_process(
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMAND "whoami"
			OUTPUT_VARIABLE GIT_USER
			OUTPUT_STRIP_TRAILING_WHITESPACE
			)
		execute_process(
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMAND "hostname"
			OUTPUT_VARIABLE GIT_HOST
			OUTPUT_STRIP_TRAILING_WHITESPACE
			)
		string(REGEX REPLACE "([^\\])[\\]([^\\])" "\\1\\\\\\\\\\2" GIT_USER ${GIT_USER})
		set(BUILD_HASH ${GIT_USER}@${GIT_HOST}-${GIT_HASH})
	endif()

    message("Git commit hash: ${BUILD_HASH}")

    configure_file(cmake/version.in.h ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
endif()
