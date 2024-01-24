/**
 * @file UdpBatchSender.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <string>
#include "UdpBatchSender.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

#ifndef _WIN32
//https://live.boost.org/doc/libs/1_84_0/doc/html/boost_asio/reference/basic_stream_socket/native_non_blocking/overload2.html
typedef std::vector<struct mmsghdr> mmsghdr_vec_t;
template <typename Handler>
struct sendmmsg_op {

    boost::asio::ip::udp::socket& m_socketRef;
    mmsghdr_vec_t& m_mmsghdrVecRef;
    Handler m_handler;
    unsigned int m_totalPacketsTransferred;

    // Function call operator meeting WriteHandler requirements.
    // Used as the handler for the async_write_some operation.
    void operator()(boost::system::error_code ec/*, std::size_t*/) {
        // Put the underlying socket into non-blocking mode.
        if (!ec) {
            if (!m_socketRef.native_non_blocking()) {
                m_socketRef.native_non_blocking(true, ec);
            }
        }

        if (!ec) {
            for (;;) {
                // Try the system call.
                errno = 0;
                //A blocking sendmmsg() call blocks until vlen messages have been
                //sent.  A nonblocking call (WHICH THIS IS) sends as many messages as possible (up
                //to the limit specified by vlen) and returns immediately.
                const unsigned int vlen = (static_cast<unsigned int>(m_mmsghdrVecRef.size())) - m_totalPacketsTransferred;
#ifdef __APPLE__
                const int n = sendmsg(m_socketRef.native_handle(), m_mmsghdrVecRef.data() + m_totalPacketsTransferred, vlen);
#else
                const int n = sendmmsg(m_socketRef.native_handle(), m_mmsghdrVecRef.data() + m_totalPacketsTransferred, vlen, 0);
#endif
                //On success, sendmmsg() returns the number of messages sent from
                //msgvec; if this is less than vlen, the caller can retry with a
                //further sendmmsg() call to send the remaining messages.
                //On error, -1 is returned, and errno is set to indicate the error.
                ec = boost::system::error_code(n < 0 ? errno : 0,
                    boost::asio::error::get_system_category());
                m_totalPacketsTransferred += ec ? 0 : n;

                // Retry operation immediately if interrupted by signal.
                if (ec == boost::asio::error::interrupted) {
                    continue;
                }

                // Check if we need to run the operation again.
                if (ec == boost::asio::error::would_block
                    || ec == boost::asio::error::try_again)
                {
                    // We have to wait for the socket to become ready again.
                    m_socketRef.async_wait(boost::asio::ip::udp::socket::wait_write, *this);
                    return;
                }

                if (ec || (m_totalPacketsTransferred == m_mmsghdrVecRef.size())) {
                    // An error occurred, or we have finished sending all packets.
                    // Either way we must exit the loop so we can call the handler.
                    break;
                }

                // Loop around to try calling sendfile again.
            }
        }

        // Pass result back to user's handler.
        m_handler(ec, m_totalPacketsTransferred);
    }
};

template <typename Handler>
void async_sendmmsg(boost::asio::ip::udp::socket& sock, mmsghdr_vec_t& mmsghdrVec, Handler h)
{
    sendmmsg_op<Handler> op = { sock, mmsghdrVec, h, 0 };
    sock.async_wait(boost::asio::ip::udp::socket::wait_write, op);
}
#endif

UdpBatchSender::UdpBatchSender() : m_udpSocketConnectedSenderOnly(m_ioService)
#if defined(_WIN32)
,m_transmitPacketsFunctionPointer(NULL)
//overlapped objects
,m_sendOverlappedAutoReset{} //zero initialize
,m_windowsObjectHandleWaitForSend(m_ioService)
#endif
,m_sendInProgress(false)
{}

UdpBatchSender::~UdpBatchSender() {
    Stop();
}

