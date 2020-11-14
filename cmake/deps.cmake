
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

if(BUILD_MAZDA)
    set(GEN_DBUS_HEADER_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mazda/dbus/dbusxx-xml2cpp)
    add_custom_command(TARGET dbus-cplusplus POST_BUILD
            COMMAND ${GEN_DBUS_HEADER_COMMAND} cmu_interfaces.xml --proxy=generated_cmu.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mazda/dbus
            COMMENT "Generating generated_cmu.h")

    set(DBUS1_LIBRARY="${TRAVIS_BUILD_DIR}/mazda/m3-toolchain/arm-cortexa9_neon-linux-gnueabi/sysroot/usr/lib/libdbus-1.so.3.7.12")

    set(OPENSSL_CRYPTO_LIBRARY="${TRAVIS_BUILD_DIR}/mazda/m3-toolchain/arm-cortexa9_neon-linux-gnueabi/sysroot/usr/lib/libcrypto.a")
    set(OPENSSL_INCLUDE_DIR="${TRAVIS_BUILD_DIR}/mazda/m3-toolchain/arm-cortexa9_neon-linux-gnueabi/sysroot/usr/include")

    set(Protobuf_LIBRARIES="${TRAVIS_BUILD_DIR}/mazda/protobuf-2.6.1-arm/lib/libprotobuf-lite.a")
    set(Protobuf_INCLUDE_DIR="${TRAVIS_BUILD_DIR}/mazda/protobuf-2.6.1-arm/include")
endif()