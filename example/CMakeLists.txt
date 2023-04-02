add_executable(example example.cpp)

target_link_libraries(
    example

    PRIVATE

        hz_client::hz_client
)

add_executable(map-put map-put.cpp)

target_link_libraries(
    map-put

    PRIVATE

        hz_client::hz_client
)