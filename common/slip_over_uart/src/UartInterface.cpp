/**
 * @file UartInterface.cpp
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

#include "UartInterface.h"
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>
#include <boost/bind/bind.hpp>
#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;


bool UartInterface::IsRunningNormally() {
    return m_runningNormally;
}



UartInterface::UartInterface(const std::string& comPortName, const unsigned int baudRate,
    const unsigned int numRxCircularBufferVectors,
    const std::size_t maxRxBundleSizeBytes,
    const unsigned int maxTxBundlesInFlight,
    const WholeBundleReadyCallback_t& wholeBundleReadyCallback) :
    m_useLocalConditionVariableAckReceived(false), //for destructor only
    m_running(false),
    m_runningNormally(true),
    m_rxBundleOverran(false),
    m_stateSerialReadActive(false),
    m_writeInProgress(false),
    m_sendErrorOccurred(false),
    M_NUM_RX_CIRCULAR_BUFFER_VECTORS(numRxCircularBufferVectors),
    m_comPortName(comPortName),
    m_maxRxBundleSizeBytes(maxRxBundleSizeBytes),
    m_serialPort(m_ioService),
    m_currentRxBundlePtr(NULL),
    m_circularIndexBuffer(M_NUM_RX_CIRCULAR_BUFFER_VECTORS),
    m_bundleRxBuffersCbVec(M_NUM_RX_CIRCULAR_BUFFER_VECTORS),
    MAX_TX_BUNDLES_IN_FLIGHT(maxTxBundlesInFlight),
    m_txBundlesCb(MAX_TX_BUNDLES_IN_FLIGHT + 1), //+1 ensures the CommitRead() can happen after the Notify to sender
    m_txBundlesCbVec(MAX_TX_BUNDLES_IN_FLIGHT + 1),
    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_userAssignedUuid(0),
    //telem
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesAcked(0),
    m_totalBundleBytesAcked(0),
    m_totalBundlesFailedToSend(0),
    m_totalSlipBytesSent(0),
    m_totalSlipBytesReceived(0),
    m_totalReceivedChunks(0),
    m_largestReceivedBytesPerChunk(0)
{
    m_readSomeBuffer.resize(1000);

    for (std::size_t i = 0; i < m_bundleRxBuffersCbVec.size(); ++i) {
        padded_vector_uint8_t& rxBundle = m_bundleRxBuffersCbVec[i];
        rxBundle.resize(0);
        rxBundle.reserve(m_maxRxBundleSizeBytes);
    }
    for (std::size_t i = 0; i < m_txBundlesCbVec.size(); ++i) {
        m_txBundlesCbVec[i].m_slipEncodedBundle.reserve((m_maxRxBundleSizeBytes * 2) + 4); //slip worst case, +4 for 2 slip_ends at both beginning and end
    }

    //Reset all rx states
    ResetRxStates();


    try {
        LOG_INFO(subprocess) << "Opening com port on " << m_comPortName;
        m_serialPort.open(m_comPortName);
        LOG_INFO(subprocess) << "Successfully opened serial port on " << m_comPortName;
    }
    catch (boost::system::system_error & ex) {
        LOG_FATAL(subprocess) << "Error opening serial port " << m_comPortName << ": Error=" << ex.what();
        m_runningNormally = false;
        return;
    }

    if (!m_serialPort.is_open()) {
        LOG_FATAL(subprocess) << "Failed to open serial port " << m_comPortName;
        m_runningNormally = false;
        return;
    }

    try {
        m_serialPort.set_option(
            boost::asio::serial_port_base::baud_rate(baudRate)); // set the baud rate after the port has been opened 
        LOG_INFO(subprocess) << "Successfully set baud rate to " << baudRate;

        m_serialPort.set_option(
            boost::asio::serial_port_base::character_size(8U));
        LOG_INFO(subprocess) << "Successfully set character size to " << 8U;

        m_serialPort.set_option(
            boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));
        LOG_INFO(subprocess) << "Successfully set flow control to " << "none";

        m_serialPort.set_option(
            boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        LOG_INFO(subprocess) << "Successfully set parity to " << "none";

        m_serialPort.set_option(
            boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        LOG_INFO(subprocess) << "Successfully set stop bits to " << "one";
    }
    catch (boost::system::system_error & ex) {
        LOG_FATAL(subprocess) << "Error configuring serial port: Error=" << ex.what() << "  code=" << ex.code();
        m_runningNormally = false;
        return;
    }
    TryStartSerialReceive();
    try {
        m_inductTelemetry.m_connectionName = boost::lexical_cast<std::string>(baudRate) + " baud";
    }
    catch (const boost::bad_lexical_cast&) {
        m_inductTelemetry.m_connectionName = "?? baud";
    }
    m_inductTelemetry.m_inputName = comPortName;
    LOG_INFO(subprocess) << "UART RX using CB size: " << M_NUM_RX_CIRCULAR_BUFFER_VECTORS;
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&UartInterface::PopCbThreadFunc, this)); //create and start the worker thread  

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioService" + m_comPortName);
}

UartInterface::~UartInterface() {
    Stop();
}
void UartInterface::Stop() {
    //prevent UartInterface from exiting before all bundles sent and acked
    try {
        { //scope for destruction of lock within try block
            boost::mutex localMutex;
            boost::mutex::scoped_lock lock(localMutex);
            m_useLocalConditionVariableAckReceived = true;
            std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
            for (unsigned int attempt = 0; attempt < 20; ++attempt) {
                const std::size_t numUnacked = GetTotalBundlesUnacked();
                if (numUnacked) {
                    LOG_INFO(subprocess) << "UartInterface destructor waiting on " << numUnacked << " unacked bundles";


                    if (previousUnacked > numUnacked) {
                        previousUnacked = numUnacked;
                        attempt = 0;
                    }
                    m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
                    continue;
                }
                break;
            }
        }
    }
    catch (const boost::condition_error& e) {
        LOG_ERROR(subprocess) << "condition_error in UartInterface::Stop: " << e.what();
    }
    catch (const boost::thread_resource_error& e) {
        LOG_ERROR(subprocess) << "thread_resource_error in UartInterface::Stop: " << e.what();
    }
    catch (const boost::thread_interrupted&) {
        LOG_ERROR(subprocess) << "thread_interrupted in UartInterface::Stop";
    }
    catch (const boost::lock_error& e) {
        LOG_ERROR(subprocess) << "lock_error in UartInterface::Stop: " << e.what();
    }

    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping StcpBundleSource io_service";
        }
    }

    
    m_mutexCb.lock();
    m_running = false; //thread stopping criteria
    m_mutexCb.unlock();
    m_conditionVariableCb.notify_one();

    if (m_threadCbReaderPtr) {
        try {
            m_threadCbReaderPtr->join();
            m_threadCbReaderPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping UartInterface threadCbReader";
        }
        //print stats once
        LOG_INFO(subprocess) << "UART " << m_comPortName << " Bidirectional Interface:"
            << "\n totalBundlesReceived " << m_totalBundlesReceived
            << "\n totalBundleBytesReceived " << m_totalBundleBytesReceived
            << "\n totalBundlesSent " << m_totalBundlesSent
            << "\n totalBundlesAcked " << m_totalBundlesAcked
            << "\n totalBundleBytesSent " << m_totalBundleBytesSent
            << "\n totalBundleBytesAcked " << m_totalBundleBytesAcked
            << "\n m_totalBundlesFailedToSend " << m_totalBundlesFailedToSend
            << "\n averageReceivedBytesPerChunk " << ((m_outductTelemetry.m_totalReceivedChunks) ?
                (m_outductTelemetry.m_totalSlipBytesReceived / m_outductTelemetry.m_totalReceivedChunks) : 0)
            << "\n m_totalSlipBytesSent " << m_totalSlipBytesSent
            << "\n m_totalSlipBytesReceived " << m_totalSlipBytesReceived
            << "\n m_totalReceivedChunks " << m_totalReceivedChunks
            << "\n m_largestReceivedBytesPerChunk " << m_largestReceivedBytesPerChunk;
    }
}



void UartInterface::TryStartSerialReceive() {
    if (!m_stateSerialReadActive) {
        m_stateSerialReadActive = true;
        m_serialPort.async_read_some(
            boost::asio::buffer(m_readSomeBuffer),
            boost::bind(&UartInterface::SerialReceiveSomeHandler, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}

void UartInterface::ResetRxStates() {
    //since SLIP_ESC received, reset all states
    m_currentRxBundlePtr = NULL;
    m_rxBundleOverran = false;
    SlipDecodeInit(&m_slipDecodeState);
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
        static thread_local bool m_printedCbTooSmallNotice = false;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            LOG_WARNING(subprocess) << "UartInterface Rx: buffers full.. bundle(s) will be dropped!";
        }
    }
    else {
        m_currentRxBundlePtr = &m_bundleRxBuffersCbVec[writeIndex]; //already resized to 0 and reserved to max bundle size at constructor
        m_currentRxBundlePtr->resize(0);
    }
}

void UartInterface::SerialReceiveSomeHandler(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        uint8_t *readSomePtr = m_readSomeBuffer.data();
        m_totalSlipBytesReceived += bytesTransferred;
        ++m_totalReceivedChunks;
        if (m_largestReceivedBytesPerChunk < bytesTransferred) {
            m_largestReceivedBytesPerChunk = bytesTransferred;
        }
        while (bytesTransferred) {
            --bytesTransferred;
            const uint8_t rxVal = *readSomePtr++;
            uint8_t outChar;
            const unsigned int retVal = SlipDecodeChar_Inline(&m_slipDecodeState, rxVal, &outChar);
            if (retVal == 1) { //new outChar
                if (m_currentRxBundlePtr) {
                    if (m_currentRxBundlePtr->size() < m_maxRxBundleSizeBytes) {
                        m_currentRxBundlePtr->emplace_back(outChar);
                    }
                    else {
                        m_rxBundleOverran = true;
                    }
                }
            }
            else if (retVal == 2) { //complete (no new outchar) (SLIP_ESC received)
                if (m_currentRxBundlePtr && m_currentRxBundlePtr->size()) {
                    if (m_rxBundleOverran) {
                        static thread_local bool printedMsg = false;
                        if (!printedMsg) {
                            printedMsg = true;
                            LOG_WARNING(subprocess) << "UartInterface RX bundle exceeded size limit of "
                                << m_maxRxBundleSizeBytes << " bytes.. dropping bundle!";
                        }
                    }
                    else {
                        //send
                        m_mutexCb.lock();
                        m_circularIndexBuffer.CommitWrite(); //write complete at this point
                        m_mutexCb.unlock();
                        m_conditionVariableCb.notify_one();
                        //m_telemetry.m_totalBundleBytesReceived += bytesTransferred;
                        //m_telemetry.m_totalStcpBytesReceived += bytesTransferred;
                        //++(m_telemetry.m_totalBundlesReceived);
                    }
                }

                //since SLIP_ESC received, reset all states
                ResetRxStates();
            }
            //else if (retVal == 0) { continue; }
        }
        m_stateSerialReadActive = false; //must be false before calling TryStartSerialReceive
        TryStartSerialReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in TcpServer::SerialReceiveSomeHandler: " << error.message() << std::endl;
    }
    else {
        std::cerr << "Error operation aborted in TcpServer::SerialReceiveSomeHandler: " << error.message() << std::endl;
    }
}

void UartInterface::PopCbThreadFunc() {
    ThreadNamer::SetThisThreadName("Uart" + m_comPortName + "CbReader");

    while (true) { //keep thread alive if running or cb not empty, i.e. "while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY))"
        unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_ioService, boost::bind(&UartInterface::TryStartSerialReceive, this)); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            //try again, but with the mutex
            boost::mutex::scoped_lock lock(m_mutexCb);
            consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
            if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty again (lock mutex (above) before checking condition)
                if (!m_running.load(std::memory_order_acquire)) { //m_running is mutex protected, if it stopped running, exit the thread (lock mutex (above) before checking condition)
                    break; //thread stopping criteria (empty and not running)
                }
                m_conditionVariableCb.wait(lock); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
        }
        padded_vector_uint8_t& rxBundle = m_bundleRxBuffersCbVec[consumeIndex];
        ++m_totalBundlesReceived;
        m_totalBundleBytesReceived += rxBundle.size();
        if (m_wholeBundleReadyCallback) {
            m_wholeBundleReadyCallback(rxBundle);
        }
        else {
            LOG_ERROR(subprocess) << "Uart " << m_comPortName 
                << " received a bundle but m_wholeBundleReadyCallback was not set.. dropping bundle";
        }
        rxBundle.resize(0);
        rxBundle.reserve(m_maxRxBundleSizeBytes);

        m_circularIndexBuffer.CommitRead();
    }

    LOG_INFO(subprocess) << "StcpBundleSink Circular buffer reader thread exiting";

}

void UartInterface::TrySendBundleIfAvailable_NotThreadSafe() {
    //only do this if idle (no write in progress)
    if (!m_writeInProgress) {
        if (m_sendErrorOccurred) {
            //prevent bundle from being sent
            EmptySendQueueOnFailure();
        }
        else {
            const unsigned int consumeIndex = m_txBundlesCb.GetIndexForRead(); //store the volatile
            if (consumeIndex != CIRCULAR_INDEX_BUFFER_EMPTY) {
                SerialSendElement& el = m_txBundlesCbVec[consumeIndex];
                m_writeInProgress = true;
                boost::asio::async_write(m_serialPort,
                    boost::asio::buffer(el.m_slipEncodedBundle),
                    boost::bind(&UartInterface::HandleSerialSend, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        consumeIndex));
            }
        }
    }
}

void UartInterface::TrySendBundleIfAvailable_ThreadSafe() {
    boost::asio::post(m_ioService, boost::bind(&UartInterface::TrySendBundleIfAvailable_NotThreadSafe, this));
}

void UartInterface::HandleSerialSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int consumeIndex) {
    (void)bytes_transferred;
    m_writeInProgress = false;
    SerialSendElement& el = m_txBundlesCbVec[consumeIndex];
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(subprocess) << "UartInterface::HandleSerialSend: " << error.message();
        //empty the queue
        EmptySendQueueOnFailure();
    }
    else {
        ++m_totalBundlesAcked;
        m_totalBundleBytesAcked += el.m_underlyingDataVecBundle.size();
        m_totalBundleBytesAcked += el.m_underlyingDataZmqBundle.size();
        if (m_onSuccessfulBundleSendCallback) { //notify first
            m_onSuccessfulBundleSendCallback(el.m_userData, m_userAssignedUuid);
        }
        m_txBundlesCb.CommitRead(); //cb is sized 1 larger in case a Forward() is called between notify and CommitRead
        if (m_useLocalConditionVariableAckReceived.load(std::memory_order_acquire)) { //for destructor
            m_localConditionVariableAckReceived.notify_one();
        }
        TrySendBundleIfAvailable_NotThreadSafe();
    }
}

void UartInterface::EmptySendQueueOnFailure() {
    while (true) {
        const unsigned int consumeIndex = m_txBundlesCb.GetIndexForRead();
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) {
            break;
        }
        SerialSendElement& el = m_txBundlesCbVec[consumeIndex];
        DoFailedBundleCallback(el); //notify first
        m_txBundlesCb.CommitRead(); //cb is sized 1 larger in case a Forward() is called between notify and CommitRead
    }
}

bool UartInterface::Forward(zmq::message_t& dataZmq, std::vector<uint8_t>&& userData) {
    if (!ReadyToForward()) {
        LOG_ERROR(subprocess) << "UartInterface not ready to forward yet";
        return false;
    }


    const unsigned int writeIndex = m_txBundlesCb.GetIndexForWrite();
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "UartInterface::Forward.. too many in flight bundles";
        return false;
    }

    


    ++m_totalBundlesSent;
    m_totalBundleBytesSent += dataZmq.size();

    SerialSendElement& el = m_txBundlesCbVec[writeIndex];
    el.m_slipEncodedBundle.resize((dataZmq.size() * 2) + 4);
    const unsigned int slipEncodedSize = SlipEncode((const uint8_t*)dataZmq.data(), el.m_slipEncodedBundle.data(), (unsigned int)dataZmq.size());
    el.m_slipEncodedBundle.resize(slipEncodedSize);
    m_totalSlipBytesSent += slipEncodedSize;
    el.m_userData = std::move(userData);
    el.m_underlyingDataZmqBundle = std::move(dataZmq);
    el.m_underlyingDataVecBundle.resize(0);

    m_txBundlesCb.CommitWrite(); //pushed
    TrySendBundleIfAvailable_ThreadSafe();
    return true;
}

bool UartInterface::Forward(padded_vector_uint8_t& dataVec, std::vector<uint8_t>&& userData) {
    if (!ReadyToForward()) {
        LOG_ERROR(subprocess) << "UartInterface not ready to forward yet";
        return false;
    }


    const unsigned int writeIndex = m_txBundlesCb.GetIndexForWrite();
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "UartInterface::Forward.. too many in flight bundles";
        return false;
    }




    ++m_totalBundlesSent;
    m_totalBundleBytesSent += dataVec.size();

    SerialSendElement& el = m_txBundlesCbVec[writeIndex];
    el.m_slipEncodedBundle.resize((dataVec.size() * 2) + 4);
    const unsigned int slipEncodedSize = SlipEncode((const uint8_t*)dataVec.data(), el.m_slipEncodedBundle.data(), (unsigned int)dataVec.size());
    el.m_slipEncodedBundle.resize(slipEncodedSize);
    m_totalSlipBytesSent += slipEncodedSize;
    el.m_userData = std::move(userData);
    if (el.m_underlyingDataZmqBundle.size()) {
        el.m_underlyingDataZmqBundle.rebuild(0);
    }
    el.m_underlyingDataVecBundle = std::move(dataVec);

    m_txBundlesCb.CommitWrite(); //pushed
    TrySendBundleIfAvailable_ThreadSafe();
    return true;
}


bool UartInterface::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    padded_vector_uint8_t vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}





bool UartInterface::ReadyToForward() {
    return m_runningNormally && (!m_sendErrorOccurred);
}

void UartInterface::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void UartInterface::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void UartInterface::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_onSuccessfulBundleSendCallback = callback;
}
void UartInterface::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_onOutductLinkStatusChangedCallback = callback;
}
void UartInterface::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}

void UartInterface::DoFailedBundleCallback(SerialSendElement& el) {
    ++m_totalBundlesFailedToSend;
    if ((el.m_underlyingDataVecBundle.size()) && (m_onFailedBundleVecSendCallback)) {
        m_onFailedBundleVecSendCallback(el.m_underlyingDataVecBundle, el.m_userData, m_userAssignedUuid, false);
    }
    else if ((el.m_underlyingDataZmqBundle.size()) && (m_onFailedBundleZmqSendCallback)) {
        m_onFailedBundleZmqSendCallback(el.m_underlyingDataZmqBundle, el.m_userData, m_userAssignedUuid, false);
    }
}

void UartInterface::SyncTelemetry() {
    m_outductTelemetry.m_totalBundlesReceived = m_totalBundlesReceived;
    m_inductTelemetry.m_totalBundlesReceived = m_outductTelemetry.m_totalBundlesReceived;

    m_outductTelemetry.m_totalBundleBytesReceived = m_totalBundleBytesReceived;
    m_inductTelemetry.m_totalBundleBytesReceived = m_outductTelemetry.m_totalBundleBytesReceived;

    m_outductTelemetry.m_totalBundlesSent = m_totalBundlesSent;
    m_inductTelemetry.m_totalBundlesSent = m_outductTelemetry.m_totalBundlesSent;

    m_outductTelemetry.m_totalBundleBytesSent = m_totalBundleBytesSent;
    m_inductTelemetry.m_totalBundleBytesSent = m_outductTelemetry.m_totalBundleBytesSent;

    m_outductTelemetry.m_totalBundlesAcked = m_totalBundlesAcked;
    m_inductTelemetry.m_totalBundlesSentAndAcked = m_outductTelemetry.m_totalBundlesAcked;

    m_outductTelemetry.m_totalBundleBytesAcked = m_totalBundleBytesAcked;
    m_inductTelemetry.m_totalBundleBytesSentAndAcked = m_outductTelemetry.m_totalBundleBytesAcked;

    m_outductTelemetry.m_totalBundlesFailedToSend = m_totalBundlesFailedToSend;
    m_inductTelemetry.m_totalBundlesFailedToSend = m_outductTelemetry.m_totalBundlesFailedToSend;

    m_outductTelemetry.m_totalSlipBytesSent = m_totalSlipBytesSent;
    m_inductTelemetry.m_totalSlipBytesSent = m_outductTelemetry.m_totalSlipBytesSent;

    m_outductTelemetry.m_totalSlipBytesReceived = m_totalSlipBytesReceived;
    m_inductTelemetry.m_totalSlipBytesReceived = m_outductTelemetry.m_totalSlipBytesReceived;

    m_outductTelemetry.m_totalReceivedChunks = m_totalReceivedChunks;
    m_inductTelemetry.m_totalReceivedChunks = m_outductTelemetry.m_totalReceivedChunks;

    m_outductTelemetry.m_largestReceivedBytesPerChunk = m_largestReceivedBytesPerChunk;
    m_inductTelemetry.m_largestReceivedBytesPerChunk = m_outductTelemetry.m_largestReceivedBytesPerChunk;

    m_outductTelemetry.m_averageReceivedBytesPerChunk = (m_outductTelemetry.m_totalReceivedChunks) ?
        (m_outductTelemetry.m_totalSlipBytesReceived / m_outductTelemetry.m_totalReceivedChunks) : 0;
    m_inductTelemetry.m_averageReceivedBytesPerChunk = m_outductTelemetry.m_averageReceivedBytesPerChunk;
}

std::size_t UartInterface::GetTotalBundlesAcked() const noexcept {
    return m_totalBundlesAcked.load(std::memory_order_acquire);
}

std::size_t UartInterface::GetTotalBundlesSent() const noexcept {
    return m_totalBundlesSent.load(std::memory_order_acquire);
}

std::size_t UartInterface::GetTotalBundlesUnacked() const noexcept {
    return m_totalBundlesSent.load(std::memory_order_acquire) - m_totalBundlesAcked.load(std::memory_order_acquire);//GetTotalBundlesQueued();
}

std::size_t UartInterface::GetTotalBundleBytesAcked() const noexcept {
    return m_totalBundleBytesAcked.load(std::memory_order_acquire);
}

std::size_t UartInterface::GetTotalBundleBytesSent() const noexcept {
    return m_totalBundleBytesSent.load(std::memory_order_acquire);
}

std::size_t UartInterface::GetTotalBundleBytesUnacked() const noexcept {
    return m_totalBundleBytesSent.load(std::memory_order_acquire) - m_totalBundleBytesAcked.load(std::memory_order_acquire);//GetTotalBundleBytesQueued();
}

boost::asio::io_service& UartInterface::GetIoServiceRef() {
    return m_ioService;
}
