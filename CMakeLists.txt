cmake_minimum_required(VERSION 3.10...3.24)
project(parking_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(CURL_USE_STATIC_LIBS TRUE)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}")
include(CPack)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Json
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

FetchContent_MakeAvailable(nlohmann_json)

# http server
set(HTTPLIB_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/external")
file(MAKE_DIRECTORY ${HTTPLIB_DOWNLOAD_DIR})
set(HTTPLIB_PATH "${HTTPLIB_DOWNLOAD_DIR}/httplib.h")

if(NOT EXISTS ${HTTPLIB_PATH})
    message(STATUS "Downloading httplib.h from https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h")
    file(DOWNLOAD "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h"
         ${HTTPLIB_PATH}
         TIMEOUT 60
         STATUS download_status)
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download httplib.h")
    endif()
endif()

# Server
add_executable(parking_system_server
    src/main.cpp
    src/auth.cpp
    src/database.cpp
    src/logger.cpp
    src/vehicle.cpp
    src/utils.cpp
)

target_include_directories(parking_system_server
    PRIVATE include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${HTTPLIB_DOWNLOAD_DIR}
)

target_link_libraries(parking_system_server
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PRIVATE pthread
    PRIVATE nlohmann_json::nlohmann_json
)


# Client
add_executable(parking_system_client
    src/client.cpp
)

target_include_directories(parking_system_client
    PRIVATE ${HTTPLIB_DOWNLOAD_DIR}
)

target_link_libraries(parking_system_client
  PRIVATE
    pthread
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Bot
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
include_directories(${TESSERACT_INCLUDE_DIRS})
link_directories(${TESSERACT_LIBRARY_DIRS})

add_executable(parking_system_bot src/bot.cpp)

target_link_libraries(parking_system_bot
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

install(TARGETS parking_system_server parking_system_client parking_system_bot
    RUNTIME DESTINATION .)

install(DIRECTORY configs/ DESTINATION .)
