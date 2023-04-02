add_executable(map-put map-put.cpp)

target_link_libraries(
    map-put

    PRIVATE

        hz_client::hz_client
)