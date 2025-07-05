if(WIN32)
    set(gitkf_lib_platform win)
    set(gitkf_lib_platform_libraries
        Crypt32.lib
        Rpcrt4.lib
        Winhttp.lib
    )
else()
    set(gitkf_lib_platform linux)
    set(gitkf_lib_platform_libraries)
endif()

add_library(gitkf_lib)
target_sources(gitkf_lib PUBLIC FILE_SET CXX_MODULES FILES
    client_exception.cpp
    git_repository.cpp
    git_smart_pointer.cpp
    gitkf.cpp
    line_reader.cpp
    module.cpp
    option.cpp
    ring_buffer.cpp
    ${gitkf_lib_platform}/platform_utils.cpp
)

target_link_libraries(gitkf_lib
    PUBLIC
        gitkf_res
    INTERFACE
        libgit2package       
        ${gitkf_lib_platform_libraries}
)