bool UdpBatchSender::Init(const std::string& remoteHostname, const uint16_t remotePort) {

    if (m_ioServiceThreadPtr) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: already initialized";
        return false;
    }
    m_ioService.reset();

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "UdpBatchSender resolving " << remoteHostname << ":" << remotePort;
    
    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioService);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "Error resolving in UdpBatchSender::Init: " << e.what() << "  code=" << e.code();
            return false;
        }
    }
    return Init(udpDestinationEndpoint);
}
bool UdpBatchSender::Init(const boost::asio::ip::udp::endpoint& udpDestinationEndpoint) {
#ifndef _WIN32
    if (sizeof(boost::asio::const_buffer) != sizeof(struct iovec)) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: sizeof(boost::asio::const_buffer) != sizeof(struct iovec)";
        return false;
    }
#endif

    if (m_ioServiceThreadPtr) {
        LOG_ERROR(subprocess) << "UdpBatchSender::Init: already initialized";
        return false;
    }
    m_ioService.reset();

    m_udpDestinationEndpoint = udpDestinationEndpoint;
    try {
        m_udpSocketConnectedSenderOnly.open(boost::asio::ip::udp::v4());
        m_udpSocketConnectedSenderOnly.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //if udpPort is 0 then bind to random ephemeral port
    }
    catch (const boost::system::system_error& e) {
        LOG_ERROR(subprocess) << "Could not bind on random ephemeral UDP port: " << e.what();
        return false;
    }

    try {
        m_udpSocketConnectedSenderOnly.connect(m_udpDestinationEndpoint);
    }
    catch (const boost::system::system_error& e) {
        LOG_INFO(subprocess) << "Error connecting socket in UdpBatchSender::Init: " << e.what() << "  code=" << e.code();
        return false;
    }

#ifdef _WIN32
    // Query the function pointer for the TransmitPacket function
    SOCKET nativeSocketHandle = m_udpSocketConnectedSenderOnly.native_handle();
    GUID TransmitPacketsGuid = WSAID_TRANSMITPACKETS;
    DWORD bytesReturned;
    int returnValue = WSAIoctl(nativeSocketHandle,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &TransmitPacketsGuid,
        sizeof(GUID),
        &m_transmitPacketsFunctionPointer,
        sizeof(PVOID),
        &bytesReturned,
        NULL,
        NULL);

    if (returnValue == SOCKET_ERROR) {
        LOG_ERROR(subprocess) << __FILE__ << ":" << __LINE__ << " - WSAIoctl failed " << WSAGetLastError();
        return false;
    }

    // Overlapped operation used for TransmitPackets
    ZeroMemory(&m_sendOverlappedAutoReset, sizeof(m_sendOverlappedAutoReset));
    m_sendOverlappedAutoReset.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL); //auto-reset event object
    if (m_sendOverlappedAutoReset.hEvent == NULL) {
        LOG_ERROR(subprocess) << __FILE__ << ":" << __LINE__ << " - CreateEvent failed (" << GetLastError() << ")";
        return false;
    }
    //https://theboostcpplibraries.com/boost.asio-platform-specific-io-objects
    try {
        m_windowsObjectHandleWaitForSend.assign(m_sendOverlappedAutoReset.hEvent);
    }
    catch (const boost::system::system_error& e) {
        LOG_ERROR(subprocess) << __FILE__ << ":" << __LINE__ 
            << " - m_windowsObjectHandleWaitForSend.assign failed " << e.what();
        return false;
    }

    m_transmitPacketsElementVec.reserve(100);
#else
    m_transmitPacketsElementVec.reserve(50);
#endif
    // github comment https://github.com/nasa/HDTN/pull/36#issuecomment-1373562636: The m_transmitPacketsElementVec vector gets more pushes for windows
    // (one push per piece of a udp packet) than it does on linux (one push per udp packet, recommended to reserve 50 for linux and 100 for windows.
    
    
    m_workPtr = boost::make_unique<boost::asio::io_service::work>(m_ioService);
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceUdpBatchSender");

    return true;
}

