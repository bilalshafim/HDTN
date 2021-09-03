#include <string.h>
#include <iostream>
#include "BpGenAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"

#include <time.h>
#include "TimestampUtil.h"
#include "codec/bpv6.h"
#ifndef _WIN32
#include "util/tsc.h"
#endif


#define BP_GEN_LOGFILE           "bpgen.%lu.csv"

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpGenAsync::BpGenAsync() : m_running(false) {

}

BpGenAsync::~BpGenAsync() {
    Stop();
}

void BpGenAsync::Stop() {
    m_running = false;
//    boost::this_thread::sleep(boost::posix_time::seconds(1));
    if(m_bpGenThreadPtr) {
        m_bpGenThreadPtr->join();
        m_bpGenThreadPtr.reset(); //delete it
    }

    m_outductManager.StopAllOutducts();
    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        outduct->GetOutductFinalStats(m_outductFinalStats);
    }
    
}

void BpGenAsync::Start(const OutductsConfig & outductsConfig, InductsConfig_ptr & inductsConfigPtr, bool custodyTransferUseAcs, const cbhe_eid_t & myEid, uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId) {
    if (m_running) {
        std::cerr << "error: BpGenAsync::Start called while BpGenAsync is already running" << std::endl;
        return;
    }
    m_myEid = myEid;
    m_myEidUriString = Uri::GetIpnUriString(m_myEid.nodeId, m_myEid.serviceId);
    m_detectedNextCustodianSupportsCteb = false;

    if (!m_outductManager.LoadOutductsFromConfig(outductsConfig)) {
        return;
    }


    m_custodyTransferUseAcs = custodyTransferUseAcs;
    if (inductsConfigPtr) {
        m_useCustodyTransfer = true;
        m_inductManager.LoadInductsFromConfig(boost::bind(&BpGenAsync::WholeCustodySignalBundleReadyCallback, this, boost::placeholders::_1), *inductsConfigPtr);
    }
    else {
        m_useCustodyTransfer = false;
    }

    m_running = true;
    m_allOutductsReady = false;
   
    
    m_bpGenThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpGenAsync::BpGenThreadFunc, this, bundleSizeBytes, bundleRate, destFlowId)); //create and start the worker thread



}


