cmake_minimum_required(VERSION 3.5.1)
project(asionet)

set(CMAKE_CXX_STANDARD 14)

# Packages to find
find_package(Boost REQUIRED COMPONENTS system regex)
find_package(Threads)

###########################
# NetworkLib Library Target
# =========================
###########################
set(SOURCE_FILES
        include/asionet/Context.h
        include/asionet/Socket.h
        include/asionet/Stream.h
        include/asionet/Message.h
        include/asionet/ServiceClient.h
        include/asionet/ServiceServer.h
        include/asionet/DatagramReceiver.h
        include/asionet/DatagramSender.h
        include/asionet/Time.h
        include/asionet/Timer.h
        include/asionet/Resolver.h
        include/asionet/Utils.h
        include/asionet/Closeable.h
        include/asionet/Error.h
        include/asionet/Frame.h
        include/asionet/Worker.h
        include/asionet/WorkerPool.h
        include/asionet/WorkSerializer.h
        include/asionet/AsyncOperationManager.h
        include/asionet/Monitor.h
        include/asionet/Wait.h
        include/asionet/ConstBuffer.h
        src/Wait.cpp)

set(PUBLIC_HEADER_FILES
        include/asionet/Context.h
        include/asionet/Socket.h
        include/asionet/Stream.h
        include/asionet/Message.h
        include/asionet/ServiceClient.h
        include/asionet/ServiceServer.h
        include/asionet/DatagramReceiver.h
        include/asionet/DatagramSender.h
        include/asionet/Time.h
        include/asionet/Timer.h
        include/asionet/Resolver.h
        include/asionet/Utils.h
        include/asionet/Closeable.h
        include/asionet/Error.h
        include/asionet/Frame.h
        include/asionet/Worker.h
        include/asionet/WorkerPool.h
        include/asionet/WorkSerializer.h
        include/asionet/AsyncOperationManager.h
        include/asionet/Monitor.h
        include/asionet/Wait.h
        include/asionet/ConstBuffer.h)

foreach(HEADER ${PUBLIC_HEADER_FILES})
    set(PUBLIC_HEADER_FILES_COMBINED "${PUBLIC_HEADER_FILES_COMBINED}\\;${HEADER}")
endforeach()

add_library(asionet STATIC ${SOURCE_FILES})

# Specify public header files
set_target_properties(asionet PROPERTIES PUBLIC_HEADER ${PUBLIC_HEADER_FILES_COMBINED})
target_link_libraries(asionet ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

#############################
# Specify install directories
#############################
set(INSTALL_LIB_DIR lib/asionet)
set(INSTALL_INCLUDE_DIR include/asionet)
set(INSTALL_CMAKE_DIR lib/asionet/CMake)

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
set(CONF_INCLUDE_DIRS "\${ASIONET_CMAKE_DIR}/${REL_INCLUDE_DIR}/..")
configure_file(asionetConfig.cmake.in
        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/asionetConfig.cmake"
        @ONLY)

########################
# Install asionet
########################
# Install library and header files
install(TARGETS asionet
        EXPORT asionetTargets
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
        RUNTIME DESTINATION "${INSTALL_LIB_DIR}" # for windows
        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}")

# Install the asionetConfig.cmake
install(FILES "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/asionetConfig.cmake"
        DESTINATION "${INSTALL_CMAKE_DIR}")

# Install the export set for use with the install-tree
install(EXPORT asionetTargets
        DESTINATION "${INSTALL_CMAKE_DIR}")

########################
# Test Executable Target
# ======================
########################
set(TEST_SOURCE_FILES
        ${SOURCE_FILES}
        test/Test.cpp
        test/TestMessage.h
        test/TestService.h
        test/TestUtils.h)
add_executable(asionetTest ${TEST_SOURCE_FILES})

target_link_libraries(asionetTest ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} gtest gtest_main)
target_include_directories(asionetTest PUBLIC ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# For debugging
# target_compile_options(asionetTest PUBLIC -fopenmp -fPIC -O0 -g3 -ggdb)