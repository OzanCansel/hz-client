add_executable(example example.cpp)

target_link_libraries(
    example

    PRIVATE

        hz_client::hz_client
)