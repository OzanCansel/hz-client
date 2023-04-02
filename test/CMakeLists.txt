FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.3.2
)

FetchContent_MakeAvailable(Catch2)

add_executable(connection-test connection-test.cpp)
target_link_libraries(connection-test hz_client::hz_client Catch2::Catch2WithMain)