add_library(hdtn_util
	src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
	src/Environment.cpp
	src/JsonSerializable.cpp
	src/SignalHandler.cpp
	src/TimestampUtil.cpp
	src/FragmentSet.cpp
	src/TcpAsyncSender.cpp
	src/Sdnv.cpp
	src/CborUint.cpp
	$<$<BOOL:${NON_ARM_COMPILATION}>:src/CpuFlagDetection.cpp>  # only add CpuFlagDetection test to non-ARM builds
	#src/RateManagerAsync.cpp
	src/Uri.cpp
	src/BinaryConversions.cpp
	src/TokenRateLimiter.cpp
	src/UdpBatchSender.cpp
	src/LtpClientServiceDataToSend.cpp
	src/DirectoryScanner.cpp
	src/MemoryInFiles.cpp
	src/DeadlineTimer.cpp
	src/ThreadNamer.cpp
	src/Utf8Paths.cpp)

#Disable the syscall deprecation warning for sendmsg_x (sendmmsg equivalent)
if(APPLE)
	set_property(
		SOURCE src/UdpBatchSender.cpp
		PROPERTY COMPILE_OPTIONS
			"-Wno-deprecated-declarations"
	)
endif()

target_compile_definitions(hdtn_util PRIVATE
    INSTALL_DATA_DIR=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR})
target_compile_options(hdtn_util PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(hdtn_util)
get_target_property(target_type hdtn_util TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtn_util PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtn_util PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
target_link_libraries(hdtn_util
	PUBLIC
		log_lib
		Boost::boost #boost headers
		Boost::date_time
		Boost::filesystem
		Boost::regex
		Boost::thread #also adds Threads::Threads
		${libzmq_LIB}
		$<TARGET_NAME_IF_EXISTS:OpenSSL::SSL>
		$<TARGET_NAME_IF_EXISTS:OpenSSL::Crypto>
	PRIVATE
		
)
target_include_directories(hdtn_util
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:$<$<BOOL:${HAVE_SSE2NEON_H}>:${CMAKE_SOURCE_DIR}/third_party_include> >
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		${libzmq_INCLUDE} #todo
	PRIVATE
		
)
set(MY_PUBLIC_HEADERS
    include/BinaryConversions.h
	include/BundleCallbackFunctionDefines.h
	include/CborUint.h
	include/CcsdsEncap.h
	include/CcsdsEncapEncode.h
	include/CcsdsEncapDecode.h
	include/CircularIndexBufferSingleProducerSingleConsumerConfigurable.h
	include/CpuFlagDetection.h
	include/DeadlineTimer.h
	include/DirectoryScanner.h
	include/EncapAsyncDuplexLocalStream.h
	include/EnumAsFlagsMacro.h
    include/Environment.h
	include/ForwardListQueue.h
	include/FragmentSet.h
	include/FreeListAllocator.h
	include/JsonSerializable.h
	include/LtpClientServiceDataToSend.h
	include/MemoryInFiles.h
	include/PaddedVectorUint8.h
	#include/RateManagerAsync.h
	include/Sdnv.h
	include/SignalHandler.h
	include/TokenRateLimiter.h
	include/TcpAsyncSender.h
	include/TimestampUtil.h
	include/ThreadNamer.h
	include/UdpBatchSender.h
	include/Uri.h
	include/UserDataRecycler.h
	include/Utf8Paths.h
	include/zmq.hpp
	${CMAKE_CURRENT_BINARY_DIR}/hdtn_util_export.h
)
set_target_properties(hdtn_util PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS hdtn_util
	EXPORT hdtn_util-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
#preserve hierarchy of dir_monitor
install(DIRECTORY include/dir_monitor DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

add_hdtn_package_export(hdtn_util HDTNUtil) #exported target will have the name HDTN::HDTNUtil and not hdtn_util.  Also requires install to EXPORT hdtn_util-targets