void UdpBatchSender::Stop() {
    if (m_ioServiceThreadPtr) {
        DoUdpShutdown();
        while (m_udpSocketConnectedSenderOnly.is_open()) {
            try {
                boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            }
            catch (const boost::thread_resource_error&) {}
            catch (const boost::thread_interrupted&) {}
            catch (const boost::condition_error&) {}
            catch (const boost::lock_error&) {}
        }

        //This function does not block, but instead simply signals the io_service to stop
        //All invocations of its run() or run_one() member functions should return as soon as possible.
        //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
        m_workPtr.reset();
        if (!m_ioService.stopped()) {
            m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
        }
        if (m_ioServiceThreadPtr) {
            try {
                m_ioServiceThreadPtr->join();
                m_ioServiceThreadPtr.reset(); //delete it
            }
            catch (const boost::thread_resource_error&) {
                LOG_ERROR(subprocess) << "error stopping UdpBatchSender io_service";
            }
        }
    }

}

void UdpBatchSender::DoUdpShutdown() {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::DoHandleSocketShutdown, this));
}

void UdpBatchSender::DoHandleSocketShutdown() {
    //final code to shut down udp sockets
    if (m_udpSocketConnectedSenderOnly.is_open()) {
        try {
            LOG_INFO(subprocess) << "shutting down UdpBatchSender UDP socket..";
            m_udpSocketConnectedSenderOnly.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "UdpBatchSender::DoHandleSocketShutdown: " << e.what();
        }
        try {
            LOG_INFO(subprocess) << "closing UdpBatchSender UDP socket..";
            m_udpSocketConnectedSenderOnly.close();
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "UdpBatchSender::DoHandleSocketShutdown: " << e.what();
        }
    }

#if defined(_WIN32)
    //close overlapped
    //CloseHandle(m_sendOverlappedAutoReset.hEvent); //hEventWaitForConnection
    if (m_windowsObjectHandleWaitForSend.is_open()) {
        try {
            m_windowsObjectHandleWaitForSend.close();
        }
        catch (const boost::system::system_error& e) {
            std::cerr << "UdpBatchSender::DoHandleSocketShutdown cannot close m_windowsObjectHandleWaitForSend " << e.what();
        }
    }
#endif
}

void UdpBatchSender::SetOnSentPacketsCallback(const OnSentPacketsCallback_t& callback) {
    m_onSentPacketsCallback = callback;
}

void UdpBatchSender::QueueSendPacketsOperation_ThreadSafe(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend) {
    boost::asio::post(m_ioService,
        boost::bind(&UdpBatchSender::QueueAndTryPerformSendPacketsOperation_NotThreadSafe, this,
            std::move(udpSendPacketInfoVecSharedPtr), numPacketsToSend));
}

