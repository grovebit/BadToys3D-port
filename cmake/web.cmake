function(bt3d_configure_web_target target_name)
    set_target_properties(${target_name} PROPERTIES SUFFIX ".html")
    target_compile_definitions(${target_name} PRIVATE
        PLATFORM_WEB
        GRAPHICS_API_OPENGL_ES2
    )
    target_link_options(${target_name} PRIVATE
        "SHELL:-sUSE_GLFW=3"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:-sINVOKE_RUN=0"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['callMain','FS']"
        "SHELL:-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
        "--pre-js" "${CMAKE_SOURCE_DIR}/web/pre.js"
        "-lidbfs.js"
    )
endfunction()
