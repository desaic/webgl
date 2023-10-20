if(NOT "OFF")
    # TODO: import dependencies
endif()
if(NOT TARGET TIFF::tiff)
    include("${CMAKE_CURRENT_LIST_DIR}/TiffTargets.cmake")
endif()
