/**
 * @file Telemetry.cpp
 * @author  Blake LaFuente
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

#include <boost/endian/conversion.hpp>

#include "TelemetryDefinitions.h"
#include "Logger.h"
#include "Uri.h"
#include <boost/make_unique.hpp>
#include <boost/foreach.hpp>
#include <vector>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//for non-throw versions of get_child which return a reference to the second parameter
static const boost::property_tree::ptree EMPTY_PTREE;


/////////////////////////////////////
//StorageTelemetry_t
/////////////////////////////////////
StorageTelemetry_t::StorageTelemetry_t() :
    m_timestampMilliseconds(0),
    //from ZmqStorageInterface
    m_totalBundlesErasedFromStorageNoCustodyTransfer(0),
    m_totalBundlesErasedFromStorageWithCustodyTransfer(0),
    m_totalBundlesErasedFromStorageBecauseExpired(0),
    m_totalBundlesRewrittenToStorageFromFailedEgressSend(0),
    m_totalBundlesSentToEgressFromStorageReadFromDisk(0),
    m_totalBundleBytesSentToEgressFromStorageReadFromDisk(0),
    m_totalBundlesSentToEgressFromStorageForwardCutThrough(0),
    m_totalBundleBytesSentToEgressFromStorageForwardCutThrough(0),
    m_numRfc5050CustodyTransfers(0),
    m_numAcsCustodyTransfers(0),
    m_numAcsPacketsReceived(0),
    //from BundleStorageCatalog
    m_numBundlesOnDisk(0),
    m_numBundleBytesOnDisk(0),
    m_totalBundleWriteOperationsToDisk(0),
    m_totalBundleByteWriteOperationsToDisk(0),
    m_totalBundleEraseOperationsFromDisk(0),
    m_totalBundleByteEraseOperationsFromDisk(0),
    //from BundleStorageManagerBase's MemoryManager
    m_usedSpaceBytes(0),
    m_freeSpaceBytes(0) {}
StorageTelemetry_t::~StorageTelemetry_t() {}
bool StorageTelemetry_t::operator==(const StorageTelemetry_t& o) const {
    return (m_timestampMilliseconds == o.m_timestampMilliseconds)
        && (m_totalBundlesErasedFromStorageNoCustodyTransfer == o.m_totalBundlesErasedFromStorageNoCustodyTransfer)
        && (m_totalBundlesErasedFromStorageWithCustodyTransfer == o.m_totalBundlesErasedFromStorageWithCustodyTransfer)
        && (m_totalBundlesErasedFromStorageBecauseExpired == o.m_totalBundlesErasedFromStorageBecauseExpired)
        && (m_totalBundlesRewrittenToStorageFromFailedEgressSend == o.m_totalBundlesRewrittenToStorageFromFailedEgressSend)
        && (m_totalBundlesSentToEgressFromStorageReadFromDisk == o.m_totalBundlesSentToEgressFromStorageReadFromDisk)
        && (m_totalBundleBytesSentToEgressFromStorageReadFromDisk == o.m_totalBundleBytesSentToEgressFromStorageReadFromDisk)
        && (m_totalBundlesSentToEgressFromStorageForwardCutThrough == o.m_totalBundlesSentToEgressFromStorageForwardCutThrough)
        && (m_totalBundleBytesSentToEgressFromStorageForwardCutThrough == o.m_totalBundleBytesSentToEgressFromStorageForwardCutThrough)
        && (m_numRfc5050CustodyTransfers == o.m_numRfc5050CustodyTransfers)
        && (m_numAcsCustodyTransfers == o.m_numAcsCustodyTransfers)
        && (m_numAcsPacketsReceived == o.m_numAcsPacketsReceived)
        && (m_numBundlesOnDisk == o.m_numBundlesOnDisk)
        && (m_numBundleBytesOnDisk == o.m_numBundleBytesOnDisk)
        && (m_totalBundleWriteOperationsToDisk == o.m_totalBundleWriteOperationsToDisk)
        && (m_totalBundleByteWriteOperationsToDisk == o.m_totalBundleByteWriteOperationsToDisk)
        && (m_totalBundleEraseOperationsFromDisk == o.m_totalBundleEraseOperationsFromDisk)
        && (m_totalBundleByteEraseOperationsFromDisk == o.m_totalBundleByteEraseOperationsFromDisk)
        && (m_usedSpaceBytes == o.m_usedSpaceBytes)
        && (m_freeSpaceBytes == o.m_freeSpaceBytes);
}
bool StorageTelemetry_t::operator!=(const StorageTelemetry_t& o) const {
    return !(*this == o);
}

bool StorageTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_timestampMilliseconds = pt.get<uint64_t>("timestampMilliseconds");
        m_totalBundlesErasedFromStorageNoCustodyTransfer = pt.get<uint64_t>("totalBundlesErasedFromStorageNoCustodyTransfer");
        m_totalBundlesErasedFromStorageWithCustodyTransfer = pt.get<uint64_t>("totalBundlesErasedFromStorageWithCustodyTransfer");
        m_totalBundlesErasedFromStorageBecauseExpired = pt.get<uint64_t>("totalBundlesErasedFromStorageBecauseExpired");
        m_totalBundlesRewrittenToStorageFromFailedEgressSend = pt.get<uint64_t>("totalBundlesRewrittenToStorageFromFailedEgressSend");
        m_totalBundlesSentToEgressFromStorageReadFromDisk = pt.get<uint64_t>("totalBundlesSentToEgressFromStorageReadFromDisk");
        m_totalBundleBytesSentToEgressFromStorageReadFromDisk = pt.get<uint64_t>("totalBundleBytesSentToEgressFromStorageReadFromDisk");
        m_totalBundlesSentToEgressFromStorageForwardCutThrough = pt.get<uint64_t>("totalBundlesSentToEgressFromStorageForwardCutThrough");
        m_totalBundleBytesSentToEgressFromStorageForwardCutThrough = pt.get<uint64_t>("totalBundleBytesSentToEgressFromStorageForwardCutThrough");
        m_numRfc5050CustodyTransfers = pt.get<uint64_t>("numRfc5050CustodyTransfers");
        m_numAcsCustodyTransfers = pt.get<uint64_t>("numAcsCustodyTransfers");
        m_numAcsPacketsReceived = pt.get<uint64_t>("numAcsPacketsReceived");
        m_numBundlesOnDisk = pt.get<uint64_t>("numBundlesOnDisk");
        m_numBundleBytesOnDisk = pt.get<uint64_t>("numBundleBytesOnDisk");
        m_totalBundleWriteOperationsToDisk = pt.get<uint64_t>("totalBundleWriteOperationsToDisk");
        m_totalBundleByteWriteOperationsToDisk = pt.get<uint64_t>("totalBundleByteWriteOperationsToDisk");
        m_totalBundleEraseOperationsFromDisk = pt.get<uint64_t>("totalBundleEraseOperationsFromDisk");
        m_totalBundleByteEraseOperationsFromDisk = pt.get<uint64_t>("totalBundleByteEraseOperationsFromDisk");
        m_usedSpaceBytes = pt.get<uint64_t>("usedSpaceBytes");
        m_freeSpaceBytes = pt.get<uint64_t>("freeSpaceBytes");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StorageTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("timestampMilliseconds", m_timestampMilliseconds);
    pt.put("totalBundlesErasedFromStorageNoCustodyTransfer", m_totalBundlesErasedFromStorageNoCustodyTransfer);
    pt.put("totalBundlesErasedFromStorageWithCustodyTransfer", m_totalBundlesErasedFromStorageWithCustodyTransfer);
    pt.put("totalBundlesErasedFromStorageBecauseExpired", m_totalBundlesErasedFromStorageBecauseExpired);
    pt.put("totalBundlesRewrittenToStorageFromFailedEgressSend", m_totalBundlesRewrittenToStorageFromFailedEgressSend);
    pt.put("totalBundlesSentToEgressFromStorageReadFromDisk", m_totalBundlesSentToEgressFromStorageReadFromDisk);
    pt.put("totalBundleBytesSentToEgressFromStorageReadFromDisk", m_totalBundleBytesSentToEgressFromStorageReadFromDisk);
    pt.put("totalBundlesSentToEgressFromStorageForwardCutThrough", m_totalBundlesSentToEgressFromStorageForwardCutThrough);
    pt.put("totalBundleBytesSentToEgressFromStorageForwardCutThrough", m_totalBundleBytesSentToEgressFromStorageForwardCutThrough);
    pt.put("numRfc5050CustodyTransfers", m_numRfc5050CustodyTransfers);
    pt.put("numAcsCustodyTransfers", m_numAcsCustodyTransfers);
    pt.put("numAcsPacketsReceived", m_numAcsPacketsReceived);
    pt.put("numBundlesOnDisk", m_numBundlesOnDisk);
    pt.put("numBundleBytesOnDisk", m_numBundleBytesOnDisk);
    pt.put("totalBundleWriteOperationsToDisk", m_totalBundleWriteOperationsToDisk);
    pt.put("totalBundleByteWriteOperationsToDisk", m_totalBundleByteWriteOperationsToDisk);
    pt.put("totalBundleEraseOperationsFromDisk", m_totalBundleEraseOperationsFromDisk);
    pt.put("totalBundleByteEraseOperationsFromDisk", m_totalBundleByteEraseOperationsFromDisk);
    pt.put("usedSpaceBytes", m_usedSpaceBytes);
    pt.put("freeSpaceBytes", m_freeSpaceBytes);
    return pt;
}


/////////////////////////////////////
//StorageExpiringBeforeThresholdTelemetry_t
/////////////////////////////////////
StorageExpiringBeforeThresholdTelemetry_t::StorageExpiringBeforeThresholdTelemetry_t() :
    priority(0),
    thresholdSecondsSinceStartOfYear2000(0) {}

StorageExpiringBeforeThresholdTelemetry_t::~StorageExpiringBeforeThresholdTelemetry_t() {}
bool StorageExpiringBeforeThresholdTelemetry_t::operator==(const StorageExpiringBeforeThresholdTelemetry_t& o) const {
    return (priority == o.priority)
        && (thresholdSecondsSinceStartOfYear2000 == o.thresholdSecondsSinceStartOfYear2000)
        && (mapNodeIdToExpiringBeforeThresholdCount == o.mapNodeIdToExpiringBeforeThresholdCount);
}
bool StorageExpiringBeforeThresholdTelemetry_t::operator!=(const StorageExpiringBeforeThresholdTelemetry_t& o) const {
    return !(*this == o);
}




bool StorageExpiringBeforeThresholdTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        priority = pt.get<uint64_t>("priority");
        thresholdSecondsSinceStartOfYear2000 = pt.get<uint64_t>("thresholdSecondsSinceStartOfYear2000");
        mapNodeIdToExpiringBeforeThresholdCount.clear();
        const boost::property_tree::ptree& nodeIdMapPt = pt.get_child("nodesExpiringBeforeThresholdCount", EMPTY_PTREE); //non-throw version
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & nodePt, nodeIdMapPt) {
            const uint64_t nodeId = nodePt.second.get<uint64_t>("nodeId");
            bundle_count_plus_bundle_bytes_pair_t& p = mapNodeIdToExpiringBeforeThresholdCount[nodeId];
            p.first = nodePt.second.get<uint64_t>("bundleCount");
            p.second = nodePt.second.get<uint64_t>("totalBundleBytes");
        }
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageExpiringBeforeThresholdTelemetry_t: " << e.what();
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageExpiringBeforeThresholdTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StorageExpiringBeforeThresholdTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("priority", priority);
    pt.put("thresholdSecondsSinceStartOfYear2000", thresholdSecondsSinceStartOfYear2000);
    boost::property_tree::ptree& nodeIdsExpiringBeforeThresholdCountPt = pt.put_child("nodesExpiringBeforeThresholdCount",
        mapNodeIdToExpiringBeforeThresholdCount.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = mapNodeIdToExpiringBeforeThresholdCount.cbegin();
        it != mapNodeIdToExpiringBeforeThresholdCount.cend(); ++it)
    {
        const std::pair<const uint64_t, bundle_count_plus_bundle_bytes_pair_t>& elPair = *it;
        boost::property_tree::ptree& node = (nodeIdsExpiringBeforeThresholdCountPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second;
        node.put("nodeId", elPair.first);
        node.put("bundleCount", elPair.second.first);
        node.put("totalBundleBytes", elPair.second.second);
    }
    return pt;
}


/////////////////////////////////////
//OutductCapabilityTelemetry_t
/////////////////////////////////////
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t() :
    outductArrayIndex(0),
    maxBundlesInPipeline(0),
    maxBundleSizeBytesInPipeline(0),
    nextHopNodeId(0), assumedInitiallyDown(false) {}
bool OutductCapabilityTelemetry_t::operator==(const OutductCapabilityTelemetry_t& o) const {
    return (outductArrayIndex == o.outductArrayIndex)
        && (maxBundlesInPipeline == o.maxBundlesInPipeline)
        && (maxBundleSizeBytesInPipeline == o.maxBundleSizeBytesInPipeline)
        && (nextHopNodeId == o.nextHopNodeId)
        && (assumedInitiallyDown == o.assumedInitiallyDown)
        && (finalDestinationEidList == o.finalDestinationEidList)
        && (finalDestinationNodeIdList == o.finalDestinationNodeIdList);
}
bool OutductCapabilityTelemetry_t::operator!=(const OutductCapabilityTelemetry_t& o) const {
    return !(*this == o);
}
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t(const OutductCapabilityTelemetry_t& o) :
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    assumedInitiallyDown(o.assumedInitiallyDown),
    finalDestinationEidList(o.finalDestinationEidList),
    finalDestinationNodeIdList(o.finalDestinationNodeIdList) { } //a copy constructor: X(const X&)
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o) noexcept :
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    assumedInitiallyDown(o.assumedInitiallyDown),
    finalDestinationEidList(std::move(o.finalDestinationEidList)),
    finalDestinationNodeIdList(std::move(o.finalDestinationNodeIdList)) { } //a move constructor: X(X&&)
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(const OutductCapabilityTelemetry_t& o) { //a copy assignment: operator=(const X&)
    outductArrayIndex = o.outductArrayIndex;
    maxBundlesInPipeline = o.maxBundlesInPipeline;
    maxBundleSizeBytesInPipeline = o.maxBundleSizeBytesInPipeline;
    nextHopNodeId = o.nextHopNodeId;
    assumedInitiallyDown = o.assumedInitiallyDown,
    finalDestinationEidList = o.finalDestinationEidList;
    finalDestinationNodeIdList = o.finalDestinationNodeIdList;
    return *this;
}
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(OutductCapabilityTelemetry_t&& o) noexcept { //a move assignment: operator=(X&&)
    outductArrayIndex = o.outductArrayIndex;
    maxBundlesInPipeline = o.maxBundlesInPipeline;
    maxBundleSizeBytesInPipeline = o.maxBundleSizeBytesInPipeline;
    nextHopNodeId = o.nextHopNodeId;
    assumedInitiallyDown = o.assumedInitiallyDown,
    finalDestinationEidList = std::move(o.finalDestinationEidList);
    finalDestinationNodeIdList = std::move(o.finalDestinationNodeIdList);
    return *this;
}
bool OutductCapabilityTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        outductArrayIndex = pt.get<uint64_t>("outductArrayIndex");
        maxBundlesInPipeline = pt.get<uint64_t>("maxBundlesInPipeline");
        maxBundleSizeBytesInPipeline = pt.get<uint64_t>("maxBundleSizeBytesInPipeline");
        nextHopNodeId = pt.get<uint64_t>("nextHopNodeId");
        assumedInitiallyDown = pt.get<bool>("assumedInitiallyDown");

        const boost::property_tree::ptree& finalDestinationEidsListPt = pt.get_child("finalDestinationEidsList", EMPTY_PTREE); //non-throw version
        finalDestinationEidList.clear();
        finalDestinationNodeIdList.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & finalDestinationEidValuePt, finalDestinationEidsListPt) {
            const std::string uriStr = finalDestinationEidValuePt.second.get_value<std::string>();
            uint64_t node, svc;
            bool serviceNumberIsWildCard;
            if (!Uri::ParseIpnUriString(uriStr, node, svc, &serviceNumberIsWildCard)) {
                LOG_ERROR(subprocess) << "error parsing JSON OutductCapabilityTelemetry_t: invalid final destination eid uri " << uriStr;
                return false;
            }
            if (serviceNumberIsWildCard) {
                finalDestinationNodeIdList.emplace_back(node);
            }
            else {
                finalDestinationEidList.emplace_back(node, svc);
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON OutductCapabilityTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree OutductCapabilityTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("outductArrayIndex", outductArrayIndex);
    pt.put("maxBundlesInPipeline", maxBundlesInPipeline);
    pt.put("maxBundleSizeBytesInPipeline", maxBundleSizeBytesInPipeline);
    pt.put("nextHopNodeId", nextHopNodeId);
    pt.put("assumedInitiallyDown", assumedInitiallyDown);
    boost::property_tree::ptree& eidListPt = pt.put_child("finalDestinationEidsList",
        (finalDestinationEidList.empty() && finalDestinationNodeIdList.empty()) ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<cbhe_eid_t>::const_iterator it = finalDestinationEidList.cbegin(); it != finalDestinationEidList.cend(); ++it) {
        const cbhe_eid_t& eid = *it;
        eidListPt.push_back(std::make_pair("", boost::property_tree::ptree(Uri::GetIpnUriString(eid.nodeId, eid.serviceId)))); //using "" as key creates json array
    }
    for (std::list<uint64_t>::const_iterator it = finalDestinationNodeIdList.cbegin(); it != finalDestinationNodeIdList.cend(); ++it) {
        const uint64_t nodeId = *it;
        eidListPt.push_back(std::make_pair("", boost::property_tree::ptree(Uri::GetIpnUriStringAnyServiceNumber(nodeId)))); //using "" as key creates json array
    }
    return pt;
}

/////////////////////////////////////
//AllOutductCapabilitiesTelemetry_t
/////////////////////////////////////
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t() {}
bool AllOutductCapabilitiesTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        const boost::property_tree::ptree& outductCapabilityTelemetryListPt = pt.get_child("outductCapabilityTelemetryList", EMPTY_PTREE); //non-throw version
        outductCapabilityTelemetryList.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductCapabilityTelemetryValuePt, outductCapabilityTelemetryListPt) {
            outductCapabilityTelemetryList.emplace_back();
            outductCapabilityTelemetryList.back().SetValuesFromPropertyTree(outductCapabilityTelemetryValuePt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON AllOutductCapabilitiesTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree AllOutductCapabilitiesTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    boost::property_tree::ptree& outductCapabilityTelemetryListPt = pt.put_child("outductCapabilityTelemetryList",
        outductCapabilityTelemetryList.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<OutductCapabilityTelemetry_t>::const_iterator it = outductCapabilityTelemetryList.cbegin(); it != outductCapabilityTelemetryList.cend(); ++it) {
        const OutductCapabilityTelemetry_t& oct = *it;
        outductCapabilityTelemetryListPt.push_back(std::make_pair("", oct.GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}
bool AllOutductCapabilitiesTelemetry_t::operator==(const AllOutductCapabilitiesTelemetry_t& o) const {
    return (outductCapabilityTelemetryList == o.outductCapabilityTelemetryList);
}
bool AllOutductCapabilitiesTelemetry_t::operator!=(const AllOutductCapabilitiesTelemetry_t& o) const {
    return !(*this == o);
}
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o) :
    outductCapabilityTelemetryList(o.outductCapabilityTelemetryList) { } //a copy constructor: X(const X&)
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o) noexcept :
    outductCapabilityTelemetryList(std::move(o.outductCapabilityTelemetryList)) { } //a move constructor: X(X&&)
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(const AllOutductCapabilitiesTelemetry_t& o) { //a copy assignment: operator=(const X&)
    outductCapabilityTelemetryList = o.outductCapabilityTelemetryList;
    return *this;
}
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(AllOutductCapabilitiesTelemetry_t&& o) noexcept { //a move assignment: operator=(X&&)
    outductCapabilityTelemetryList = std::move(o.outductCapabilityTelemetryList);
    return *this;
}













/////////////////////////////////////
//InductConnectionTelemetry_t
/////////////////////////////////////

InductConnectionTelemetry_t::InductConnectionTelemetry_t() : m_totalBundlesReceived(0), m_totalBundleBytesReceived(0) {}
InductConnectionTelemetry_t::~InductConnectionTelemetry_t() {}
bool InductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    return (m_connectionName == o.m_connectionName)
        && (m_totalBundlesReceived == o.m_totalBundlesReceived)
        && (m_totalBundleBytesReceived == o.m_totalBundleBytesReceived);
}
bool InductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool InductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_connectionName = pt.get<std::string>("connectionName");
        m_inputName = pt.get<std::string>("inputName");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree InductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("connectionName", m_connectionName);
    pt.put("inputName", m_inputName);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}

StcpInductConnectionTelemetry_t::StcpInductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_totalStcpBytesReceived(0) {}
StcpInductConnectionTelemetry_t::~StcpInductConnectionTelemetry_t() {}
bool StcpInductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const StcpInductConnectionTelemetry_t* oPtr = dynamic_cast<const StcpInductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_totalStcpBytesReceived == oPtr->m_totalStcpBytesReceived);
    }
    return false;
}
bool StcpInductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool StcpInductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalStcpBytesReceived = pt.get<uint64_t>("totalStcpBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpInductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree StcpInductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("totalStcpBytesReceived", m_totalStcpBytesReceived);
    return pt;
}


UdpInductConnectionTelemetry_t::UdpInductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_countCircularBufferOverruns(0) {}
UdpInductConnectionTelemetry_t::~UdpInductConnectionTelemetry_t() {}
bool UdpInductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const UdpInductConnectionTelemetry_t* oPtr = dynamic_cast<const UdpInductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_countCircularBufferOverruns == oPtr->m_countCircularBufferOverruns);
    }
    return false;
}
bool UdpInductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool UdpInductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_countCircularBufferOverruns = pt.get<uint64_t>("countCircularBufferOverruns");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON UdpInductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree UdpInductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("countCircularBufferOverruns", m_countCircularBufferOverruns);
    return pt;
}


TcpclV3InductConnectionTelemetry_t::TcpclV3InductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_totalIncomingFragmentsAcked(0),
    m_totalOutgoingFragmentsSent(0),
    m_totalBundlesSentAndAcked(0),
    m_totalBundleBytesSentAndAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0) {}
TcpclV3InductConnectionTelemetry_t::~TcpclV3InductConnectionTelemetry_t() {}
bool TcpclV3InductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const TcpclV3InductConnectionTelemetry_t* oPtr = dynamic_cast<const TcpclV3InductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_totalIncomingFragmentsAcked == oPtr->m_totalIncomingFragmentsAcked)
            && (m_totalOutgoingFragmentsSent == oPtr->m_totalOutgoingFragmentsSent)
            && (m_totalBundlesSentAndAcked == oPtr->m_totalBundlesSentAndAcked)
            && (m_totalBundleBytesSentAndAcked == oPtr->m_totalBundleBytesSentAndAcked)
            && (m_totalBundlesSent == oPtr->m_totalBundlesSent)
            && (m_totalBundleBytesSent == oPtr->m_totalBundleBytesSent)
            && (m_totalBundlesFailedToSend == oPtr->m_totalBundlesFailedToSend);
    }
    return false;
}
bool TcpclV3InductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool TcpclV3InductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalIncomingFragmentsAcked = pt.get<uint64_t>("totalIncomingFragmentsAcked");
        m_totalOutgoingFragmentsSent = pt.get<uint64_t>("totalOutgoingFragmentsSent");
        m_totalBundlesSentAndAcked = pt.get<uint64_t>("totalBundlesSentAndAcked");
        m_totalBundleBytesSentAndAcked = pt.get<uint64_t>("totalBundleBytesSentAndAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV3InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV3InductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("totalIncomingFragmentsAcked", m_totalIncomingFragmentsAcked);
    pt.put("totalOutgoingFragmentsSent", m_totalOutgoingFragmentsSent);
    pt.put("totalBundlesSentAndAcked", m_totalBundlesSentAndAcked);
    pt.put("totalBundleBytesSentAndAcked", m_totalBundleBytesSentAndAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    return pt;
}


TcpclV4InductConnectionTelemetry_t::TcpclV4InductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_totalIncomingFragmentsAcked(0),
    m_totalOutgoingFragmentsSent(0),
    m_totalBundlesSentAndAcked(0),
    m_totalBundleBytesSentAndAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0) {}
TcpclV4InductConnectionTelemetry_t::~TcpclV4InductConnectionTelemetry_t() {}
bool TcpclV4InductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const TcpclV4InductConnectionTelemetry_t* oPtr = dynamic_cast<const TcpclV4InductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_totalIncomingFragmentsAcked == oPtr->m_totalIncomingFragmentsAcked)
            && (m_totalOutgoingFragmentsSent == oPtr->m_totalOutgoingFragmentsSent)
            && (m_totalBundlesSentAndAcked == oPtr->m_totalBundlesSentAndAcked)
            && (m_totalBundleBytesSentAndAcked == oPtr->m_totalBundleBytesSentAndAcked)
            && (m_totalBundlesSent == oPtr->m_totalBundlesSent)
            && (m_totalBundleBytesSent == oPtr->m_totalBundleBytesSent)
            && (m_totalBundlesFailedToSend == oPtr->m_totalBundlesFailedToSend);
    }
    return false;
}
bool TcpclV4InductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool TcpclV4InductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalIncomingFragmentsAcked = pt.get<uint64_t>("totalIncomingFragmentsAcked");
        m_totalOutgoingFragmentsSent = pt.get<uint64_t>("totalOutgoingFragmentsSent");
        m_totalBundlesSentAndAcked = pt.get<uint64_t>("totalBundlesSentAndAcked");
        m_totalBundleBytesSentAndAcked = pt.get<uint64_t>("totalBundleBytesSentAndAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV4InductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("totalIncomingFragmentsAcked", m_totalIncomingFragmentsAcked);
    pt.put("totalOutgoingFragmentsSent", m_totalOutgoingFragmentsSent);
    pt.put("totalBundlesSentAndAcked", m_totalBundlesSentAndAcked);
    pt.put("totalBundleBytesSentAndAcked", m_totalBundleBytesSentAndAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    return pt;
}

SlipOverUartInductConnectionTelemetry_t::SlipOverUartInductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_totalSlipBytesSent(0),
    m_totalSlipBytesReceived(0),
    m_totalReceivedChunks(0),
    m_largestReceivedBytesPerChunk(0),
    m_averageReceivedBytesPerChunk(0),
    m_totalBundlesSentAndAcked(0),
    m_totalBundleBytesSentAndAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0) {}
SlipOverUartInductConnectionTelemetry_t::~SlipOverUartInductConnectionTelemetry_t() {}
bool SlipOverUartInductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const SlipOverUartInductConnectionTelemetry_t* oPtr = dynamic_cast<const SlipOverUartInductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_totalSlipBytesSent == oPtr->m_totalSlipBytesSent)
            && (m_totalSlipBytesReceived == oPtr->m_totalSlipBytesReceived)
            && (m_totalReceivedChunks == oPtr->m_totalReceivedChunks)
            && (m_largestReceivedBytesPerChunk == oPtr->m_largestReceivedBytesPerChunk)
            && (m_averageReceivedBytesPerChunk == oPtr->m_averageReceivedBytesPerChunk)
            && (m_totalBundlesSentAndAcked == oPtr->m_totalBundlesSentAndAcked)
            && (m_totalBundleBytesSentAndAcked == oPtr->m_totalBundleBytesSentAndAcked)
            && (m_totalBundlesSent == oPtr->m_totalBundlesSent)
            && (m_totalBundleBytesSent == oPtr->m_totalBundleBytesSent)
            && (m_totalBundlesFailedToSend == oPtr->m_totalBundlesFailedToSend);
    }
    return false;
}
bool SlipOverUartInductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool SlipOverUartInductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalSlipBytesSent = pt.get<uint64_t>("totalSlipBytesSent");
        m_totalSlipBytesReceived = pt.get<uint64_t>("totalSlipBytesReceived");
        m_totalReceivedChunks = pt.get<uint64_t>("totalReceivedChunks");
        m_largestReceivedBytesPerChunk = pt.get<uint64_t>("largestReceivedBytesPerChunk");
        m_averageReceivedBytesPerChunk = pt.get<uint64_t>("averageReceivedBytesPerChunk");
        m_totalBundlesSentAndAcked = pt.get<uint64_t>("totalBundlesSentAndAcked");
        m_totalBundleBytesSentAndAcked = pt.get<uint64_t>("totalBundleBytesSentAndAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree SlipOverUartInductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("totalSlipBytesSent", m_totalSlipBytesSent);
    pt.put("totalSlipBytesReceived", m_totalSlipBytesReceived);
    pt.put("totalReceivedChunks", m_totalReceivedChunks);
    pt.put("largestReceivedBytesPerChunk", m_largestReceivedBytesPerChunk);
    pt.put("averageReceivedBytesPerChunk", m_averageReceivedBytesPerChunk);
    pt.put("totalBundlesSentAndAcked", m_totalBundlesSentAndAcked);
    pt.put("totalBundleBytesSentAndAcked", m_totalBundleBytesSentAndAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    return pt;
}

BpOverEncapLocalStreamInductConnectionTelemetry_t::BpOverEncapLocalStreamInductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_totalEncapHeaderBytesSent(0),
    m_totalEncapHeaderBytesReceived(0),
    m_largestEncapHeaderSizeBytesReceived(0),
    m_smallestEncapHeaderSizeBytesReceived(0),
    m_averageEncapHeaderSizeBytesReceived(0),
    m_totalBundlesSentAndAcked(0),
    m_totalBundleBytesSentAndAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0) {}
BpOverEncapLocalStreamInductConnectionTelemetry_t::~BpOverEncapLocalStreamInductConnectionTelemetry_t() {}
bool BpOverEncapLocalStreamInductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const BpOverEncapLocalStreamInductConnectionTelemetry_t* oPtr = dynamic_cast<const BpOverEncapLocalStreamInductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_totalEncapHeaderBytesSent == oPtr->m_totalEncapHeaderBytesSent)
            && (m_totalEncapHeaderBytesReceived == oPtr->m_totalEncapHeaderBytesReceived)
            && (m_largestEncapHeaderSizeBytesReceived == oPtr->m_largestEncapHeaderSizeBytesReceived)
            && (m_smallestEncapHeaderSizeBytesReceived == oPtr->m_smallestEncapHeaderSizeBytesReceived)
            && (m_averageEncapHeaderSizeBytesReceived == oPtr->m_averageEncapHeaderSizeBytesReceived)
            && (m_totalBundlesSentAndAcked == oPtr->m_totalBundlesSentAndAcked)
            && (m_totalBundleBytesSentAndAcked == oPtr->m_totalBundleBytesSentAndAcked)
            && (m_totalBundlesSent == oPtr->m_totalBundlesSent)
            && (m_totalBundleBytesSent == oPtr->m_totalBundleBytesSent)
            && (m_totalBundlesFailedToSend == oPtr->m_totalBundlesFailedToSend);
    }
    return false;
}
bool BpOverEncapLocalStreamInductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool BpOverEncapLocalStreamInductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalEncapHeaderBytesSent = pt.get<uint64_t>("totalEncapHeaderBytesSent");
        m_totalEncapHeaderBytesReceived = pt.get<uint64_t>("totalEncapHeaderBytesReceived");
        m_largestEncapHeaderSizeBytesReceived = pt.get<uint64_t>("largestEncapHeaderSizeBytesReceived");
        m_smallestEncapHeaderSizeBytesReceived = pt.get<uint64_t>("smallestEncapHeaderSizeBytesReceived");
        m_averageEncapHeaderSizeBytesReceived = pt.get<uint64_t>("averageEncapHeaderSizeBytesReceived");
        m_totalBundlesSentAndAcked = pt.get<uint64_t>("totalBundlesSentAndAcked");
        m_totalBundleBytesSentAndAcked = pt.get<uint64_t>("totalBundleBytesSentAndAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree BpOverEncapLocalStreamInductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("totalEncapHeaderBytesSent", m_totalEncapHeaderBytesSent);
    pt.put("totalEncapHeaderBytesReceived", m_totalEncapHeaderBytesReceived);
    pt.put("largestEncapHeaderSizeBytesReceived", m_largestEncapHeaderSizeBytesReceived);
    pt.put("smallestEncapHeaderSizeBytesReceived", m_smallestEncapHeaderSizeBytesReceived);
    pt.put("averageEncapHeaderSizeBytesReceived", m_averageEncapHeaderSizeBytesReceived);
    pt.put("totalBundlesSentAndAcked", m_totalBundlesSentAndAcked);
    pt.put("totalBundleBytesSentAndAcked", m_totalBundleBytesSentAndAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    return pt;
}

LtpInductConnectionTelemetry_t::LtpInductConnectionTelemetry_t() :
    InductConnectionTelemetry_t(),
    m_numReportSegmentTimerExpiredCallbacks(0),
    m_numReportSegmentsUnableToBeIssued(0),
    m_numReportSegmentsTooLargeAndNeedingSplit(0),
    m_numReportSegmentsCreatedViaSplit(0),
    m_numGapsFilledByOutOfOrderDataSegments(0),
    m_numDelayedFullyClaimedPrimaryReportSegmentsSent(0),
    m_numDelayedFullyClaimedSecondaryReportSegmentsSent(0),
    m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent(0),
    m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent(0),
    m_totalCancelSegmentsStarted(0),
    m_totalCancelSegmentSendRetries(0),
    m_totalCancelSegmentsFailedToSend(0),
    m_totalCancelSegmentsAcknowledged(0),
    m_numRxSessionsCancelledBySender(0),
    m_numStagnantRxSessionsDeleted(0),
    m_countUdpPacketsSent(0),
    m_countRxUdpCircularBufferOverruns(0),
    m_countTxUdpPacketsLimitedByRate(0) {}
LtpInductConnectionTelemetry_t::~LtpInductConnectionTelemetry_t() {}
bool LtpInductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    if (const LtpInductConnectionTelemetry_t* oPtr = dynamic_cast<const LtpInductConnectionTelemetry_t*>(&o)) {
        return InductConnectionTelemetry_t::operator==(o)
            && (m_numReportSegmentTimerExpiredCallbacks == oPtr->m_numReportSegmentTimerExpiredCallbacks)
            && (m_numReportSegmentsUnableToBeIssued == oPtr->m_numReportSegmentsUnableToBeIssued)
            && (m_numReportSegmentsTooLargeAndNeedingSplit == oPtr->m_numReportSegmentsTooLargeAndNeedingSplit)
            && (m_numReportSegmentsCreatedViaSplit == oPtr->m_numReportSegmentsCreatedViaSplit)
            && (m_numGapsFilledByOutOfOrderDataSegments == oPtr->m_numGapsFilledByOutOfOrderDataSegments)
            && (m_numDelayedFullyClaimedPrimaryReportSegmentsSent == oPtr->m_numDelayedFullyClaimedPrimaryReportSegmentsSent)
            && (m_numDelayedFullyClaimedSecondaryReportSegmentsSent == oPtr->m_numDelayedFullyClaimedSecondaryReportSegmentsSent)
            && (m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent == oPtr->m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent)
            && (m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent == oPtr->m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent)
            && (m_totalCancelSegmentsStarted == oPtr->m_totalCancelSegmentsStarted)
            && (m_totalCancelSegmentSendRetries == oPtr->m_totalCancelSegmentSendRetries)
            && (m_totalCancelSegmentsFailedToSend == oPtr->m_totalCancelSegmentsFailedToSend)
            && (m_totalCancelSegmentsAcknowledged == oPtr->m_totalCancelSegmentsAcknowledged)
            && (m_numRxSessionsCancelledBySender == oPtr->m_numRxSessionsCancelledBySender)
            && (m_numStagnantRxSessionsDeleted == oPtr->m_numStagnantRxSessionsDeleted)
            && (m_countUdpPacketsSent == oPtr->m_countUdpPacketsSent)
            && (m_countRxUdpCircularBufferOverruns == oPtr->m_countRxUdpCircularBufferOverruns)
            && (m_countTxUdpPacketsLimitedByRate == oPtr->m_countTxUdpPacketsLimitedByRate);
    }
    return false;
}
bool LtpInductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool LtpInductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!InductConnectionTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_numReportSegmentTimerExpiredCallbacks = pt.get<uint64_t>("numReportSegmentTimerExpiredCallbacks");
        m_numReportSegmentsUnableToBeIssued = pt.get<uint64_t>("numReportSegmentsUnableToBeIssued");
        m_numReportSegmentsTooLargeAndNeedingSplit = pt.get<uint64_t>("numReportSegmentsTooLargeAndNeedingSplit");
        m_numReportSegmentsCreatedViaSplit = pt.get<uint64_t>("numReportSegmentsCreatedViaSplit");
        m_numGapsFilledByOutOfOrderDataSegments = pt.get<uint64_t>("numGapsFilledByOutOfOrderDataSegments");
        m_numDelayedFullyClaimedPrimaryReportSegmentsSent = pt.get<uint64_t>("numDelayedFullyClaimedPrimaryReportSegmentsSent");
        m_numDelayedFullyClaimedSecondaryReportSegmentsSent = pt.get<uint64_t>("numDelayedFullyClaimedSecondaryReportSegmentsSent");
        m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent = pt.get<uint64_t>("numDelayedPartiallyClaimedPrimaryReportSegmentsSent");
        m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent = pt.get<uint64_t>("numDelayedPartiallyClaimedSecondaryReportSegmentsSent");
        m_totalCancelSegmentsStarted = pt.get<uint64_t>("totalCancelSegmentsStarted");
        m_totalCancelSegmentSendRetries = pt.get<uint64_t>("totalCancelSegmentSendRetries");
        m_totalCancelSegmentsFailedToSend = pt.get<uint64_t>("totalCancelSegmentsFailedToSend");
        m_totalCancelSegmentsAcknowledged = pt.get<uint64_t>("totalCancelSegmentsAcknowledged");
        m_numRxSessionsCancelledBySender = pt.get<uint64_t>("numRxSessionsCancelledBySender");
        m_numStagnantRxSessionsDeleted = pt.get<uint64_t>("numStagnantRxSessionsDeleted");
        m_countUdpPacketsSent = pt.get<uint64_t>("countUdpPacketsSent");
        m_countRxUdpCircularBufferOverruns = pt.get<uint64_t>("countRxUdpCircularBufferOverruns");
        m_countTxUdpPacketsLimitedByRate = pt.get<uint64_t>("countTxUdpPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON LtpInductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree LtpInductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = InductConnectionTelemetry_t::GetNewPropertyTree();
    pt.put("numReportSegmentTimerExpiredCallbacks", m_numReportSegmentTimerExpiredCallbacks);
    pt.put("numReportSegmentsUnableToBeIssued", m_numReportSegmentsUnableToBeIssued);
    pt.put("numReportSegmentsTooLargeAndNeedingSplit", m_numReportSegmentsTooLargeAndNeedingSplit);
    pt.put("numReportSegmentsCreatedViaSplit", m_numReportSegmentsCreatedViaSplit);
    pt.put("numGapsFilledByOutOfOrderDataSegments", m_numGapsFilledByOutOfOrderDataSegments);
    pt.put("numDelayedFullyClaimedPrimaryReportSegmentsSent", m_numDelayedFullyClaimedPrimaryReportSegmentsSent);
    pt.put("numDelayedFullyClaimedSecondaryReportSegmentsSent", m_numDelayedFullyClaimedSecondaryReportSegmentsSent);
    pt.put("numDelayedPartiallyClaimedPrimaryReportSegmentsSent", m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent);
    pt.put("numDelayedPartiallyClaimedSecondaryReportSegmentsSent", m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent);
    pt.put("totalCancelSegmentsStarted", m_totalCancelSegmentsStarted);
    pt.put("totalCancelSegmentSendRetries", m_totalCancelSegmentSendRetries);
    pt.put("totalCancelSegmentsFailedToSend", m_totalCancelSegmentsFailedToSend);
    pt.put("totalCancelSegmentsAcknowledged", m_totalCancelSegmentsAcknowledged);
    pt.put("numRxSessionsCancelledBySender", m_numRxSessionsCancelledBySender);
    pt.put("numStagnantRxSessionsDeleted", m_numStagnantRxSessionsDeleted);
    pt.put("countUdpPacketsSent", m_countUdpPacketsSent);
    pt.put("countRxUdpCircularBufferOverruns", m_countRxUdpCircularBufferOverruns);
    pt.put("countTxUdpPacketsLimitedByRate", m_countTxUdpPacketsLimitedByRate);
    return pt;
}


InductTelemetry_t::InductTelemetry_t() {}
static bool UniquePtrInductConnectionTelemEquivalent(const std::unique_ptr<InductConnectionTelemetry_t>& a, const std::unique_ptr<InductConnectionTelemetry_t>& b) {
    if ((!a) && (!b)) return true; //both null
    if (!a) return false;
    if (!b) return false;
    return ((*a) == (*b));
}
bool InductTelemetry_t::operator==(const InductTelemetry_t& o) const {
    return std::equal(m_listInductConnections.begin(), m_listInductConnections.end(), o.m_listInductConnections.begin(), UniquePtrInductConnectionTelemEquivalent);
    //(m_listInductConnections == o.m_listInductConnections);
}
bool InductTelemetry_t::operator!=(const InductTelemetry_t& o) const {
    return !(*this == o);
}
bool InductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_convergenceLayer = pt.get<std::string>("convergenceLayer");
        const boost::property_tree::ptree& inductConnectionsPt = pt.get_child("inductConnections", EMPTY_PTREE); //non-throw version
        m_listInductConnections.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductConnectionPt, inductConnectionsPt) {
            if ((m_convergenceLayer == "ltp_over_udp")
                || (m_convergenceLayer == "ltp_over_ipc")
                || (m_convergenceLayer == "ltp_over_encap_local_stream"))
            {
                m_listInductConnections.emplace_back(boost::make_unique<LtpInductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "bp_over_encap_local_stream") {
                m_listInductConnections.emplace_back(boost::make_unique<BpOverEncapLocalStreamInductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "slip_over_uart") {
                m_listInductConnections.emplace_back(boost::make_unique<SlipOverUartInductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "udp") {
                m_listInductConnections.emplace_back(boost::make_unique<UdpInductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "tcpcl_v3") {
                m_listInductConnections.emplace_back(boost::make_unique<TcpclV3InductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "tcpcl_v4") {
                m_listInductConnections.emplace_back(boost::make_unique<TcpclV4InductConnectionTelemetry_t>());
            }
            else if (m_convergenceLayer == "stcp") {
                m_listInductConnections.emplace_back(boost::make_unique<StcpInductConnectionTelemetry_t>());
            }
            else {
                return false;
            }
            if (!m_listInductConnections.back()->SetValuesFromPropertyTree(inductConnectionPt.second)) {
                return false;
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON InductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree InductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("convergenceLayer", m_convergenceLayer);
    boost::property_tree::ptree& inductConnectionsPt = pt.put_child("inductConnections",
        m_listInductConnections.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<std::unique_ptr<InductConnectionTelemetry_t> >::const_iterator it = m_listInductConnections.cbegin(); it != m_listInductConnections.cend(); ++it) {
        const std::unique_ptr<InductConnectionTelemetry_t>& ictPtr = *it;
        if (!ictPtr) {
            break;
        }
        inductConnectionsPt.push_back(std::make_pair("", ictPtr->GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}



AllInductTelemetry_t::AllInductTelemetry_t() : 
    m_timestampMilliseconds(0),
    m_bundleCountEgress(0),
    m_bundleCountStorage(0),
    m_bundleByteCountEgress(0),
    m_bundleByteCountStorage(0) {}
bool AllInductTelemetry_t::operator==(const AllInductTelemetry_t& o) const {
    return (m_listAllInducts == o.m_listAllInducts)
        && (m_timestampMilliseconds == o.m_timestampMilliseconds)
        && (m_bundleCountEgress == o.m_bundleCountEgress)
        && (m_bundleCountStorage == o.m_bundleCountStorage)
        && (m_bundleByteCountEgress == o.m_bundleByteCountEgress)
        && (m_bundleByteCountStorage == o.m_bundleByteCountStorage);
}
bool AllInductTelemetry_t::operator!=(const AllInductTelemetry_t& o) const {
    return !(*this == o);
}
bool AllInductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_timestampMilliseconds = pt.get<uint64_t>("timestampMilliseconds");
        m_bundleCountEgress = pt.get<uint64_t>("bundleCountEgress");
        m_bundleCountStorage = pt.get<uint64_t>("bundleCountStorage");
        m_bundleByteCountEgress = pt.get<uint64_t>("bundleByteCountEgress");
        m_bundleByteCountStorage = pt.get<uint64_t>("bundleByteCountStorage");
        const boost::property_tree::ptree& allInductsPt = pt.get_child("allInducts", EMPTY_PTREE); //non-throw version
        m_listAllInducts.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductPt, allInductsPt) {
            m_listAllInducts.emplace_back();
            m_listAllInducts.back().SetValuesFromPropertyTree(inductPt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON AllInductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree AllInductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("timestampMilliseconds", m_timestampMilliseconds);
    pt.put("bundleCountEgress", m_bundleCountEgress);
    pt.put("bundleCountStorage", m_bundleCountStorage);
    pt.put("bundleByteCountEgress", m_bundleByteCountEgress);
    pt.put("bundleByteCountStorage", m_bundleByteCountStorage);
    boost::property_tree::ptree& allInductsPt = pt.put_child("allInducts",
        m_listAllInducts.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<InductTelemetry_t>::const_iterator it = m_listAllInducts.cbegin(); it != m_listAllInducts.cend(); ++it) {
        const InductTelemetry_t& induct = *it;
        allInductsPt.push_back(std::make_pair("", induct.GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}




/////////////////////////////////////
//OutductTelemetry_t
/////////////////////////////////////
OutductTelemetry_t::OutductTelemetry_t() :
    m_totalBundlesAcked(0),
    m_totalBundleBytesAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0),
    m_linkIsUpPhysically(false),
    m_linkIsUpPerTimeSchedule(false)
{
}
OutductTelemetry_t::~OutductTelemetry_t() {}
bool OutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    return (m_convergenceLayer == o.m_convergenceLayer)
        && (m_totalBundlesAcked == o.m_totalBundlesAcked)
        && (m_totalBundleBytesAcked == o.m_totalBundleBytesAcked)
        && (m_totalBundlesSent == o.m_totalBundlesSent)
        && (m_totalBundleBytesSent == o.m_totalBundleBytesSent)
        && (m_totalBundlesFailedToSend == o.m_totalBundlesFailedToSend)
        && (m_linkIsUpPhysically == o.m_linkIsUpPhysically)
        && (m_linkIsUpPerTimeSchedule == o.m_linkIsUpPerTimeSchedule);
}
bool OutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_convergenceLayer = pt.get<std::string>("convergenceLayer");
        m_totalBundlesAcked = pt.get<uint64_t>("totalBundlesAcked");
        m_totalBundleBytesAcked = pt.get<uint64_t>("totalBundleBytesAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
        m_linkIsUpPhysically = pt.get<bool>("linkIsUpPhysically");
        m_linkIsUpPerTimeSchedule = pt.get<bool>("linkIsUpPerTimeSchedule");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("convergenceLayer", m_convergenceLayer);
    pt.put("totalBundlesAcked", m_totalBundlesAcked);
    pt.put("totalBundleBytesAcked", m_totalBundleBytesAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    pt.put("linkIsUpPhysically", m_linkIsUpPhysically);
    pt.put("linkIsUpPerTimeSchedule", m_linkIsUpPerTimeSchedule);
    return pt;
}
uint64_t OutductTelemetry_t::GetTotalBundlesQueued() const {
    return m_totalBundlesSent - m_totalBundlesAcked;
}
uint64_t OutductTelemetry_t::GetTotalBundleBytesQueued() const {
    return m_totalBundleBytesSent - m_totalBundleBytesAcked;
}

StcpOutductTelemetry_t::StcpOutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalStcpBytesSent(0),
    m_numTcpReconnectAttempts(0)
{
    m_convergenceLayer = "stcp";
}

StcpOutductTelemetry_t::~StcpOutductTelemetry_t() {}
bool StcpOutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const StcpOutductTelemetry_t* oPtr = dynamic_cast<const StcpOutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalStcpBytesSent == oPtr->m_totalStcpBytesSent)
            && (m_numTcpReconnectAttempts == oPtr->m_numTcpReconnectAttempts);
    }
    return false;
}
bool StcpOutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}

bool StcpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalStcpBytesSent = pt.get<uint64_t>("totalStcpBytesSent");
        m_numTcpReconnectAttempts = pt.get<uint64_t>("numTcpReconnectAttempts");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StcpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalStcpBytesSent", m_totalStcpBytesSent);
    pt.put("numTcpReconnectAttempts", m_numTcpReconnectAttempts);
    return pt;
}

LtpOutductTelemetry_t::LtpOutductTelemetry_t() :
    OutductTelemetry_t(),
    m_numCheckpointsExpired(0),
    m_numDiscretionaryCheckpointsNotResent(0),
    m_numDeletedFullyClaimedPendingReports(0),
    m_totalCancelSegmentsStarted(0),
    m_totalCancelSegmentSendRetries(0),
    m_totalCancelSegmentsFailedToSend(0),
    m_totalCancelSegmentsAcknowledged(0),
    m_totalPingsStarted(0),
    m_totalPingRetries(0),
    m_totalPingsFailedToSend(0),
    m_totalPingsAcknowledged(0),
    m_numTxSessionsReturnedToStorage(0),
    m_numTxSessionsCancelledByReceiver(0),
    m_countUdpPacketsSent(0),
    m_countRxUdpCircularBufferOverruns(0),
    m_countTxUdpPacketsLimitedByRate(0)
{
    m_convergenceLayer = "ltp_over_udp";
}
LtpOutductTelemetry_t::~LtpOutductTelemetry_t() {}
bool LtpOutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const LtpOutductTelemetry_t* oPtr = dynamic_cast<const LtpOutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_numCheckpointsExpired == oPtr->m_numCheckpointsExpired)
            && (m_numDiscretionaryCheckpointsNotResent == oPtr->m_numDiscretionaryCheckpointsNotResent)
            && (m_numDeletedFullyClaimedPendingReports == oPtr->m_numDeletedFullyClaimedPendingReports)
            && (m_totalCancelSegmentsStarted == oPtr->m_totalCancelSegmentsStarted)
            && (m_totalCancelSegmentSendRetries == oPtr->m_totalCancelSegmentSendRetries)
            && (m_totalCancelSegmentsFailedToSend == oPtr->m_totalCancelSegmentsFailedToSend)
            && (m_totalCancelSegmentsAcknowledged == oPtr->m_totalCancelSegmentsAcknowledged)
            && (m_totalPingsStarted == oPtr->m_totalPingsStarted)
            && (m_totalPingRetries == oPtr->m_totalPingRetries)
            && (m_totalPingsFailedToSend == oPtr->m_totalPingsFailedToSend)
            && (m_totalPingsAcknowledged == oPtr->m_totalPingsAcknowledged)
            && (m_numTxSessionsReturnedToStorage == oPtr->m_numTxSessionsReturnedToStorage)
            && (m_numTxSessionsCancelledByReceiver == oPtr->m_numTxSessionsCancelledByReceiver)
            && (m_countUdpPacketsSent == oPtr->m_countUdpPacketsSent)
            && (m_countRxUdpCircularBufferOverruns == oPtr->m_countRxUdpCircularBufferOverruns)
            && (m_countTxUdpPacketsLimitedByRate == oPtr->m_countTxUdpPacketsLimitedByRate);
    }
    return false;
}
bool LtpOutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool LtpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_numCheckpointsExpired = pt.get<uint64_t>("numCheckpointsExpired");
        m_numDiscretionaryCheckpointsNotResent = pt.get<uint64_t>("numDiscretionaryCheckpointsNotResent");
        m_numDeletedFullyClaimedPendingReports = pt.get<uint64_t>("numDeletedFullyClaimedPendingReports");
        m_totalCancelSegmentsStarted = pt.get<uint64_t>("totalCancelSegmentsStarted");
        m_totalCancelSegmentSendRetries = pt.get<uint64_t>("totalCancelSegmentSendRetries");
        m_totalCancelSegmentsFailedToSend = pt.get<uint64_t>("totalCancelSegmentsFailedToSend");
        m_totalCancelSegmentsAcknowledged = pt.get<uint64_t>("totalCancelSegmentsAcknowledged");
        m_totalPingsStarted = pt.get<uint64_t>("totalPingsStarted");
        m_totalPingRetries = pt.get<uint64_t>("totalPingRetries");
        m_totalPingsFailedToSend = pt.get<uint64_t>("totalPingsFailedToSend");
        m_totalPingsAcknowledged = pt.get<uint64_t>("totalPingsAcknowledged");
        m_numTxSessionsReturnedToStorage = pt.get<uint64_t>("numTxSessionsReturnedToStorage");
        m_numTxSessionsCancelledByReceiver = pt.get<uint64_t>("numTxSessionsCancelledByReceiver");
        m_countUdpPacketsSent = pt.get<uint64_t>("countUdpPacketsSent");
        m_countRxUdpCircularBufferOverruns = pt.get<uint64_t>("countRxUdpCircularBufferOverruns");
        m_countTxUdpPacketsLimitedByRate = pt.get<uint64_t>("countTxUdpPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON LtpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree LtpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("numCheckpointsExpired", m_numCheckpointsExpired);
    pt.put("numDiscretionaryCheckpointsNotResent", m_numDiscretionaryCheckpointsNotResent);
    pt.put("numDeletedFullyClaimedPendingReports", m_numDeletedFullyClaimedPendingReports);
    pt.put("totalCancelSegmentsStarted", m_totalCancelSegmentsStarted);
    pt.put("totalCancelSegmentSendRetries", m_totalCancelSegmentSendRetries);
    pt.put("totalCancelSegmentsFailedToSend", m_totalCancelSegmentsFailedToSend);
    pt.put("totalCancelSegmentsAcknowledged", m_totalCancelSegmentsAcknowledged);
    pt.put("totalPingsStarted", m_totalPingsStarted);
    pt.put("totalPingRetries", m_totalPingRetries);
    pt.put("totalPingsFailedToSend", m_totalPingsFailedToSend);
    pt.put("totalPingsAcknowledged", m_totalPingsAcknowledged);
    pt.put("numTxSessionsReturnedToStorage", m_numTxSessionsReturnedToStorage);
    pt.put("numTxSessionsCancelledByReceiver", m_numTxSessionsCancelledByReceiver);
    pt.put("countUdpPacketsSent", m_countUdpPacketsSent);
    pt.put("countRxUdpCircularBufferOverruns", m_countRxUdpCircularBufferOverruns);
    pt.put("countTxUdpPacketsLimitedByRate", m_countTxUdpPacketsLimitedByRate);
    return pt;
}


TcpclV3OutductTelemetry_t::TcpclV3OutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalFragmentsAcked(0),
    m_totalFragmentsSent(0),
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0),
    m_numTcpReconnectAttempts(0)
{
    m_convergenceLayer = "tcpcl_v3";
}
TcpclV3OutductTelemetry_t::~TcpclV3OutductTelemetry_t() {}
bool TcpclV3OutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const TcpclV3OutductTelemetry_t* oPtr = dynamic_cast<const TcpclV3OutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalFragmentsAcked == oPtr->m_totalFragmentsAcked)
            && (m_totalFragmentsSent == oPtr->m_totalFragmentsSent)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived)
            && (m_numTcpReconnectAttempts == oPtr->m_numTcpReconnectAttempts);
    }
    return false;
}
bool TcpclV3OutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool TcpclV3OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalFragmentsAcked = pt.get<uint64_t>("totalFragmentsAcked");
        m_totalFragmentsSent = pt.get<uint64_t>("totalFragmentsSent");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
        m_numTcpReconnectAttempts = pt.get<uint64_t>("numTcpReconnectAttempts");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV3OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV3OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalFragmentsAcked", m_totalFragmentsAcked);
    pt.put("totalFragmentsSent", m_totalFragmentsSent);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    pt.put("numTcpReconnectAttempts", m_numTcpReconnectAttempts);
    return pt;
}

TcpclV4OutductTelemetry_t::TcpclV4OutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalFragmentsAcked(0),
    m_totalFragmentsSent(0),
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0),
    m_numTcpReconnectAttempts(0)
{
    m_convergenceLayer = "tcpcl_v4";
}
TcpclV4OutductTelemetry_t::~TcpclV4OutductTelemetry_t() {}
bool TcpclV4OutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const TcpclV4OutductTelemetry_t* oPtr = dynamic_cast<const TcpclV4OutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalFragmentsAcked == oPtr->m_totalFragmentsAcked)
            && (m_totalFragmentsSent == oPtr->m_totalFragmentsSent)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived)
            && (m_numTcpReconnectAttempts == oPtr->m_numTcpReconnectAttempts);
    }
    return false;
}
bool TcpclV4OutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool TcpclV4OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalFragmentsAcked = pt.get<uint64_t>("totalFragmentsAcked");
        m_totalFragmentsSent = pt.get<uint64_t>("totalFragmentsSent");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
        m_numTcpReconnectAttempts = pt.get<uint64_t>("numTcpReconnectAttempts");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV4OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalFragmentsAcked", m_totalFragmentsAcked);
    pt.put("totalFragmentsSent", m_totalFragmentsSent);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    pt.put("numTcpReconnectAttempts", m_numTcpReconnectAttempts);
    return pt;
}

SlipOverUartOutductTelemetry_t::SlipOverUartOutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalSlipBytesSent(0),
    m_totalSlipBytesReceived(0),
    m_totalReceivedChunks(0),
    m_largestReceivedBytesPerChunk(0),
    m_averageReceivedBytesPerChunk(0),
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0)
{
    m_convergenceLayer = "slip_over_uart";
}
SlipOverUartOutductTelemetry_t::~SlipOverUartOutductTelemetry_t() {}
bool SlipOverUartOutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const SlipOverUartOutductTelemetry_t* oPtr = dynamic_cast<const SlipOverUartOutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalSlipBytesSent == oPtr->m_totalSlipBytesSent)
            && (m_totalSlipBytesReceived == oPtr->m_totalSlipBytesReceived)
            && (m_totalReceivedChunks == oPtr->m_totalReceivedChunks)
            && (m_largestReceivedBytesPerChunk == oPtr->m_largestReceivedBytesPerChunk)
            && (m_averageReceivedBytesPerChunk == oPtr->m_averageReceivedBytesPerChunk)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived);
    }
    return false;
}
bool SlipOverUartOutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool SlipOverUartOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalSlipBytesSent = pt.get<uint64_t>("totalSlipBytesSent");
        m_totalSlipBytesReceived = pt.get<uint64_t>("totalSlipBytesReceived");
        m_totalReceivedChunks = pt.get<uint64_t>("totalReceivedChunks");
        m_largestReceivedBytesPerChunk = pt.get<uint64_t>("largestReceivedBytesPerChunk");
        m_averageReceivedBytesPerChunk = pt.get<uint64_t>("averageReceivedBytesPerChunk");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree SlipOverUartOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalSlipBytesSent", m_totalSlipBytesSent);
    pt.put("totalSlipBytesReceived", m_totalSlipBytesReceived);
    pt.put("totalReceivedChunks", m_totalReceivedChunks);
    pt.put("largestReceivedBytesPerChunk", m_largestReceivedBytesPerChunk);
    pt.put("averageReceivedBytesPerChunk", m_averageReceivedBytesPerChunk);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}


BpOverEncapLocalStreamOutductTelemetry_t::BpOverEncapLocalStreamOutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalEncapHeaderBytesSent(0),
    m_totalEncapHeaderBytesReceived(0),
    m_largestEncapHeaderSizeBytesSent(0),
    m_smallestEncapHeaderSizeBytesSent(0),
    m_averageEncapHeaderSizeBytesSent(0),
    m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0)
{
    m_convergenceLayer = "bp_over_encap_local_stream";
}
BpOverEncapLocalStreamOutductTelemetry_t::~BpOverEncapLocalStreamOutductTelemetry_t() {}
bool BpOverEncapLocalStreamOutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const BpOverEncapLocalStreamOutductTelemetry_t* oPtr = dynamic_cast<const BpOverEncapLocalStreamOutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalEncapHeaderBytesSent == oPtr->m_totalEncapHeaderBytesSent)
            && (m_totalEncapHeaderBytesReceived == oPtr->m_totalEncapHeaderBytesReceived)
            && (m_largestEncapHeaderSizeBytesSent == oPtr->m_largestEncapHeaderSizeBytesSent)
            && (m_smallestEncapHeaderSizeBytesSent == oPtr->m_smallestEncapHeaderSizeBytesSent)
            && (m_averageEncapHeaderSizeBytesSent == oPtr->m_averageEncapHeaderSizeBytesSent)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived);
    }
    return false;
}
bool BpOverEncapLocalStreamOutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool BpOverEncapLocalStreamOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalEncapHeaderBytesSent = pt.get<uint64_t>("totalEncapHeaderBytesSent");
        m_totalEncapHeaderBytesReceived = pt.get<uint64_t>("totalEncapHeaderBytesReceived");
        m_largestEncapHeaderSizeBytesSent = pt.get<uint64_t>("largestEncapHeaderSizeBytesSent");
        m_smallestEncapHeaderSizeBytesSent = pt.get<uint64_t>("smallestEncapHeaderSizeBytesSent");
        m_averageEncapHeaderSizeBytesSent = pt.get<uint64_t>("averageEncapHeaderSizeBytesSent");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON TcpclV4OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree BpOverEncapLocalStreamOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalEncapHeaderBytesSent", m_totalEncapHeaderBytesSent);
    pt.put("totalEncapHeaderBytesReceived", m_totalEncapHeaderBytesReceived);
    pt.put("largestEncapHeaderSizeBytesSent", m_largestEncapHeaderSizeBytesSent);
    pt.put("smallestEncapHeaderSizeBytesSent", m_smallestEncapHeaderSizeBytesSent);
    pt.put("averageEncapHeaderSizeBytesSent", m_averageEncapHeaderSizeBytesSent);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}


UdpOutductTelemetry_t::UdpOutductTelemetry_t() :
    OutductTelemetry_t(),
    m_totalPacketsSent(0), m_totalPacketBytesSent(0), m_totalPacketsDequeuedForSend(0),
    m_totalPacketBytesDequeuedForSend(0), m_totalPacketsLimitedByRate(0)
{
    m_convergenceLayer = "udp";
}
UdpOutductTelemetry_t::~UdpOutductTelemetry_t() {}
bool UdpOutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    if (const UdpOutductTelemetry_t* oPtr = dynamic_cast<const UdpOutductTelemetry_t*>(&o)) {
        return OutductTelemetry_t::operator==(o)
            && (m_totalPacketsSent == oPtr->m_totalPacketsSent)
            && (m_totalPacketBytesSent == oPtr->m_totalPacketBytesSent)
            && (m_totalPacketsDequeuedForSend == oPtr->m_totalPacketsDequeuedForSend)
            && (m_totalPacketBytesDequeuedForSend == oPtr->m_totalPacketBytesDequeuedForSend)
            && (m_totalPacketsLimitedByRate == oPtr->m_totalPacketsLimitedByRate);
    }
    return false;
}
bool UdpOutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}
bool UdpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalPacketsSent = pt.get<uint64_t>("totalPacketsSent");
        m_totalPacketBytesSent = pt.get<uint64_t>("totalPacketBytesSent");
        m_totalPacketsDequeuedForSend = pt.get<uint64_t>("totalPacketsDequeuedForSend");
        m_totalPacketBytesDequeuedForSend = pt.get<uint64_t>("totalPacketBytesDequeuedForSend");
        m_totalPacketsLimitedByRate = pt.get<uint64_t>("totalPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON UdpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree UdpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalPacketsSent", m_totalPacketsSent);
    pt.put("totalPacketBytesSent", m_totalPacketBytesSent);
    pt.put("totalPacketsDequeuedForSend", m_totalPacketsDequeuedForSend);
    pt.put("totalPacketBytesDequeuedForSend", m_totalPacketBytesDequeuedForSend);
    pt.put("totalPacketsLimitedByRate", m_totalPacketsLimitedByRate);
    return pt;
}

AllOutductTelemetry_t::AllOutductTelemetry_t() :
    m_timestampMilliseconds(0),
    m_totalBundlesGivenToOutducts(0),
    m_totalBundleBytesGivenToOutducts(0),
    m_totalTcpclBundlesReceived(0),
    m_totalTcpclBundleBytesReceived(0),
    m_totalStorageToIngressOpportunisticBundles(0),
    m_totalStorageToIngressOpportunisticBundleBytes(0),
    m_totalBundlesSuccessfullySent(0),
    m_totalBundleBytesSuccessfullySent(0) {}
static bool UniquePtrOutductTelemEquivalent(const std::unique_ptr<OutductTelemetry_t>& a, const std::unique_ptr<OutductTelemetry_t>& b) {
    if ((!a) && (!b)) return true; //both null
    if (!a) return false;
    if (!b) return false;
    return ((*a) == (*b));
}
bool AllOutductTelemetry_t::operator==(const AllOutductTelemetry_t& o) const {
    return (m_timestampMilliseconds == o.m_timestampMilliseconds)
        && (m_totalBundlesGivenToOutducts == o.m_totalBundlesGivenToOutducts)
        && (m_totalBundleBytesGivenToOutducts == o.m_totalBundleBytesGivenToOutducts)
        && (m_totalTcpclBundlesReceived == o.m_totalTcpclBundlesReceived)
        && (m_totalTcpclBundleBytesReceived == o.m_totalTcpclBundleBytesReceived)
        && (m_totalStorageToIngressOpportunisticBundles == o.m_totalStorageToIngressOpportunisticBundles)
        && (m_totalStorageToIngressOpportunisticBundleBytes == o.m_totalStorageToIngressOpportunisticBundleBytes)
        && (m_totalBundlesSuccessfullySent == o.m_totalBundlesSuccessfullySent)
        && (m_totalBundleBytesSuccessfullySent == o.m_totalBundleBytesSuccessfullySent)
        && std::equal(m_listAllOutducts.begin(), m_listAllOutducts.end(), o.m_listAllOutducts.begin(), UniquePtrOutductTelemEquivalent);
}
bool AllOutductTelemetry_t::operator!=(const AllOutductTelemetry_t& o) const {
    return !(*this == o);
}
bool AllOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_timestampMilliseconds = pt.get<uint64_t>("timestampMilliseconds");
        m_totalBundlesGivenToOutducts = pt.get<uint64_t>("totalBundlesGivenToOutducts");
        m_totalBundleBytesGivenToOutducts = pt.get<uint64_t>("totalBundleBytesGivenToOutducts");
        m_totalTcpclBundlesReceived = pt.get<uint64_t>("totalTcpclBundlesReceived");
        m_totalTcpclBundleBytesReceived = pt.get<uint64_t>("totalTcpclBundleBytesReceived");
        m_totalStorageToIngressOpportunisticBundles = pt.get<uint64_t>("totalStorageToIngressOpportunisticBundles");
        m_totalStorageToIngressOpportunisticBundleBytes = pt.get<uint64_t>("totalStorageToIngressOpportunisticBundleBytes");
        m_totalBundlesSuccessfullySent = pt.get<uint64_t>("totalBundlesSuccessfullySent");
        m_totalBundleBytesSuccessfullySent = pt.get<uint64_t>("totalBundleBytesSuccessfullySent");
        const boost::property_tree::ptree& allOutductsPt = pt.get_child("allOutducts", EMPTY_PTREE); //non-throw version
        m_listAllOutducts.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductPt, allOutductsPt) {
            const std::string convergenceLayer = outductPt.second.get<std::string>("convergenceLayer");
            
            if ((convergenceLayer == "ltp_over_udp")
                || (convergenceLayer == "ltp_over_ipc")
                || (convergenceLayer == "ltp_over_encap_local_stream"))
            {
                m_listAllOutducts.emplace_back(boost::make_unique<LtpOutductTelemetry_t>());
            }
            else if (convergenceLayer == "bp_over_encap_local_stream") {
                m_listAllOutducts.emplace_back(boost::make_unique<BpOverEncapLocalStreamOutductTelemetry_t>());
            }
            else if (convergenceLayer == "slip_over_uart") {
                m_listAllOutducts.emplace_back(boost::make_unique<SlipOverUartOutductTelemetry_t>());
            }
            else if (convergenceLayer == "udp") {
                m_listAllOutducts.emplace_back(boost::make_unique<UdpOutductTelemetry_t>());
            }
            else if (convergenceLayer == "tcpcl_v3") {
                m_listAllOutducts.emplace_back(boost::make_unique<TcpclV3OutductTelemetry_t>());
            }
            else if (convergenceLayer == "tcpcl_v4") {
                m_listAllOutducts.emplace_back(boost::make_unique<TcpclV4OutductTelemetry_t>());
            }
            else if (convergenceLayer == "stcp") {
                m_listAllOutducts.emplace_back(boost::make_unique<StcpOutductTelemetry_t>());
            }
            else {
                return false;
            }
            if (!m_listAllOutducts.back()->SetValuesFromPropertyTree(outductPt.second)) {
                return false;
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON AllInductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree AllOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("timestampMilliseconds", m_timestampMilliseconds);
    pt.put("totalBundlesGivenToOutducts", m_totalBundlesGivenToOutducts);
    pt.put("totalBundleBytesGivenToOutducts", m_totalBundleBytesGivenToOutducts);
    pt.put("totalTcpclBundlesReceived", m_totalTcpclBundlesReceived);
    pt.put("totalTcpclBundleBytesReceived", m_totalTcpclBundleBytesReceived);
    pt.put("totalStorageToIngressOpportunisticBundles", m_totalStorageToIngressOpportunisticBundles);
    pt.put("totalStorageToIngressOpportunisticBundleBytes", m_totalStorageToIngressOpportunisticBundleBytes);
    pt.put("totalBundlesSuccessfullySent", m_totalBundlesSuccessfullySent);
    pt.put("totalBundleBytesSuccessfullySent", m_totalBundleBytesSuccessfullySent);
    boost::property_tree::ptree& allInductsPt = pt.put_child("allOutducts",
        m_listAllOutducts.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<std::unique_ptr<OutductTelemetry_t> >::const_iterator it = m_listAllOutducts.cbegin(); it != m_listAllOutducts.cend(); ++it) {
        const std::unique_ptr<OutductTelemetry_t>& outductPtr = *it;
        if (!outductPtr) {
            break;
        }
        allInductsPt.push_back(std::make_pair("", outductPtr->GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}

/**
 * ZmqConnectionID_t 
 */

