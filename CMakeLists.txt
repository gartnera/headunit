cmake_minimum_required(VERSION 3.10.2)

if(NOT CMAKE_MODULE_PATH)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
elseif()
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
endif()

project(AAHU LANGUAGES CXX)

option(BUILD_MAZDA "Builds Mazda version of Android Auto" OFF)
option(BUILD_UBUNTU "Builds Ubuntu version of Android Auto" ON)
if(BUILD_MAZDA)
    set(BUILD_UBUNTU OFF)
elseif(BUILD_UBUNTU)
    set(BUILD_MAZDA OFF)
endif()

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
    mazda/nm/mzd_nightmode.cpp
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
    ${GTK3_INCLUDE_DIRS}
    ${USB_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SYSROOT}/usr/include/SDL2
)

target_compile_options(headunit PUBLIC
    -Wno-narrowing
    -D__STDC_FORMAT_MACROS
    -DDBUS_API_SUBJECT_TO_CHANGE
)

target_link_libraries(headunit PUBLIC
    ${GST_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${X11_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${Protobuf_LIBRARY}
    ${GLFW_STATIC_LIBRARY}
    ${SDL2_LIBRARIES}
    ${UDEV_LIBRARIES}
    ${USB_LIBRARIES}
    ${UNWIND_LIBRARIES}
    OpenSSL::Crypto
    OpenSSL::SSL
    Threads::Threads
    -v
)

if(BUILD_MAZDA)
    add_dependencies(headunit dbus-cplusplus)

    target_compile_options(headunit PUBLIC -static-libstdc++ -DCMU=1)

    target_include_directories(headunit PUBLIC
        ${CMAKE_STAGING_PREFIX}/include
        ${THIRD_PARTY_DIR}/dbus-cplusplus
        ${CMAKE_STAGING_PREFIX}/include/dbus-c++-1)

    target_link_libraries(headunit PUBLIC
        ${CMAKE_STAGING_PREFIX}/lib/libdbus-c++-1.a
        ${CMAKE_STAGING_PREFIX}/lib/libdbus-c++-glib-1.a
        ${DBUS_LIBRARIES})
endif()

install(TARGETS headunit RUNTIME DESTINATION bin)
