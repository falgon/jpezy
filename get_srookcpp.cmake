include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
ExternalProject_Add(SrookCppLibraries
    GIT_REPOSITORY https://github.com/falgon/SrookCppLibraries
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
    BUILD_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/SrookCppLibraries-prefix/src/SrookCppLibraries ${EXTERNAL_INSTALL_LOCATION}/SrookCppLibraries
    LOG_DOWNLOAD ON
    LOG_INSTALL ON
)