ZmqConnectionId_t::ZmqConnectionId_t(const uint8_t val) {
    std::fill(m_id.begin(), m_id.end(), 0);
    m_id[ZMQ_CONNECTION_ID_LEN - 1] = val;
}

zmq::message_t ZmqConnectionId_t::Msg() {
    return zmq::message_t(m_id.data(), m_id.size());
}

bool ZmqConnectionId_t::operator==(const ZmqConnectionId_t& other) const {
    return m_id == other.m_id;
}

bool ZmqConnectionId_t::operator==(const zmq::message_t& msg) const {
    if (msg.size() != m_id.size()) {
        return false;
    }
    return memcmp(msg.data(), m_id.data(), m_id.size()) == 0;
}

ZmqConnectionId_t::ZmqConnectionId_t() {}

/**
 * API Command Names
*/
const std::string GetStorageApiCommand_t::name = "get_storage";
const std::string GetInductsApiCommand_t::name = "get_inducts";
const std::string GetOutductsApiCommand_t::name = "get_outducts";
const std::string GetOutductCapabilitiesApiCommand_t::name = "get_outduct_capabilities";
const std::string PingApiCommand_t::name = "ping";
const std::string UploadContactPlanApiCommand_t::name = "upload_contact_plan";
const std::string GetExpiringStorageApiCommand_t::name = "get_expiring_storage";
const std::string UpdateBpSecApiCommand_t::name = "update_bpsec_config";
const std::string GetBpSecApiCommand_t::name = "get_bpsec_config";
const std::string SetMaxSendRateApiCommand_t::name = "set_max_send_rate";
const std::string GetHdtnConfigApiCommand_t::name = "get_hdtn_config";
const std::string GetHdtnVersionApiCommand_t::name = "get_hdtn_version";
const std::string SetLinkDownApiCommand_t::name = "set_link_down";
const std::string SetLinkUpApiCommand_t::name = "set_link_up";

