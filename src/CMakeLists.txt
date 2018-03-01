
include_directories(${PROJECT_SOURCE_DIR}/include)

link_directories(${PROJECT_SOURCE_DIR}/lib)

add_executable(SwiftTest main.cpp ../include/rs232.h rs232.c)

#unix
#target_link_libraries(SwiftTest ${PROJECT_SOURCE_DIR}/lib/libsbp-static.a ${PROJECT_SOURCE_DIR}/lib/libserialport.a)

#windows
#target_link_libraries(SwiftTest ${PROJECT_SOURCE_DIR}/lib/libsbp.dll.a)
target_link_libraries(SwiftTest ${PROJECT_SOURCE_DIR}/lib/cygsbp.dll)

add_custom_command(TARGET SwiftTest PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/lib $<TARGET_FILE_DIR:SwiftTest>)

install(TARGETS SwiftTest DESTINATION ${PROJECT_SOURCE_DIR})
