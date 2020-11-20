cmake_minimum_required(VERSION 3.10.2)

if(NOT CMAKE_MODULE_PATH)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
elseif()
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
endif()

project(AAHU LANGUAGES CXX)

option(BUILD_MAZDA "Builds Mazda version of Android Auto" ON)
option(BUILD_UBUNTU "Builds Ubuntu version of Android Auto" OFF)
if(BUILD_MAZDA)
    set(BUILD_UBUNTU OFF)
    set(OPENSSL_USE_STATIC_LIBS TRUE) #Statically link openssl for Mazda target
    set(Protobuf_USE_STATIC_LIBS ON) #Statically link protobuf for Mazda target
elseif(BUILD_UBUNTU)
    set(BUILD_MAZDA OFF)
endif()

set(protobuf_MODULE_COMPATIBLE ON CACHE BOOL "")

include(deps)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)


protobuf_generate_cpp(PROTO_SRC PROTO_HEADER hu/hu.proto)

set(SRC_FILES
        hu/hu_aap.cpp
        hu/hu_aad.cpp
        hu/hu_ssl.cpp
        hu/hu_usb.cpp
        hu/hu_uti.cpp
        hu/hu_tcp.cpp
        ${PROTO_HEADER} ${PROTO_SRC}

        common/audio.cpp
        common/command_server.cpp
        common/config.cpp
        common/glib_utils.cpp
        common/web++/web++.cpp
        )

set(MAZDA_SRC
        mazda/bt/mzd_bluetooth.cpp
        mazda/gps/mzd_gps.cpp
        mazda/hud/hud.cpp
        mazda/wireless/wireless.cpp
        mazda/usb/usb.cpp
        mazda/callbacks.cpp
        mazda/outputs.cpp
        mazda/main.cpp
        )

set(UBUNTU_SRC
        ubuntu/bt/ub_bluetooth.cpp
        ubuntu/main.cpp
        ubuntu/outputs.cpp
        ubuntu/callbacks.cpp
        )

if(BUILD_MAZDA)
    list(APPEND SRC_FILES ${MAZDA_SRC})
endif()

if(BUILD_UBUNTU)
    list(APPEND SRC_FILES ${UBUNTU_SRC})
endif()

add_executable(headunit ${SRC_FILES})

target_include_directories(headunit PUBLIC
        hu common
        ${GLIB_INCLUDE_DIRS}
        ${GTK3_INCLUDE_DIRS}
        ${USB_INCLUDE_DIRS}
        ${GST_INCLUDE_DIRS}
        ${DBUS_INCLUDE_DIRS}
        ${DBUS_C++_INCLUDE_DIRS}
        ${DBUS_C++-GLIB_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SYSROOT}/usr/include/SDL2
        ${CMAKE_SYSROOT}/mazda/dbus/
        )

target_compile_options(headunit PUBLIC
        -Wno-narrowing
        -D__STDC_FORMAT_MACROS
        -DDBUS_API_SUBJECT_TO_CHANGE
        )

target_link_libraries(headunit PUBLIC
        ${GST_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${ALSA_LIBRARIES}
        ${UDEV_LIBRARIES}
        ${Protobuf_LITE_LIBRARIES}
        OpenSSL::Crypto
        OpenSSL::SSL
        Threads::Threads
        -v
        )

if(BUILD_UBUNTU)
    target_link_libraries(headunit PUBLIC
        ${GTK3_LIBRARIES}
        ${X11_LIBRARIES}
        ${GLFW_STATIC_LIBRARY}
        ${SDL2_LIBRARIES}
        ${USB_LIBRARIES}
        ${UNWIND_LIBRARIES}
        ${DBUS_LIBRARIES}
        ${DBUS_C++_LIBRARIES}
        )
endif()

if(BUILD_MAZDA)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -s")

    target_compile_options(headunit PUBLIC -static-libstdc++ -DCMU=1)

    #Statically link in some libraries that are either different, or not availible on Mazda CMU
    target_link_libraries(headunit PUBLIC
            libdbus-1.a
            libdbus-c++-1.a
            libunwind.a
#            libudev.a
            libusb-1.0.a)
    target_link_libraries(headunit PUBLIC -static-libstdc++)

endif()

install(TARGETS headunit RUNTIME DESTINATION bin)