/**
 * ApiCommand_t 
 */

ApiCommand_t::ApiCommand_t() : m_apiCall("") {}
ApiCommand_t::~ApiCommand_t() {}

bool ApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt){
    try {
        m_apiCall = pt.get<std::string>("apiCall");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON ApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON ApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

bool ApiCommand_t::operator==(const ApiCommand_t& o) const {
    return m_apiCall == o.m_apiCall;
}

bool ApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}

boost::property_tree::ptree ApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("apiCall", m_apiCall);
    return pt;
}

std::shared_ptr<ApiCommand_t> ApiCommand_t::CreateFromJson(const std::string& jsonStr) {
    std::shared_ptr<ApiCommand_t> apiCommandPtr;
    boost::property_tree::ptree pt;
    if (!JsonSerializable::GetPropertyTreeFromJsonString(jsonStr, pt)) {
        LOG_ERROR(subprocess) << "error parsing JSON Api command";
        return apiCommandPtr; //null
    }
    std::string apiCall;
    try {
        apiCall = pt.get<std::string>("apiCall");
    }
    catch (const boost::property_tree::ptree_error&) {
        LOG_ERROR(subprocess) << "error getting apiCall from JSON Api command";
        return apiCommandPtr; //null
    }
    if (apiCall == PingApiCommand_t::name) {
        apiCommandPtr = std::make_shared<PingApiCommand_t>();
    }
    else if (apiCall == UploadContactPlanApiCommand_t::name) {
        apiCommandPtr = std::make_shared<UploadContactPlanApiCommand_t>();
    }
    else if (apiCall == GetExpiringStorageApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetExpiringStorageApiCommand_t>();
    }
    else if (apiCall == UpdateBpSecApiCommand_t::name) {
        apiCommandPtr = std::make_shared<UpdateBpSecApiCommand_t>();
    }
    else if (apiCall == GetBpSecApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetBpSecApiCommand_t>();
    }
    else if (apiCall == SetMaxSendRateApiCommand_t::name) {
        apiCommandPtr = std::make_shared<SetMaxSendRateApiCommand_t>();
    }
    else if (apiCall == GetInductsApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetInductsApiCommand_t>();
    }
    else if (apiCall == GetOutductsApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetOutductsApiCommand_t>();
    }
    else if (apiCall == GetOutductCapabilitiesApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetOutductCapabilitiesApiCommand_t>();
    }
    else if (apiCall == GetStorageApiCommand_t::name) {
        apiCommandPtr = std::make_shared<GetStorageApiCommand_t>();
    }
    else if (apiCall == SetLinkDownApiCommand_t::name) {
        apiCommandPtr = std::make_shared<SetLinkDownApiCommand_t>();
    }
    else if (apiCall == SetLinkUpApiCommand_t::name) {
        apiCommandPtr = std::make_shared<SetLinkUpApiCommand_t>();
    }
    else { //generic api command
        apiCommandPtr = std::make_shared<ApiCommand_t>();
    }

    if (!apiCommandPtr->SetValuesFromPropertyTree(pt)) {
        return std::shared_ptr<ApiCommand_t>(); //NULL
    }
    return apiCommandPtr;
}


