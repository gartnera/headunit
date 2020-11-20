find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(ALSA REQUIRED)


find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_EXECUTABLE)

    pkg_check_modules(GST REQUIRED gstreamer-0.10
            gstreamer-video-0.10
            gstreamer-app-0.10
            gstreamer-audio-0.10
            )

    pkg_check_modules(USB REQUIRED libusb-1.0)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    pkg_check_modules(UNWIND REQUIRED libunwind)
    pkg_check_modules(UDEV REQUIRED libudev)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    pkg_check_modules(DBUS_C++ REQUIRED dbus-c++-1)
    if(BUILD_UBUNTU)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        find_package(X11 REQUIRED)
        find_package(SDL2 REQUIRED)
    endif()

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

    set(GEN_DBUS_HEADER_COMMAND /home/silverchris/headunit/mazda/dbus/dbusxx-xml2cpp)

    execute_process(COMMAND ${GEN_DBUS_HEADER_COMMAND} cmu_interfaces.xml --proxy=generated_cmu.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mazda/dbus)

#    add_custom_target(generated_cmu ALL DEPENDS generated_cmu.h)

endif()