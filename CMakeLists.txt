cmake_minimum_required(VERSION 3.5.1)
project(NetworkingLib)

set(NetworkingLib_VERSION_MAJOR 0)
set(NetworkingLib_VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 14)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file(
    "${PROJECT_SOURCE_DIR}/src/NetworkingLibConfig.h.in"
    "${PROJECT_BINARY_DIR}/NetworkingLibConfig.h")

# so we can import the config header file
include_directories("${PROJECT_BINARY_DIR}")

# Packages to find
find_package(Boost REQUIRED COMPONENTS system regex)

# compiler flags
set(CMAKE_CXX_FLAGS -pthread)

###########################
# NetworkLib Library Target
# =========================
###########################
set(SOURCE_FILES
        include/NetworkingLib/Networking.h
        include/NetworkingLib/Socket.h
        include/NetworkingLib/Stream.h
        include/NetworkingLib/Message.h
        include/NetworkingLib/ServiceClient.h
        include/NetworkingLib/ServiceServer.h
        include/NetworkingLib/DatagramReceiver.h
        include/NetworkingLib/DatagramSender.h
        include/NetworkingLib/Time.h
        include/NetworkingLib/Timer.h
        include/NetworkingLib/Resolver.h
        include/NetworkingLib/Utils.h
        include/NetworkingLib/Closeable.h
        include/NetworkingLib/Error.h
        include/NetworkingLib/Frame.h
        include/NetworkingLib/Busyable.h
        include/NetworkingLib/TimedValue.h
        src/Timer.cpp
        src/Time.cpp
        src/Networking.cpp
        src/Busyable.cpp)

add_library(NetworkingLib SHARED ${SOURCE_FILES})

set(PUBLIC_HEADER_FILES
        include/NetworkingLib/Networking.h
        include/NetworkingLib/Message.h
        include/NetworkingLib/ServiceClient.h
        include/NetworkingLib/ServiceServer.h
        include/NetworkingLib/Stream.h
        include/NetworkingLib/Socket.h
        include/NetworkingLib/Closeable.h
        include/NetworkingLib/Timer.h
        include/NetworkingLib/Time.h
        include/NetworkingLib/DatagramReceiver.h
        include/NetworkingLib/DatagramSender.h
        include/NetworkingLib/Utils.h
        include/NetworkingLib/Error.h
        include/NetworkingLib/Resolver.h
        include/NetworkingLib/Busyable.h
        include/NetworkingLib/Frame.h
        include/NetworkingLib/TimedValue.h
        ${CMAKE_CURRENT_BINARY_DIR}/NetworkingLibConfig.h)

foreach(HEADER ${PUBLIC_HEADER_FILES})
    set(PUBLIC_HEADER_FILES_COMBINED "${PUBLIC_HEADER_FILES_COMBINED}\\;${HEADER}")
endforeach()

# Specify public header files
set_target_properties(NetworkingLib PROPERTIES PUBLIC_HEADER ${PUBLIC_HEADER_FILES_COMBINED})

target_link_libraries(NetworkingLib ${Boost_LIBRARIES})

set(LOCAL_INSTALL_DIR ${CMAKE_CURRENT_LIST_DIR}/../../install)

set(INSTALL_PACKAGE ON)
if(INSTALL_PACKAGE)
    #############################
    # Specify install directories
    #############################
    set(CMAKE_INSTALL_PREFIX "${LOCAL_INSTALL_DIR}")
    set(INSTALL_LIB_DIR lib/NetworkingLib)
    set(INSTALL_INCLUDE_DIR include/NetworkingLib)
    set(INSTALL_CMAKE_DIR lib/NetworkingLib/CMake)

    # Make relative paths absolute (needed later on)
    foreach(p LIB INCLUDE CMAKE)
        set(var INSTALL_${p}_DIR)
        if(NOT IS_ABSOLUTE "${${var}}")
            set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
        endif()
    endforeach()

    ####################################################
    # Create the *Config.cmake and *ConfigVersion files
    ####################################################
    # Get the relative path from the cmake install directory to the include install directory
    file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}" "${INSTALL_INCLUDE_DIR}")

    # Configure *Config.cmake.in for the install tree
    # Put it inside the cmake-files directory (which is located inside the binary directory)
    set(CONF_INCLUDE_DIRS "\${NETWORKINGLIB_CMAKE_DIR}/${REL_INCLUDE_DIR}/..")
    configure_file(NetworkingLibConfig.cmake.in
            "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/NetworkingLibConfig.cmake"
            @ONLY)

    # Configure *ConfigVersion.cmake.in for both build and install tree
    # Put it inside the binary directory
    configure_file(NetworkingLibConfigVersion.cmake.in
            "${PROJECT_BINARY_DIR}/NetworkingLibConfigVersion.cmake" @ONLY)

    ########################
    # Install NetworkingLib
    ########################
    # Install library and header files
    install(TARGETS NetworkingLib
            EXPORT NetworkingLibTargets
            LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
            ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
            RUNTIME DESTINATION "${INSTALL_LIB_DIR}" # for windows
            PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}")

    # Install the NetworkingLibConfig.cmake and NetworkingLibConfigVersion.cmake
    install(FILES
            "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/NetworkingLibConfig.cmake"
            "${PROJECT_BINARY_DIR}/NetworkingLibConfigVersion.cmake"
            DESTINATION "${INSTALL_CMAKE_DIR}")

    # Install the export set for use with the install-tree
    install(EXPORT NetworkingLibTargets
            DESTINATION "${INSTALL_CMAKE_DIR}")
endif()

########################
# Test Executable Target
# ======================
########################
set(TEST_SOURCE_FILES
        ${SOURCE_FILES}
        test/Test.h
        test/Test.cpp
        test/PlatoonMessage.h
        test/PlatoonService.h
        test/Main.cpp
        test/TestUtils.h)
add_executable(NetworkingLibTest ${TEST_SOURCE_FILES})

target_link_libraries(NetworkingLibTest ${Boost_LIBRARIES})

# For debugging
target_compile_options(NetworkingLibTest PUBLIC -fopenmp -fPIC -O0 -g3 -ggdb)