/**
 * GetHdtnConfigApiCommand_t
 */
GetHdtnConfigApiCommand_t::GetHdtnConfigApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetHdtnConfigApiCommand_t::name;
}


/**
 * GetStorageApiCommand_t
 */
GetStorageApiCommand_t::GetStorageApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetStorageApiCommand_t::name;
}

GetStorageApiCommand_t::~GetStorageApiCommand_t() {}

/**
 * GetInductsApiCommand_t
 */
GetInductsApiCommand_t::GetInductsApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetInductsApiCommand_t::name;
}

GetInductsApiCommand_t::~GetInductsApiCommand_t() {}

/**
 * GetOutductsApiCommand_t
 */
GetOutductsApiCommand_t::GetOutductsApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetOutductsApiCommand_t::name;
}

GetOutductsApiCommand_t::~GetOutductsApiCommand_t() {}

/**
 * GetOutductCapabilitiesApiCommand_t
 */
GetOutductCapabilitiesApiCommand_t::GetOutductCapabilitiesApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetOutductCapabilitiesApiCommand_t::name;
}

GetOutductCapabilitiesApiCommand_t::~GetOutductCapabilitiesApiCommand_t() {}

/**
 * PingApiCommand_t 
 */

PingApiCommand_t::PingApiCommand_t()
    : ApiCommand_t(), m_nodeId(0), m_pingServiceNumber(0), m_bpVersion(0)
{
    ApiCommand_t::m_apiCall = PingApiCommand_t::name;
}
PingApiCommand_t::~PingApiCommand_t() {}

