cmake_minimum_required(VERSION 3.5.1)
project(asionet)

set(CMAKE_CXX_STANDARD 14)

# Packages to find
find_package(Boost REQUIRED COMPONENTS system regex)

# compiler flags
set(CMAKE_CXX_FLAGS -pthread)

###########################
# NetworkLib Library Target
# =========================
###########################
set(SOURCE_FILES
        include/Networking.h
        include/Socket.h
        include/Stream.h
        include/Message.h
        include/ServiceClient.h
        include/ServiceServer.h
        include/DatagramReceiver.h
        include/DatagramSender.h
        include/Time.h
        include/Timer.h
        include/Resolver.h
        include/Utils.h
        include/Closeable.h
        include/Error.h
        include/Frame.h
        include/Busyable.h)

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
# target_compile_options(NetworkingLibTest PUBLIC -fopenmp -fPIC -O0 -g3 -ggdb)