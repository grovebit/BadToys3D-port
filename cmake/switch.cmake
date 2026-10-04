function(bt3d_configure_switch_target target_name)
    target_compile_definitions(${target_name} PRIVATE
        PLATFORM_NX
        GRAPHICS_API_OPENGL_ES2
    )
    # raylib-nx is prebuilt into the devkitPro portlibs (docker/Dockerfile) and
    # must come before the system libraries it links against.
    target_link_libraries(${target_name} PRIVATE
        $ENV{DEVKITPRO}/portlibs/switch/lib/libraylib.a
        EGL
        GLESv2
        glapi
        drm_nouveau
        nx
        m
        stdc++
    )
endfunction()

function(bt3d_configure_switch_packaging target_name)
    set(DEVKITPRO $ENV{DEVKITPRO})
    set(APP_TITLE "Bad Toys 3D")
    set(APP_AUTHOR "Tibo Software / port")
    set(APP_VERSION "0.1.0")
    set(ROMFS_DIR ${CMAKE_SOURCE_DIR}/romfs)
    set(ELF_FILE ${CMAKE_BINARY_DIR}/${target_name})
    set(NACP_FILE ${CMAKE_BINARY_DIR}/${target_name}.nacp)
    set(NRO_FILE ${CMAKE_BINARY_DIR}/${target_name}.nro)
    set(ICON_FILE ${CMAKE_SOURCE_DIR}/icon.jpg)

    add_custom_command(
        OUTPUT ${NACP_FILE}
        COMMAND ${DEVKITPRO}/tools/bin/nacptool --create "${APP_TITLE}" "${APP_AUTHOR}" "${APP_VERSION}" ${NACP_FILE}
        COMMENT "Generating NACP metadata"
    )

    if (BT3D_EMBED_ROMFS AND EXISTS ${ROMFS_DIR})
        set(ROMFS_ARGS --romfsdir=${ROMFS_DIR})
        file(GLOB ROMFS_FILES ${ROMFS_DIR}/*)
        set(NRO_COMMENT "Packaging .nro with romfs")
    else()
        set(ROMFS_ARGS "")
        set(ROMFS_FILES "")
        set(NRO_COMMENT "Packaging .nro (no romfs - create romfs/ with data.pck to embed assets)")
    endif()
    add_custom_command(
        OUTPUT ${NRO_FILE}
        COMMAND ${DEVKITPRO}/tools/bin/elf2nro ${ELF_FILE} ${NRO_FILE}
            --nacp=${NACP_FILE}
            --icon=${ICON_FILE}
            ${ROMFS_ARGS}
        DEPENDS ${target_name} ${NACP_FILE} ${ICON_FILE} ${ROMFS_FILES}
        COMMENT "${NRO_COMMENT}"
    )
    add_custom_target(nro ALL DEPENDS ${NRO_FILE})
endfunction()
