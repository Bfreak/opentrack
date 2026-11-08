# Trackhat: head-tracking test window (3D cockpit / race car / soldier scenes), opened from the main window.
find_package(Qt6 REQUIRED COMPONENTS OpenGL OpenGLWidgets)

otr_module(head-test BIN NO-I18N)
target_link_libraries(${self} PRIVATE Qt6::OpenGL Qt6::OpenGLWidgets)
target_include_directories(${self} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")

if(WIN32)
    otr_install_lib(Qt6::OpenGL ".")
    otr_install_lib(Qt6::OpenGLWidgets ".")
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        DESTINATION "${opentrack-libexec}/head-test"
        FILE_PERMISSIONS ${opentrack-perms-file}
        DIRECTORY_PERMISSIONS ${opentrack-perms-dir})
