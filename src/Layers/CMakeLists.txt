add_subdirectory(xrAPI)

# TODO test on Windows
if (WIN32)
    add_subdirectory(xrRenderPC_R4)
endif()

add_subdirectory(xrRenderPC_GL)