bool PingApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_bpVersion = pt.get<uint64_t>("bpVersion");
        m_nodeId = pt.get<uint64_t>("nodeId");
        m_pingServiceNumber = pt.get<uint64_t>("serviceId");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON PingApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON PingApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree PingApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("nodeId", m_nodeId);
    pt.put("serviceId", m_pingServiceNumber);
    pt.put("bpVersion", m_bpVersion);
    return pt;
}

bool PingApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const PingApiCommand_t* oPtr = dynamic_cast<const PingApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_nodeId == oPtr->m_nodeId) &&
            (m_pingServiceNumber == oPtr->m_pingServiceNumber) &&
            (m_bpVersion == oPtr->m_bpVersion);
    }
    return false;
}

bool PingApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}

/**
 * UploadContactPlanApiCommand_t 
 */

UploadContactPlanApiCommand_t::UploadContactPlanApiCommand_t()
    : ApiCommand_t(), m_contactPlanJson("{}")
{
    ApiCommand_t::m_apiCall = UploadContactPlanApiCommand_t::name;
}
UploadContactPlanApiCommand_t::~UploadContactPlanApiCommand_t() {}

bool UploadContactPlanApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_contactPlanJson = pt.get<std::string>("contactPlanJson");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON UploadContactPlanApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON UploadContactPlanApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree UploadContactPlanApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("contactPlanJson", m_contactPlanJson);
    return pt;
}

