cmake_minimum_required(VERSION 3.5.1)
project(asionet)

set(CMAKE_CXX_STANDARD 14)

# Packages to find
find_package(Boost REQUIRED COMPONENTS system regex)

# compiler flags
set(CMAKE_CXX_FLAGS "-pthread")

###########################
# NetworkLib Library Target
# =========================
###########################
set(SOURCE_FILES
        include/Context.h
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
        include/Busyable.h
        include/Worker.h
        include/WorkerPool.h
        include/WorkSerializer.h
        include/OperationQueue.h
        include/Monitor.h
        include/Wait.h
        include/OverrideOperation.h
        src/Wait.cpp)

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

target_link_libraries(asionetTest ${Boost_LIBRARIES})

target_link_libraries(asionetTest gtest gtest_main)
target_include_directories(asionetTest PUBLIC ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# For debugging
# target_compile_options(asionetTest PUBLIC -fopenmp -fPIC -O0 -g3 -ggdb)