void BpGenAsync::BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId) {

    while (m_running) {
        std::cout << "Waiting for Outduct to become ready to forward..." << std::endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        if (m_outductManager.AllReadyToForward()) {
            std::cout << "Outduct ready to forward" << std::endl;
            break;
        }
    }
    if (!m_running) {
        std::cout << "BpGen Terminated before a connection could be made" << std::endl;
        return;
    }
    m_allOutductsReady = true;

    #define BP_MSG_BUFSZ             (65536 * 100) //todo

    double sval = 0.0;
    uint64_t sValU64 = 0;
    if(bundleRate) {
        std::cout << "Generating up to " << bundleRate << " bundles / second" << std::endl;
        sval = 1000000.0 / bundleRate;   // sleep val in usec
        ////sval *= BP_MSG_NBUF;
        sValU64 = static_cast<uint64_t>(sval);
        std::cout << "Sleeping for " << sValU64 << " usec between bursts" << std::endl;
    }
    else {

        if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
            std::cout << "bundle rate of zero used.. Going as fast as possible by allowing up to " << outduct->GetOutductMaxBundlesInPipeline() << " unacked bundles" << std::endl;
        }
        else {
            std::cerr << "error: null outduct" << std::endl;
            return;
        }
    }

    std::size_t numEventsTooManyUnackedBundles = 0;
    //stats?
    //uint64_t bundle_count = 0;
    m_bundleCount = 0;
    m_numRfc5050CustodyTransfers = 0;
    m_numAcsCustodyTransfers = 0;
    m_numAcsPacketsReceived = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;


    std::vector<uint8_t> data_buffer(bundleSizeBytes);
    memset(data_buffer.data(), 0, bundleSizeBytes);
    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer.data();

    //const bool doStepSize = false; //unsafe at the moment due to data_buffer
    uint32_t bundleSizeBytesStep = 100;
    

    uint64_t lastTimeRfc5050 = 0;
    uint64_t currentTimeRfc5050 = 0;
    uint64_t seq = 0;
    uint64_t bseq = 0;
    uint64_t nextCtebCustodyId = 0;


    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    boost::asio::io_service ioService;
    boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::microseconds(sValU64));
    std::vector<uint8_t> bundleToSend;
    boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
    while (m_running) { //keep thread alive if running
        /*if (doStepSize) {
            bundleSizeBytes = bundleSizeBytesStep;
            bundleSizeBytesStep += 1;
            if (bundleSizeBytesStep > 1000000) {
                bundleSizeBytesStep = 90000;
            }
        }*/
        /*
        if (HegrTcpclEntryAsync * entryTcpcl = dynamic_cast<HegrTcpclEntryAsync*>(entryIt->second.get())) {
                    const std::size_t numAckedRemaining = entryTcpcl->GetTotalBundlesSent() - entryTcpcl->GetTotalBundlesAcked();
                    while (q.size() > numAckedRemaining) {*/
        
        if(bundleRate) {
            boost::system::error_code ec;
            deadlineTimer.wait(ec);
            if(ec) {
                std::cout << "timer error: " << ec.message() << std::endl;
                return;
            }
            deadlineTimer.expires_at(deadlineTimer.expires_at() + boost::posix_time::microseconds(sValU64));
        }
        

        
        bundleToSend.resize(bundleSizeBytes + 1000);
        

        {

            uint8_t * buffer = bundleToSend.data();
            uint8_t * const serializationBase = buffer;
            currentTimeRfc5050 = TimestampUtil::GetSecondsSinceEpochRfc5050(); //curr_time = time(0);
            
            if(currentTimeRfc5050 == lastTimeRfc5050) {
                ++seq;
            }
            else {
                /*gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
                fflush(log);
                bundle_count = 0;
                bundle_data = 0;
                raw_data = 0;
                tsc_total = 0;*/
                seq = 0;
            }
            lastTimeRfc5050 = currentTimeRfc5050;
            
            bpv6_primary_block primary;
            memset(&primary, 0, sizeof(bpv6_primary_block));
            primary.version = 6;
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) | bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            if (m_useCustodyTransfer) {
                primary.flags |= BPV6_BUNDLEFLAG_CUSTODY;
                primary.custodian_node = m_myEid.nodeId;
                primary.custodian_svc = m_myEid.serviceId;
            }
            primary.src_node = m_myEid.nodeId;
            primary.src_svc = m_myEid.serviceId;
            primary.dst_node = destFlowId;
            primary.dst_svc = 1;
            primary.creation = currentTimeRfc5050; //(uint64_t)bpv6_unix_to_5050(curr_time);
            primary.lifetime = 1000;
            primary.sequence = seq;
            ////uint64_t tsc_start = rdtsc();
            uint64_t retVal;
            retVal = cbhe_bpv6_primary_block_encode(&primary, (char *)buffer, 0, BP_MSG_BUFSZ);
            if (retVal == 0) {
                std::cout << "primary encode error\n";
                m_running = false;
                continue;
            }
            buffer += retVal;

            if (m_useCustodyTransfer) {
                if (m_custodyTransferUseAcs) {
                    const uint64_t ctebCustodyId = nextCtebCustodyId++;
                    bpv6_canonical_block returnedCanonicalBlock;
                    retVal = CustodyTransferEnhancementBlock::StaticSerializeCtebCanonicalBlock((uint8_t*)buffer, 0, //0=> not last block
                        ctebCustodyId, m_myEidUriString, returnedCanonicalBlock);

                    if (retVal == 0) {
                        std::cout << "cteb encode error\n";
                        m_running = false;
                        continue;
                    }
                    buffer += retVal;

                    m_mutexCtebSet.lock();
                    FragmentSet::InsertFragment(m_outstandingCtebCustodyIdsFragmentSet, FragmentSet::data_fragment_t(ctebCustodyId, ctebCustodyId));
                    m_mutexCtebSet.unlock();
                }
                //always prepare for rfc5050 style custody transfer if next hop doesn't support acs
                if (!m_detectedNextCustodianSupportsCteb) { //prevent map from filling up endlessly if we're using cteb
                    cbhe_bundle_uuid_nofragment_t uuid(primary);
                    m_mutexBundleUuidSet.lock();
                    const bool success = m_cbheBundleUuidSet.insert(uuid).second;
                    m_mutexBundleUuidSet.unlock();
                    if (!success) {
                        std::cerr << "error insert bundle uuid\n";
                        m_running = false;
                        continue;
                    }
                }
                
            }
            bpv6_canonical_block block;
            //memset 0 not needed because all fields set below
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = bundleSizeBytes;

            retVal = bpv6_canonical_block_encode(&block, (char *)buffer, 0, BP_MSG_BUFSZ);
            if (retVal == 0) {
                std::cout << "payload canonical encode error\n";
                m_running = false;
                continue;
            }
            buffer += retVal;


           
            hdr->seq = bseq++;
            memcpy(buffer, data_buffer.data(), bundleSizeBytes);
            buffer += bundleSizeBytes;

            const uint64_t bundleLength = buffer - serializationBase;
            
            ++m_bundleCount;
            bundle_data += bundleSizeBytes;     // payload data
            raw_data += bundleLength; // bundle overhead + payload data

            bundleToSend.resize(bundleLength);
        }

        //send message
        if (!m_outductManager.Forward_Blocking(destFlowId, bundleToSend, 3)) {
            std::cerr << "bpgen was unable to send a bundle for 3 seconds.. exiting" << std::endl;
            m_running = false;
        }
       
        if (bundleToSend.size() != 0) {
            std::cerr << "error in BpGenAsync::BpGenThreadFunc: bundleToSend was not moved in Forward" << std::endl;
            std::cerr << "bundleToSend.size() : " << bundleToSend.size() << std::endl;
        }

    }
    boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();

