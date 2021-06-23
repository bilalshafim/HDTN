#ifndef _LTP_UDP_ENGINE_H
#define _LTP_UDP_ENGINE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "LtpEngine.h"

class LtpUdpEngine : public LtpEngine {
private:
    LtpUdpEngine();
public:
    typedef boost::function<bool(const uint8_t ltpHeaderByte)> UdpDropSimulatorFunction_t;
    
    LtpUdpEngine(const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, 
        const uint16_t udpPort = 0, const unsigned int numUdpRxCircularBufferVectors = 100, const unsigned int maxUdpRxPacketSizeBytes = UINT16_MAX,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION = 0, uint32_t checkpointEveryNthDataPacketSender = 0, uint32_t maxRetriesPerSerialNumber = 5);

    virtual ~LtpUdpEngine();

    virtual void Reset();
    void Stop();
    void DoUdpShutdown();
    void Connect(const std::string & hostname, const std::string & port);
    
    bool ReadyToForward();
private:
    void StartUdpReceive();
    void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void HandleUdpReceiveDiscard(const boost::system::error_code & error, std::size_t bytesTransferred);
    void SessionOriginatorEngineIdDecodedCallbackFromThisUdpEndpoint(const boost::asio::ip::udp::endpoint * const udpEndpointPtr, const uint64_t sessionOriginatorEngineId);
    virtual void PacketInFullyProcessedCallback(bool success);
    virtual void SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId);
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results);
    void HandleUdpSend(boost::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallback, const boost::system::error_code& error, std::size_t bytes_transferred);


    const uint16_t M_MY_BOUND_UDP_PORT;
    boost::asio::io_service m_ioServiceUdp;
    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::ip::udp::endpoint m_udpDestinationResolvedEndpointDataSourceToDataSink;
    std::unique_ptr<boost::thread> m_ioServiceUdpThreadPtr;

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const unsigned int M_MAX_UDP_PACKET_SIZE_BYTES;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;
    std::vector<boost::asio::ip::udp::endpoint> m_remoteEndpointsCbVec;
    std::vector<Ltp::SessionOriginatorEngineIdDecodedCallback_t> m_sessionOriginatorEngineIdDecodedCallbackCbVec;
    std::vector<boost::uint8_t> m_udpReceiveDiscardBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpointDiscard;
    std::map<uint64_t, boost::asio::ip::udp::endpoint> m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly;

    volatile bool m_readyToForward;
    bool m_printedCbTooSmallNotice;
public:
    volatile uint64_t m_countAsyncSendCalls;
    volatile uint64_t m_countAsyncSendCallbackCalls;

    //unit testing drop packet simulation stuff
    UdpDropSimulatorFunction_t m_udpDropSimulatorFunction;
   // boost::asio::ip::udp::endpoint m_udpDestinationNullEndpoint;
};



#endif //_LTP_UDP_ENGINE_H
