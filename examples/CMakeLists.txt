find_package(Threads REQUIRED)

cmake_minimum_required (VERSION 2.8.11)

if(CMAKE_THREAD_LIBS_INIT)

    add_executable(spawn1M spawn1M.c)
    set(INC_CSDIR ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (spawn1M PUBLIC ${INC_CSDIR}/../tcpm/include)
    target_link_libraries(spawn1M LINK_PUBLIC tcpm "${CMAKE_THREAD_LIBS_INIT}")

    add_executable(test15GM test15GM.c)
    set(INC_CSDIR ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (test15GM PUBLIC ${INC_CSDIR}/../tcpm/include)
    target_link_libraries(test15GM LINK_PUBLIC tcpm "${CMAKE_THREAD_LIBS_INIT}")

endif()