bool UploadContactPlanApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const UploadContactPlanApiCommand_t* oPtr = dynamic_cast<const UploadContactPlanApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_contactPlanJson == oPtr->m_contactPlanJson);
    }
    return false;
}

bool UploadContactPlanApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}

/**
 * GetExpiringStorageApiCommand_t
 */

GetExpiringStorageApiCommand_t::GetExpiringStorageApiCommand_t()
    : ApiCommand_t(), m_priority(0), m_thresholdSecondsFromNow(0)
{
    ApiCommand_t::m_apiCall = GetExpiringStorageApiCommand_t::name;
}
GetExpiringStorageApiCommand_t::~GetExpiringStorageApiCommand_t() {}

bool GetExpiringStorageApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_priority = pt.get<uint64_t>("priority");
        m_thresholdSecondsFromNow = pt.get<uint64_t>("thresholdSecondsFromNow");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON GetExpiringStorageApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON GetExpiringStorageApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree GetExpiringStorageApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("priority", m_priority);
    pt.put("thresholdSecondsFromNow", m_thresholdSecondsFromNow);
    return pt;
}

bool GetExpiringStorageApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const GetExpiringStorageApiCommand_t* oPtr = dynamic_cast<const GetExpiringStorageApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_priority == oPtr->m_priority) &&
            (m_thresholdSecondsFromNow == oPtr->m_thresholdSecondsFromNow);
    }
    return true;
}