void UdpBatchSender::AppendConstBufferVecToTransmissionElements(std::vector<boost::asio::const_buffer>& currentPacketConstBuffers) {
    const std::size_t currentPacketConstBuffersSize = currentPacketConstBuffers.size();
    if (currentPacketConstBuffersSize) {
#ifdef _WIN32
        std::size_t j = 0;
        while (true) {
            m_transmitPacketsElementVec.emplace_back();
            TRANSMIT_PACKETS_ELEMENT& tpe = m_transmitPacketsElementVec.back();
            boost::asio::const_buffer& constBuf = currentPacketConstBuffers[j];
            tpe.dwElFlags = TP_ELEMENT_MEMORY;
            tpe.pBuffer = (PVOID)constBuf.data();
            tpe.cLength = (ULONG)constBuf.size();
            ++j;
            if (j == currentPacketConstBuffersSize) {
                break;
            }
        }
        m_transmitPacketsElementVec.back().dwElFlags |= TP_ELEMENT_EOP;
#else //not _WIN32
        /*
        the following two data structures are equivalent so a cast will work
        struct iovec {
            ptr_t iov_base; // Starting address
            size_t iov_len; // Length in bytes
        };
        class const_buffer
        {
        private:
            const void* data_;
            std::size_t size_;
        };
        */
        m_transmitPacketsElementVec.emplace_back();
#ifdef __APPLE__
        struct msghdr& msgHeader = m_transmitPacketsElementVec.back();
#else
        struct mmsghdr& msgHeader = m_transmitPacketsElementVec.back();
#endif
        /*
        struct msghdr {
            void    *   msg_name;   // Socket name
            int     msg_namelen;    // Length of name
            struct iovec* msg_iov;    // Data blocks
            __kernel_size_t msg_iovlen; // Number of blocks
            void* msg_control;    // Per protocol magic (eg BSD file descriptor passing)
            __kernel_size_t msg_controllen; // Length of cmsg list
            unsigned int    msg_flags;
        };

        // For recvmmsg/sendmmsg
        struct mmsghdr {
            struct msghdr msg_hdr;
            unsigned int  msg_len; //The msg_len field is used to return the number of
                                   //bytes sent from the message in msg_hdr (i.e., the same as the
                                   //return value from a single sendmsg(2) call).
        };
        */
#ifdef __APPLE__
        memset(&msgHeader, 0, sizeof(struct msghdr));
        msgHeader.msg_iov = reinterpret_cast<struct iovec*>(currentPacketConstBuffers.data());
        msgHeader.msg_iovlen = currentPacketConstBuffersSize;
#else
        memset(&msgHeader, 0, sizeof(struct mmsghdr));
        msgHeader.msg_hdr.msg_iov = reinterpret_cast<struct iovec*>(currentPacketConstBuffers.data());
        msgHeader.msg_hdr.msg_iovlen = currentPacketConstBuffersSize;
#endif     
#endif //#ifdef _WIN32
    }
    else {}
}


