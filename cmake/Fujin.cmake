# Requires only Git + PowerShell at build time. No Fujin/JS runtime is shipped.
function(dac_add_fujin target)
    find_program(DAC_POWERSHELL NAMES pwsh powershell REQUIRED)
    find_package(Git REQUIRED)
    get_filename_component(dac_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(FUJIN_SOURCE_DIR "${dac_root}/.deps/Fujin" CACHE PATH "Clean Fujin v0.1.0 tagged dependency checkout")
    set(DAC_FUJIN_ACCENT "violet" CACHE STRING "Generated Fujin accent")
    set_property(CACHE DAC_FUJIN_ACCENT PROPERTY STRINGS violet indigo blue cyan teal green orange)
    set(generator "${dac_root}/tools/generate-theme.ps1")
    set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/fujin.hpp")
    set(generator_args -NoProfile -ExecutionPolicy Bypass -File "${generator}"
        -SourceDirectory "${FUJIN_SOURCE_DIR}" -OutputPath "${header}" -Accent "${DAC_FUJIN_ACCENT}")
    execute_process(COMMAND "${DAC_POWERSHELL}" ${generator_args}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Fujin verification failed. Run tools/bootstrap-fujin.ps1 first.\n${output}\n${error}")
    endif()
    # ALWAYS execute verification, including builds with no changed source timestamps.
    if(NOT TARGET dac_fujin_verify)
        add_custom_target(dac_fujin_verify
            COMMAND "${DAC_POWERSHELL}" ${generator_args}
            BYPRODUCTS "${header}"
            COMMENT "Verify pinned Fujin checkout and native theme"
            VERBATIM)
    endif()
    add_dependencies(${target} dac_fujin_verify)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()