bool GetExpiringStorageApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}

/**
 * UpdateBpSecApiCommand_t
 */

UpdateBpSecApiCommand_t::UpdateBpSecApiCommand_t()
    : ApiCommand_t(), m_bpSecJson("{}")
{
    ApiCommand_t::m_apiCall = UpdateBpSecApiCommand_t::name;
}
UpdateBpSecApiCommand_t::~UpdateBpSecApiCommand_t() {}

bool UpdateBpSecApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_bpSecJson = pt.get<std::string>("newBPSec");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON UpdateBpSecApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON UpdateBpSecApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree UpdateBpSecApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("newBPSec", m_bpSecJson);
    return pt;
}

bool UpdateBpSecApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const UpdateBpSecApiCommand_t* oPtr = dynamic_cast<const UpdateBpSecApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_bpSecJson == oPtr->m_bpSecJson);
    }
    return false;
}

bool UpdateBpSecApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}

/**
 * GetBpSecApiCommand_t
 */

GetBpSecApiCommand_t::GetBpSecApiCommand_t()
    : ApiCommand_t()
{
    ApiCommand_t::m_apiCall = GetBpSecApiCommand_t::name;
}
GetBpSecApiCommand_t::~GetBpSecApiCommand_t() {}


