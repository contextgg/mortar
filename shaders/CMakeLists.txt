find_program(GLSLC glslc REQUIRED)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(SHADERS
    mesh.vert
    mesh.frag
)

set(SPIRV_OUTPUTS "")

foreach(SHADER ${SHADERS})
    set(INPUT ${SHADER_SOURCE_DIR}/${SHADER})
    set(OUTPUT ${SHADER_BINARY_DIR}/${SHADER}.spv)

    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${GLSLC} ${INPUT} -o ${OUTPUT}
        DEPENDS ${INPUT}
        COMMENT "Compiling shader ${SHADER}"
    )

    list(APPEND SPIRV_OUTPUTS ${OUTPUT})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_OUTPUTS})