void UdpBatchSender::QueueAndTryPerformSendPacketsOperation_NotThreadSafe(std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend) {
    m_udpSendPacketInfoQueue.emplace(std::move(udpSendPacketInfoVecSharedPtr), numPacketsToSend);
    TrySendQueued();

}
void UdpBatchSender::TrySendQueued() {
    if (!m_udpSendPacketInfoQueue.empty()) {
        if (!m_sendInProgress.exchange(true)) {
            m_transmitPacketsElementVec.resize(0); //space has been reserved in UdpBatchSender::Init()

            std::vector<UdpSendPacketInfo>& udpSendPacketInfoVec = *m_udpSendPacketInfoQueue.front().first;
            const std::size_t numPacketsToSend = m_udpSendPacketInfoQueue.front().second;
            //for loop should not compare to udpSendPacketInfoVec.size() because that vector may be resized for preallocation,
            // and don't want to everudpSendPacketInfoVec.resize() down because that would call destructor on preallocated UdpSendPacketInfo
            for (std::size_t i = 0; i < numPacketsToSend; ++i) {
                AppendConstBufferVecToTransmissionElements(udpSendPacketInfoVec[i].constBufferVec);
            }

            bool successfulSend = true;

            if (m_transmitPacketsElementVec.size()) { //there is data to send.. however if there is nothing to send then succeed with callback function and don't call OS routine
#ifdef _WIN32
                // Send the first burst of packets
                SOCKET nativeSocketHandle = m_udpSocketConnectedSenderOnly.native_handle();
                LPTRANSMIT_PACKETS_ELEMENT lpPacketArray = m_transmitPacketsElementVec.data();
                bool result = (*m_transmitPacketsFunctionPointer)(nativeSocketHandle,
                    lpPacketArray,
                    (DWORD)m_transmitPacketsElementVec.size(),
                    0xFFFFFFFF, //TODO DETERMINE NUMBER OF F'S (7 OR 8).. Setting nSendSize to 0xFFFFFFF enables the caller to control the size and content of each send request,
                    // achieved by using the TP_ELEMENT_EOP flag in the TRANSMIT_PACKETS_ELEMENT array pointed to in the lpPacketArray parameter.
                    &m_sendOverlappedAutoReset, // NULL would be used for synchronous waiting
                    TF_USE_DEFAULT_WORKER); //TF_USE_KERNEL_APC Directs Winsock to use kernel Asynchronous Procedure Calls (APCs) instead of worker threads to process
                // long TransmitPackets requests.
                // Long TransmitPackets requests are defined as requests that require more than a single read from the file or a cache;
                // the long request definition therefore depends on the size of the file and the specified length of the send packet.

                if (!result) {
                    DWORD lastError = WSAGetLastError();
                    if (lastError != ERROR_IO_PENDING) {
                        LOG_ERROR(subprocess) << __FILE__ << ":" << __LINE__ << " - TransmitPackets failed " << GetLastError();
                        successfulSend = false;
                    }
                }

                if (successfulSend) {
                    // Wait for the send operation to be completed, which causes a completion 
                    // routine to be queued for execution. 
                    m_windowsObjectHandleWaitForSend.async_wait(boost::bind(&UdpBatchSender::OnAsyncSendCompleted,
                        this, boost::asio::placeholders::error, numPacketsToSend));
                }


#else //not #ifdef _WIN32
                async_sendmmsg(m_udpSocketConnectedSenderOnly, m_transmitPacketsElementVec, boost::bind(&UdpBatchSender::OnAsyncSendCompleted,
                    this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)); //bytes_transferred is num packets sent

#endif //#ifdef _WIN32
            }

            if (!successfulSend) { //fail
                if (m_onSentPacketsCallback) {
                    m_onSentPacketsCallback(successfulSend, m_udpSendPacketInfoQueue.front().first, numPacketsToSend);
                }
                m_udpSendPacketInfoQueue.pop();
            }
        }
    }
}
void UdpBatchSender::OnAsyncSendCompleted(const boost::system::error_code& e, const std::size_t numPacketsSent) {
    
    bool successfulSend = false;
    const std::size_t numPacketsToSend = m_udpSendPacketInfoQueue.front().second;
    if (!e) {
        // Not cancelled and no errors, take necessary action.
        if (numPacketsSent == numPacketsToSend) {
            successfulSend = true;
        }
        else {
            LOG_ERROR(subprocess) << "UdpBatchSender::OnAsyncSendCompleted: numPacketsSent("
                << numPacketsSent << ") != numPacketsToSend(" << numPacketsToSend << ")";
        }
    }
    else if (e != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "UdpBatchSender::OnAsyncSendCompleted: " << e.message();
        //sendErrorOccurred = true; //prevents sending from queue
    }

    if (m_onSentPacketsCallback) {
        m_onSentPacketsCallback(successfulSend, m_udpSendPacketInfoQueue.front().first, numPacketsSent);
    }
    m_udpSendPacketInfoQueue.pop();
    m_sendInProgress.store(false, std::memory_order_release);
    TrySendQueued();
}

void UdpBatchSender::SetEndpointAndReconnect_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::SetEndpointAndReconnect, this, remoteEndpoint));
}
void UdpBatchSender::SetEndpointAndReconnect_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort) {
    boost::asio::post(m_ioService, boost::bind(&UdpBatchSender::SetEndpointAndReconnect, this, remoteHostname, remotePort));
}
bool UdpBatchSender::SetEndpointAndReconnect(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    m_udpDestinationEndpoint = remoteEndpoint;
    try {
        m_udpSocketConnectedSenderOnly.connect(m_udpDestinationEndpoint);
    }
    catch (const boost::system::system_error& e) {
        LOG_INFO(subprocess) << "Error connecting socket in UdpBatchSender::SetEndpointAndReconnect: " << e.what() << "  code=" << e.code();
        return false;
    }
    return true;
}
bool UdpBatchSender::SetEndpointAndReconnect(const std::string& remoteHostname, const uint16_t remotePort) {
    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "UdpBatchSender resolving " << remoteHostname << ":" << remotePort;

    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioService);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_INFO(subprocess) << "Error resolving in UdpBatchSender::SetEndpointAndReconnect: " << e.what() << "  code=" << e.code();
            return false;
        }
    }
    return SetEndpointAndReconnect(udpDestinationEndpoint);
}
boost::asio::ip::udp::endpoint UdpBatchSender::GetCurrentUdpEndpoint() const {
    return m_udpDestinationEndpoint;
}
