
add_definitions(-std=c11)

include_directories(${PROJECT_SOURCE_DIR}/include)

link_directories(${PROJECT_SOURCE_DIR}/lib)

add_executable(SwiftTest main.c)

IF(WIN32)
    target_link_libraries(SwiftTest wsock32 ws2_32)
    target_link_libraries(SwiftTest libsbp)
ELSE()
    target_link_libraries(SwiftTest libsbp.so)
ENDIF()

add_custom_command(TARGET SwiftTest PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/lib $<TARGET_FILE_DIR:SwiftTest>)

install(TARGETS SwiftTest DESTINATION ${PROJECT_SOURCE_DIR})