//    std::cout << "bundle_count: " << bundle_count << std::endl;
    std::cout << "bundle_count: " << m_bundleCount << std::endl;
    std::cout << "bundle_data (payload data): " << bundle_data << " bytes" << std::endl;
    std::cout << "raw_data (bundle overhead + payload data): " << raw_data << " bytes" << std::endl;
    if (bundleRate == 0) {
        std::cout << "numEventsTooManyUnackedBundles: " << numEventsTooManyUnackedBundles << std::endl;
    }

    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        if (outduct->GetConvergenceLayerName() == "ltp_over_udp") {
            std::cout << "Bpgen Keeping UDP open for 4 seconds to acknowledge report segments" << std::endl;
            boost::this_thread::sleep(boost::posix_time::seconds(4));
        }
        else if (m_useCustodyTransfer) {
            std::cout << "Bpgen waiting for 10 seconds to receive custody transfers" << std::endl;
            boost::this_thread::sleep(boost::posix_time::seconds(10));
        }
    }
    std::cout << "m_numRfc5050CustodyTransfers: " << m_numRfc5050CustodyTransfers << std::endl;
    std::cout << "m_numAcsCustodyTransfers: " << m_numAcsCustodyTransfers << std::endl;
    std::cout << "m_numAcsPacketsReceived: " << m_numAcsPacketsReceived << std::endl;

    boost::posix_time::time_duration diff = finishedTime - startTime;
    {
        const double rateMbps = (bundle_data * 8.0) / (diff.total_microseconds());
        printf("Sent bundle_data (payload data) at %0.4f Mbits/sec\n", rateMbps);
    }
    {
        const double rateMbps = (raw_data * 8.0) / (diff.total_microseconds());
        printf("Sent raw_data (bundle overhead + payload data) at %0.4f Mbits/sec\n", rateMbps);
    }

    std::cout << "BpGenAsync::BpGenThreadFunc thread exiting\n";
}

void BpGenAsync::WholeCustodySignalBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 Induct, must protect shared resources with mutex.  Each Induct has
    //its own processing thread that calls this callback

    BundleViewV6 bv;
    if (!bv.SwapInAndLoadBundle(wholeBundleVec)) {
        std::cerr << "malformed custody signal\n";
        return;
    }
    //check primary
    bpv6_primary_block & primary = bv.m_primaryBlockView.header;
    const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
    if ((primary.flags & requiredPrimaryFlags) != requiredPrimaryFlags) {
        std::cerr << "custody signal has wrong primary flags\n";
        return;
    }
    const cbhe_eid_t receivedFinalDestinationEid(primary.dst_node, primary.dst_svc);
    if (receivedFinalDestinationEid != m_myEid) {
        std::cerr << "custody signal has wrong destination\n";
        return;
    }

    if (bv.GetNumCanonicalBlocks() != 0) { //admin record is not canonical
        std::cerr << "error admin record has canonical block\n";
        return;
    }
    if (bv.m_applicationDataUnitStartPtr == NULL) { //admin record is not canonical
        std::cerr << "error null application data unit\n";
        return;
    }
    const uint8_t adminRecordType = (*bv.m_applicationDataUnitStartPtr >> 4);

    if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL)) {
        m_detectedNextCustodianSupportsCteb = true;
        ++m_numAcsPacketsReceived;
        //check acs
        AggregateCustodySignal acs;
        if (!acs.Deserialize(bv.m_applicationDataUnitStartPtr, bv.m_frontBuffer.size() - bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size())) {
            std::cerr << "malformed ACS\n";
            return;
        }
        if (!acs.DidCustodyTransferSucceed()) {
            std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(acs.GetReasonCode()) << "\n";
            return;
        }

        m_mutexCtebSet.lock();
        for (std::set<FragmentSet::data_fragment_t>::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
            m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
            FragmentSet::RemoveFragment(m_outstandingCtebCustodyIdsFragmentSet, *it);
        }
        m_mutexCtebSet.unlock();
    }
    else if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL)) { //rfc5050 style custody transfer
        CustodySignal cs;
        uint16_t numBytesTakenToDecode = cs.Deserialize(bv.m_applicationDataUnitStartPtr);
        if (numBytesTakenToDecode == 0) {
            std::cerr << "malformed CustodySignal\n";
            return;
        }
        if (!cs.DidCustodyTransferSucceed()) {
            std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(cs.GetReasonCode()) << "\n";
            return;
        }
        if (cs.m_isFragment) {
            std::cerr << "error custody signal with fragmentation received\n";
            return;
        }
        cbhe_bundle_uuid_nofragment_t uuid;
        if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
            std::cerr << "error custody signal with bad ipn string\n";
            return;
        }
        uuid.creationSeconds = cs.m_copyOfBundleCreationTimestampTimeSeconds;
        uuid.sequence = cs.m_copyOfBundleCreationTimestampSequenceNumber;
        m_mutexBundleUuidSet.lock();
        const bool success = (m_cbheBundleUuidSet.erase(uuid) != 0);
        m_mutexBundleUuidSet.unlock();
        if (!success) {
            std::cerr << "error rfc5050 custody signal received but bundle uuid not found\n";
            return;
        }
        ++m_numRfc5050CustodyTransfers;
    }
    else { 
        std::cerr << "error unknown admin record type\n";
        return;
    }
    
}
