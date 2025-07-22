
add_executable(vbz_example vbz_example.cpp)
target_link_libraries(vbz_example
    PUBLIC
        vbz
    ${CMAKE_THREAD_LIBS_INIT}
)
