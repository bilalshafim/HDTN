/**
 * @file StcpOutduct.cpp
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

#include "StcpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

StcpOutduct::StcpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid),
    m_stcpBundleSource(outductConfig.keepAliveIntervalSeconds, outductConfig.maxNumberOfBundlesInPipeline + 5)
{}
StcpOutduct::~StcpOutduct() {}

std::size_t StcpOutduct::GetTotalBundlesUnacked() const noexcept {
    return m_stcpBundleSource.GetTotalBundlesUnacked();
}
bool StcpOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(bundleData, size, std::move(userData));
}
bool StcpOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(movableDataZmq, std::move(userData));
}
bool StcpOutduct::Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(movableDataVec, std::move(userData));
}

void StcpOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_stcpBundleSource.SetOnFailedBundleVecSendCallback(callback);
}
void StcpOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_stcpBundleSource.SetOnFailedBundleZmqSendCallback(callback);
}
void StcpOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_stcpBundleSource.SetOnSuccessfulBundleSendCallback(callback);
}
void StcpOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_stcpBundleSource.SetOnOutductLinkStatusChangedCallback(callback);
}
void StcpOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_stcpBundleSource.SetUserAssignedUuid(userAssignedUuid);
}

void StcpOutduct::Connect() {
    m_stcpBundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool StcpOutduct::ReadyToForward() {
    return m_stcpBundleSource.ReadyToForward();
}
void StcpOutduct::Stop() {
    m_stcpBundleSource.Stop();
}
void StcpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalBundlesAcked = m_stcpBundleSource.GetTotalBundlesAcked();
    finalStats.m_totalBundlesSent = m_stcpBundleSource.GetTotalBundlesSent();
}
void StcpOutduct::PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) {
    std::unique_ptr<StcpOutductTelemetry_t> t = boost::make_unique<StcpOutductTelemetry_t>();
    m_stcpBundleSource.GetTelemetry(*t);
    outductTelem = std::move(t);
    outductTelem->m_linkIsUpPerTimeSchedule = m_linkIsUpPerTimeSchedule;
}
