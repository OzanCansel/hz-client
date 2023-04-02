cmake_minimum_required( VERSION 3.10 )
project( hz_client )

set(CMAKE_VERBOSE_MAKEFILE ON)

include(FetchContent)

FetchContent_Declare(
    rbs
    GIT_REPOSITORY https://github.com/OzanCansel/rbs.git
)

FetchContent_MakeAvailable(rbs)

add_library( hz_client INTERFACE )
add_library(
    hz_client::hz_client
    
    ALIAS
    
    hz_client
)

find_package( Boost REQUIRED )

target_include_directories(
    rbs

    INTERFACE

        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    hz_client

    INTERFACE

        Boost::boost
        rbs::rbs
)

target_compile_features(
    hz_client

    INTERFACE

        cxx_std_20
)

add_subdirectory( example )