# ==============================================================================
# MinGW Host Toolchain Configuration
# ==============================================================================

if(MINGW)
    message(STATUS "[GeoRescuX Prototype] MinGW detected: configuring BFD linker.")
    add_compile_options("-fuse-ld=bfd")
    add_link_options("-fuse-ld=bfd")
endif()
