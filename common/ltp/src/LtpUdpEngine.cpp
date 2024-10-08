/**
 * @file LtpUdpEngine.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "LtpUdpEngine.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpUdpEngine::LtpUdpEngine(
    boost::asio::ip::udp::socket& udpSocketRef,
    const uint8_t engineIndexForEncodingIntoRandomSessionNumber,
    const boost::asio::ip::udp::endpoint& remoteEndpoint,
    const uint64_t maxUdpRxPacketSizeBytes,
    const LtpEngineConfig& ltpRxOrTxCfg) :
    LtpEngine(ltpRxOrTxCfg, engineIndexForEncodingIntoRandomSessionNumber, true),
    m_udpBatchSenderConnected(m_ioServiceLtpEngine), //use ltp engine io service thread (which will itself use the m_udpBatchSenderConnected) in order to use faster non thread safe functions
    m_udpSocketRef(udpSocketRef),
    m_remoteEndpoint(remoteEndpoint),
    M_NUM_CIRCULAR_BUFFER_VECTORS(ltpRxOrTxCfg.numUdpRxCircularBufferVectors),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_printedCbTooSmallNotice(false),
    m_printedUdpSendFailedNotice(false),
    m_resetInProgress(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countBatchSendCalls(0),
    m_countBatchSendCallbackCalls(0),
    m_countBatchUdpPacketsSent(0),
    m_countCircularBufferOverruns(0),
    m_countUdpPacketsReceived(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(maxUdpRxPacketSizeBytes);
    }

    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) { //need a dedicated connected sender socket
        m_udpBatchSenderConnected.SetOnSentPacketsCallback(
            boost::bind(&LtpUdpEngine::OnSentPacketsCallback, this,
                boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
        if (!m_udpBatchSenderConnected.Init(m_remoteEndpoint)) {
            LOG_ERROR(subprocess) << "LtpUdpEngine::LtpUdpEngine: could not init dedicated udp batch sender socket";
        }
        else {
            LOG_INFO(subprocess) << "LtpUdpEngine successfully initialized dedicated udp batch sender socket to send up to "
                << M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL << " udp packets per system call";
        }
    }
}

LtpUdpEngine::~LtpUdpEngine() {
    LOG_INFO(subprocess) << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls 
        << " m_countBatchSendCalls " << m_countBatchSendCalls
        << " m_countBatchUdpPacketsSent " << m_countBatchUdpPacketsSent
        << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns
        << " m_countUdpPacketsReceived " << m_countUdpPacketsReceived;
}

void LtpUdpEngine::Reset_ThreadSafe_Blocking() {
    boost::mutex::scoped_lock cvLock(m_resetMutex);
    m_resetInProgress = true;
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::Reset, this));
    while (m_resetInProgress) { //lock mutex (above) before checking condition
        m_resetConditionVariable.wait(cvLock);
    }
}

void LtpUdpEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countBatchSendCalls = 0;
    m_countBatchSendCallbackCalls = 0;
    m_countBatchUdpPacketsSent = 0;
    m_countCircularBufferOverruns = 0;
    m_countUdpPacketsReceived = 0;
    m_resetMutex.lock();
    m_resetInProgress = false;
    m_resetMutex.unlock();
    m_resetConditionVariable.notify_one();
}


void LtpUdpEngine::PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size) {
    //per: https://en.cppreference.com/w/cpp/atomic/memory_order
    // memory_order_relaxed is okay because this is an RMW,
    // and RMWs (with any ordering) following a release form a release sequence
    m_countUdpPacketsReceived.fetch_add(1, std::memory_order_relaxed);

    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
        m_countCircularBufferOverruns.fetch_add(1, std::memory_order_relaxed);
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            LOG_WARNING(subprocess) << "LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! Next UDP packet will be dropped!";
        }
    }
    else {
        packetIn_thenSwappedForAnotherSameSizeVector.swap(m_udpReceiveBuffersCbVec[writeIndex]);
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        PacketIn_ThreadSafe(m_udpReceiveBuffersCbVec[writeIndex].data(), size); //Post to the LtpEngine IoService so its thread will process
    }
}

void LtpUdpEngine::SendPacket(
    const std::vector<boost::asio::const_buffer>& constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback)
{
    //called by LtpEngine Thread
    m_countAsyncSendCalls.fetch_add(1, std::memory_order_relaxed);
    m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
        boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback), std::move(underlyingCsDataToDeleteOnSentCallback),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}

void LtpUdpEngine::SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend)
{
    //called by LtpEngine Thread
    m_countBatchSendCalls.fetch_add(1, std::memory_order_relaxed);
    
    //using the non-thread-safe function call because it's the same io_service and thread calling this function
    m_udpBatchSenderConnected.QueueSendPacketsOperation_CalledFromWithinIoServiceThread(std::move(udpSendPacketInfoVecSharedPtr), numPacketsToSend); //data gets stolen
    //LtpUdpEngine::OnSentPacketsCallback will be called next
}


void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    (void)success;
    //Called by LTP Engine thread
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}



void LtpUdpEngine::HandleUdpSend(std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback,
    const boost::system::error_code& error, std::size_t bytes_transferred)
{
    (void)underlyingDataToDeleteOnSentCallback;
    (void)underlyingCsDataToDeleteOnSentCallback;
    (void)bytes_transferred;
    //Called by m_ioServiceUdpRef thread
    m_countAsyncSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    if (error) {
        if (!m_printedUdpSendFailedNotice) {
            m_printedUdpSendFailedNotice = true;
            LOG_ERROR(subprocess) << "LtpUdpEngine::HandleUdpSend: " << error.message() 
                << ".. this packet (and subsequent packets) will be dropped";
        }
        //DoUdpShutdown();
    }
    else {
        if (m_printedUdpSendFailedNotice) {
            m_printedUdpSendFailedNotice = false; //trigger these prints on "rising edge"
            LOG_INFO(subprocess) << "LtpUdpEngine send packets back to normal operation";
        }
        

        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine

        //OLD BEHAVIOR (only notify LtpEngine when socket is out of data)
        //if (m_countAsyncSendCallbackCalls == m_countAsyncSendCalls) { //prevent too many sends from stacking up in ioService queue
        //    SignalReadyForSend_ThreadSafe();
        //}

        
    }
    //notify the ltp engine regardless whether or not there is an error
    //NEW BEHAVIOR (always notify LtpEngine which keeps its own internal count of pending Udp Send system calls queued)
    OnSendPacketsSystemCallCompleted_ThreadSafe(); //per system call operation, not per udp packet(s)
}

void LtpUdpEngine::OnSentPacketsCallback(bool success, std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsSent) {
    (void)udpSendPacketInfoVecSharedPtr;
    //Called by UdpBatchSender thread
    m_countBatchSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    m_countBatchUdpPacketsSent.fetch_add(numPacketsSent, std::memory_order_relaxed);
    if (!success) {
        if (!m_printedUdpSendFailedNotice) {
            m_printedUdpSendFailedNotice = true;
            LOG_ERROR(subprocess) << "LtpUdpEngine::OnSentPacketsCallback failed.. these packet(s) (and subsequent packets) will be dropped";
        }
        //DoUdpShutdown();
    }
    else {
        if (m_printedUdpSendFailedNotice) {
            m_printedUdpSendFailedNotice = false; //trigger these prints on "rising edge"
            LOG_INFO(subprocess) << "LtpUdpEngine batch sender back to normal operation";
        }

        //rate stuff handled in LtpEngine due to self-sending nature of LtpEngine

        //OLD BEHAVIOR (only notify LtpEngine when socket is out of data)
        //if (m_countBatchSendCallbackCalls == m_countBatchSendCalls) { //prevent too many sends from stacking up in UdpBatchSender queue
        //    SignalReadyForSend_ThreadSafe();
        //}

        
    }
    //notify the ltp engine regardless whether or not there is an error
    //NEW BEHAVIOR (always notify LtpEngine which keeps its own internal count of pending Udp Send system calls queued)
    OnSendPacketsSystemCallCompleted_ThreadSafe(); //per system call operation, not per udp packet(s)
}

void LtpUdpEngine::SetEndpoint_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    //m_ioServiceLtpEngine is the only running thread that uses m_remoteEndpoint
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::SetEndpoint, this, remoteEndpoint));
}
void LtpUdpEngine::SetEndpoint_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpUdpEngine::SetEndpoint, this, remoteHostname, remotePort));
}
void LtpUdpEngine::SetEndpoint(const boost::asio::ip::udp::endpoint& remoteEndpoint) {
    m_remoteEndpoint = remoteEndpoint;
    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) { //using dedicated connected sender socket
        m_udpBatchSenderConnected.SetEndpointAndReconnect_ThreadSafe(m_remoteEndpoint);
    }
}
void LtpUdpEngine::SetEndpoint(const std::string& remoteHostname, const uint16_t remotePort) {
    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "LtpUdpEngine resolving " << remoteHostname << ":" << remotePort;

    boost::asio::ip::udp::endpoint udpDestinationEndpoint;
    {
        boost::asio::ip::udp::resolver resolver(m_ioServiceLtpEngine);
        try {
            udpDestinationEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "LtpUdpEngine::SetEndpoint: " << e.what() << "  code=" << e.code();
            return;
        }
    }
    SetEndpoint(udpDestinationEndpoint);
}

bool LtpUdpEngine::ReadyToSend() const noexcept {
    return !m_printedUdpSendFailedNotice.load(std::memory_order_acquire);
}
