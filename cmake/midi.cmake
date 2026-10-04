function(bt3d_embed_menu_midi target_name root_dir)
    set(BT3D_MIDI_SRC "${root_dir}/../original/m1.dat" CACHE FILEPATH "Path to original menu MIDI m1.dat embedded into the executable")
    if (NOT IS_ABSOLUTE "${BT3D_MIDI_SRC}")
        get_filename_component(BT3D_MIDI_SRC "${BT3D_MIDI_SRC}" ABSOLUTE BASE_DIR "${root_dir}")
    endif()
    set(BT3D_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
    set(BT3D_EMBEDDED_MIDI_HEADER "${BT3D_GENERATED_DIR}/bt3d_embedded_midi.h")
    set(BT3D_EMBEDDED_MIDI_DEPS "${root_dir}/cmake/embed_binary.cmake")
    if (EXISTS "${BT3D_MIDI_SRC}")
        list(APPEND BT3D_EMBEDDED_MIDI_DEPS "${BT3D_MIDI_SRC}")
    else()
        message(WARNING "Menu MIDI source not found: ${BT3D_MIDI_SRC}. The executable will build without menu music.")
    endif()
    add_custom_command(
        OUTPUT "${BT3D_EMBEDDED_MIDI_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BT3D_GENERATED_DIR}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${BT3D_MIDI_SRC}
            -DOUTPUT=${BT3D_EMBEDDED_MIDI_HEADER}
            -DSYMBOL=bt3d_menu_midi
            -P "${root_dir}/cmake/embed_binary.cmake"
        DEPENDS ${BT3D_EMBEDDED_MIDI_DEPS}
        COMMENT "Embedding menu MIDI"
    )
    add_custom_target(${target_name}_embedded_midi DEPENDS "${BT3D_EMBEDDED_MIDI_HEADER}")

    add_dependencies(${target_name} ${target_name}_embedded_midi)
    target_include_directories(${target_name} PRIVATE "${BT3D_GENERATED_DIR}")
endfunction()