/**
 * SetMaxSendRateApiCommand_t
 */

SetMaxSendRateApiCommand_t::SetMaxSendRateApiCommand_t()
    : ApiCommand_t(), m_rateBitsPerSec(0), m_outduct(0)
{
    ApiCommand_t::m_apiCall = SetMaxSendRateApiCommand_t::name;
}

bool SetMaxSendRateApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_rateBitsPerSec = pt.get<uint64_t>("rateBitsPerSec");
        m_outduct = pt.get<uint64_t>("outduct");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetMaxSendRateApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetMaxSendRateApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree SetMaxSendRateApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("rateBitsPerSec", m_rateBitsPerSec);
    pt.put("outduct", m_outduct);
    return pt;
}

bool SetMaxSendRateApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const SetMaxSendRateApiCommand_t* oPtr = dynamic_cast<const SetMaxSendRateApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_rateBitsPerSec == oPtr->m_rateBitsPerSec) &&
            (m_outduct == oPtr->m_outduct);
    }
    return true;
}

bool SetMaxSendRateApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}



/**
 * SetLinkDownApiCommand_t
 */

SetLinkDownApiCommand_t::SetLinkDownApiCommand_t()
    : ApiCommand_t(), m_index(0)
{
    ApiCommand_t::m_apiCall = SetLinkDownApiCommand_t::name;
}

bool SetLinkDownApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_index = pt.get<uint64_t>("outductIndex");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetLinkDownApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetLinkDownApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree SetLinkDownApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("outductIndex", m_index);
    return pt;
}

bool SetLinkDownApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const SetLinkDownApiCommand_t* oPtr = dynamic_cast<const SetLinkDownApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_index == oPtr->m_index);
    }
    return true;
}

bool SetLinkDownApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}


/**
 * SetLinkUpApiCommand_t
 */

SetLinkUpApiCommand_t::SetLinkUpApiCommand_t()
    : ApiCommand_t(), m_index(0)
{
    ApiCommand_t::m_apiCall = SetLinkDownApiCommand_t::name;
}

bool SetLinkUpApiCommand_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!ApiCommand_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_index = pt.get<uint64_t>("outductIndex");
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetLinkUpApiCommand_t: " << e.what();
        return false;
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON SetLinkUpApiCommand_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree SetLinkUpApiCommand_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = ApiCommand_t::GetNewPropertyTree();
    pt.put("outductIndex", m_index);
    return pt;
}

bool SetLinkUpApiCommand_t::operator==(const ApiCommand_t& o) const {
    if (const SetLinkUpApiCommand_t* oPtr = dynamic_cast<const SetLinkUpApiCommand_t*>(&o)) {
        return ApiCommand_t::operator==(o)
            && (m_index == oPtr->m_index);
    }
    return true;
}

bool SetLinkUpApiCommand_t::operator!=(const ApiCommand_t& o) const {
    return !(*this == o);
}


/**
 * ApiResp_t
*/

ApiResp_t::ApiResp_t() : m_success(false), m_message("") {}
ApiResp_t::~ApiResp_t() {}

bool ApiResp_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt){
    try {
        m_success = pt.get<bool>("success");
        m_message = pt.get<std::string>("message");
    }
    catch (const boost::bad_lexical_cast&) {
        return false;
    }
    catch (const boost::property_tree::ptree_error&) {
        return false;
    }
    return true;
}

boost::property_tree::ptree ApiResp_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("success", m_success);
    pt.put("message", m_message);
    return pt;
}