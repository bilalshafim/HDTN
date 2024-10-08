/**
 * @file TestBpv6Fragmentation.cpp
 * @author  Evan Danish <evan.j.danish@nasa.gov>
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/thread.hpp>
#include "codec/BundleViewV6.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/next_prior.hpp>
#include <boost/make_unique.hpp>
#include "BinaryConversions.h"
#include "codec/bpv6.h"
#include "codec/Bpv6Fragment.h"
#include "codec/Bpv6FragmentManager.h"

BOOST_AUTO_TEST_SUITE(TestBpv6Fragmentation)

static const uint64_t PRIMARY_SRC_NODE = 1;
static const uint64_t PRIMARY_SRC_SVC = 2;
static const uint64_t PRIMARY_DEST_NODE = 3;
static const uint64_t PRIMARY_DEST_SVC = 4;
static const uint64_t PRIMARY_TIME = 1000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 5;

static void buildPrimaryBlock(Bpv6CbhePrimaryBlock & primary) {
    primary.SetZero();

    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.SetZero();
    primary.m_reportToEid.SetZero();
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
}

static std::unique_ptr<Bpv6CanonicalBlock> buildCanonicalBlock(std::string & blockBody, BPV6_BLOCK_TYPE_CODE type, BPV6_BLOCKFLAG flags) {
    std::unique_ptr<Bpv6CanonicalBlock> p = boost::make_unique<Bpv6CanonicalBlock>();
    Bpv6CanonicalBlock & block = *p;

    block.m_blockTypeCode = type;
    block.m_blockProcessingControlFlags = flags;
    block.m_blockTypeSpecificDataLength = blockBody.length();
    block.m_blockTypeSpecificDataPtr = (uint8_t*)blockBody.data(); //blockBody must remain in scope until after render

    return p;
}

static std::unique_ptr<Bpv6CanonicalBlock> buildPrimaryBlock(std::string & blockBody) {
        return buildCanonicalBlock(blockBody, BPV6_BLOCK_TYPE_CODE::PAYLOAD, BPV6_BLOCKFLAG::NO_FLAGS_SET);
}

BOOST_AUTO_TEST_CASE(FragmentZero)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, 0, fragments);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentBundleLength)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentFlagNoFrag)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::NOFRAGMENT;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == false);
}

static void CheckPrimaryBlock(Bpv6CbhePrimaryBlock &p, uint64_t offset, uint64_t aduLen) {

    BOOST_REQUIRE(p.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(p.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(p.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(p.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(p.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(p.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(p.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(p.m_fragmentOffset == offset);
    BOOST_REQUIRE(p.m_totalApplicationDataUnitLength == aduLen);
 
}

static void CheckCanonicalBlock(BundleViewV6::Bpv6CanonicalBlockView &block, size_t expectedLen, const void * expectedData, BPV6_BLOCK_TYPE_CODE type, BPV6_BLOCKFLAG flags) {
    BOOST_REQUIRE(block.headerPtr->m_blockTypeSpecificDataLength == expectedLen);
    BOOST_REQUIRE(memcmp(block.headerPtr->m_blockTypeSpecificDataPtr, expectedData, expectedLen) == 0);
    BOOST_REQUIRE(block.headerPtr->m_blockTypeCode == type);
    BOOST_REQUIRE(block.headerPtr->m_blockProcessingControlFlags == flags);
}

static void CheckPayload(BundleViewV6 & bv, size_t expectedLen, const void * expectedData, BPV6_BLOCKFLAG flags=BPV6_BLOCKFLAG::NO_FLAGS_SET) {
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> blocks;
    bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
    BOOST_REQUIRE(blocks.size() == 1);
    BundleViewV6::Bpv6CanonicalBlockView &payload = *blocks[0];

    if(bv.m_listCanonicalBlockView.back().headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD) {
        flags |= BPV6_BLOCKFLAG::IS_LAST_BLOCK;
    }
    CheckCanonicalBlock(payload, expectedLen, expectedData, BPV6_BLOCK_TYPE_CODE::PAYLOAD, flags);
}

struct FragmentPayloadTestData {
    // Inputs
    std::string payloadData;
    uint64_t fragmentPayloadSize;

    // expected
    struct DataAndOffset {
        std::string data;
        uint64_t offset;
    };
    std::vector<DataAndOffset> expectedFragments;
};

std::vector<FragmentPayloadTestData> FragmentPayloadTestVec = {
    {"abcdefghij", 5, {{"abcde", 0}, {"fghij", 5}}},
    {"klmnopqrst", 6, {{"klmnop", 0}, {"qrst", 6}}},
    {"uvwxyz0123", 2, {{"uv", 0}, {"wx", 2}, {"yz", 4}, {"01", 6}, {"23", 8}}},
    {"ABCDEFGHIJKLMNOP", 6, {{"ABCDEF", 0}, {"GHIJKL", 6}, {"MNOP", 12}}},
};

BOOST_DATA_TEST_CASE(
        FragmentPayload,
        boost::unit_test::data::xrange(FragmentPayloadTestVec.size()),
        testIndex)
{
    FragmentPayloadTestData test = FragmentPayloadTestVec[testIndex];
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(test.payloadData));

    BOOST_REQUIRE(bv.Render(5000));

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, test.fragmentPayloadSize, fragments);
    BOOST_REQUIRE(ret == true);

    uint64_t expectedAduLen = test.payloadData.size();

    BOOST_REQUIRE(fragments.size() == test.expectedFragments.size());

    std::list<BundleViewV6>::iterator it = fragments.begin();
    for(uint64_t i = 0, numFragments = fragments.size(); i < numFragments; i++, it++) {
        BundleViewV6 & b = *it;

        uint64_t expectedOffset = test.expectedFragments[i].offset;
        std::string &expectedData = test.expectedFragments[i].data;

        CheckPrimaryBlock(b.m_primaryBlockView.header, expectedOffset, expectedAduLen);
        BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);

        uint64_t expectedFragmentSize = expectedData.size();
        CheckPayload(b, expectedFragmentSize, expectedData.data());

    }
}

BOOST_AUTO_TEST_CASE(FragmentPayloadMultiple)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & a = *(it++);
    BundleViewV6 & b = *(it++);
    BundleViewV6 & c = *(it++);

    CheckPrimaryBlock(a.m_primaryBlockView.header, 0, 14);
    CheckPrimaryBlock(b.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(c.m_primaryBlockView.header, 12, 14);

    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(c.m_listCanonicalBlockView.size() == 1);

    CheckPayload(a, 6, "helloB");
    CheckPayload(b, 6, "igworl");
    CheckPayload(c, 2, "d!");
}

BOOST_AUTO_TEST_CASE(FragmentFragment)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    it++;
    BundleViewV6 & b = *it;

    std::list<BundleViewV6> bFrags;

    ret = Bpv6Fragmenter::Fragment(b, 3, bFrags);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(bFrags.size() == 2);

    BundleViewV6 & front = bFrags.front(), & back = bFrags.back();

    CheckPrimaryBlock(front.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(back.m_primaryBlockView.header, 9, 14);

    BOOST_REQUIRE(front.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(back.m_listCanonicalBlockView.size() == 1);

    CheckPayload(front, 3, "igw");
    CheckPayload(back, 3, "orl");
}

BOOST_AUTO_TEST_CASE(FragmentBlockBefore)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string beforeBlockBody = "before block";
    bv.AppendMoveCanonicalBlock(
        buildCanonicalBlock(
            beforeBlockBody,
            BPV6_BLOCK_TYPE_CODE::UNUSED_11,
            BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED));

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & a = *(it++);
    BundleViewV6 & b = *(it++);
    BundleViewV6 & c = *(it++);

    CheckPrimaryBlock(a.m_primaryBlockView.header, 0, 14);
    CheckPrimaryBlock(b.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(c.m_primaryBlockView.header, 12, 14);

    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 2);
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(c.m_listCanonicalBlockView.size() == 1);

    CheckCanonicalBlock(a.m_listCanonicalBlockView.front(),
            beforeBlockBody.size(),
            beforeBlockBody.data(),
            BPV6_BLOCK_TYPE_CODE::UNUSED_11,
            BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED);

    CheckPayload(a, 6, "helloB");
    CheckPayload(b, 6, "igworl");
    CheckPayload(c, 2, "d!");
}

// TODO add test cases for:
// + blocks before payload
// + blocks after payload
// + repeat-in-all blocks

struct BlockTestInfo {
    std::string body;
    BPV6_BLOCK_TYPE_CODE type;
    BPV6_BLOCKFLAG flags;
};

struct MultiBlockTestInfo {
    std::vector<BlockTestInfo> beforeBlocks;
    std::vector<BlockTestInfo> afterBlocks;


    std::vector<BlockTestInfo> GetReplicatedBefore() {
        BPV6_BLOCKFLAG rep = BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
        std::vector<BlockTestInfo> ret;
        for(std::vector<BlockTestInfo>::iterator it = beforeBlocks.begin(); it != beforeBlocks.end(); it++) {
            BlockTestInfo &bi = *it;
            if((bi.flags & rep) == rep) {
                ret.push_back(bi);
            }
        }
        return ret;
    }
    std::vector<BlockTestInfo> GetReplicatedAfter() {
        BPV6_BLOCKFLAG rep = BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
        std::vector<BlockTestInfo> ret;
        for(std::vector<BlockTestInfo>::iterator it = afterBlocks.begin(); it != afterBlocks.end(); it++) {
            BlockTestInfo &bi = *it;
            if((bi.flags & rep) == rep) {
                ret.push_back(bi);
            }
        }
        return ret;
    }
    uint64_t numReplicatedBefore() {
        return GetReplicatedBefore().size();
    }

    uint64_t numReplicatedAfter() {
        return GetReplicatedAfter().size();
    }

    uint64_t numReplicated() {
        return numReplicatedBefore() + numReplicatedAfter();
    }

};

MultiBlockTestInfo MultiBlockTestInfos[] = {
    { // Only payload
        { // beforeBlocks
        },
        { // afterBlocks
        },
    },
    { // Single before block
        { // beforeBlocks
            BlockTestInfo{"before1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
        { // afterBlocks
        },
    },
    { // Single after block
        { // beforeBlocks
        },
        { // afterBlocks
            BlockTestInfo{"after1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
    },
    { // One before and one after
        { // beforeBlocks
            BlockTestInfo{"before1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
        { // afterBlocks
            BlockTestInfo{"after1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
    },
    { // Single before block REPLICATED IN ALL
        { // beforeBlocks
            BlockTestInfo{"before1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
        },
        { // afterBlocks
        },
    },
    { // Single after block REPLICATED IN ALL
        { // beforeBlocks
        },
        { // afterBlocks
            BlockTestInfo{"after1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
        },
    },
    { // One before and one after REPLICATED IN ALL
        { // beforeBlocks
            BlockTestInfo{"before1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
        },
        { // afterBlocks
            BlockTestInfo{"after1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
        },
    },
    { // Big mix
        { // beforeBlocks
            BlockTestInfo{"before1", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"before2", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
            BlockTestInfo{"before3", BPV6_BLOCK_TYPE_CODE::UNUSED_6, BPV6_BLOCKFLAG::NO_FLAGS_SET},
            BlockTestInfo{"before4", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT | BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"before5", BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCKFLAG::NO_FLAGS_SET},
            BlockTestInfo{"before6", BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCKFLAG::NO_FLAGS_SET},
            BlockTestInfo{"before7", BPV6_BLOCK_TYPE_CODE::UNUSED_12, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
            BlockTestInfo{"before8", BPV6_BLOCK_TYPE_CODE::UNUSED_6, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT | BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"before9", BPV6_BLOCK_TYPE_CODE::UNUSED_12, BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
        },
        { // afterBlocks
            BlockTestInfo{"after1", BPV6_BLOCK_TYPE_CODE::UNUSED_6, BPV6_BLOCKFLAG::NO_FLAGS_SET},
            BlockTestInfo{"after2", BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCKFLAG::NO_FLAGS_SET},
            BlockTestInfo{"after3", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"after4", BPV6_BLOCK_TYPE_CODE::UNUSED_12, BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"after5", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
            BlockTestInfo{"after6", BPV6_BLOCK_TYPE_CODE::UNUSED_6, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT | BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
            BlockTestInfo{"after7", BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT},
            BlockTestInfo{"after8", BPV6_BLOCK_TYPE_CODE::UNUSED_6, BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT | BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED},
        },
    },
};

static void CheckBlocks(BundleViewV6 &bv, const std::vector<BlockTestInfo> &beforeBlocks, const std::vector<BlockTestInfo> &afterBlocks) {
        BundleViewV6::canonical_block_view_list_t::iterator blockIt = bv.m_listCanonicalBlockView.begin();
        for(std::vector<BlockTestInfo>::const_iterator it = beforeBlocks.cbegin(); it != beforeBlocks.cend(); it++) {
            const BlockTestInfo &bi = *it;
            if(blockIt == bv.m_listCanonicalBlockView.end()) {
                BOOST_FAIL("Reached end of blocks while testing before payload blocks");
                return; // needed for static analysis
            }
            CheckCanonicalBlock(*blockIt,
                    bi.body.size(),
                    bi.body.data(),
                    bi.type,
                    bi.flags);
            blockIt++;
        }
        if(blockIt == bv.m_listCanonicalBlockView.end()) {
            BOOST_FAIL("Reached end of blocks while looking for payload");
            return; // needed for static analysis
        }
        BOOST_REQUIRE(blockIt->headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD);
        blockIt++; // Skip  payload
        for(std::vector<BlockTestInfo>::const_iterator it = afterBlocks.cbegin(); it != afterBlocks.cend(); it++) {
            const BlockTestInfo &bi = *it;
            if(blockIt == bv.m_listCanonicalBlockView.end()) {
                BOOST_FAIL("Reached end of blocks while testing after payload blocks");
                return; // needed for static analysis
            }
            BPV6_BLOCKFLAG flags = bi.flags;
            if(&bi == &afterBlocks.back()) {
                flags |= BPV6_BLOCKFLAG::IS_LAST_BLOCK;
            }
            CheckCanonicalBlock(*blockIt,
                    bi.body.size(),
                    bi.body.data(),
                    bi.type,
                    flags);
            blockIt++;
        }
        BOOST_REQUIRE(blockIt == bv.m_listCanonicalBlockView.end());
}

BOOST_DATA_TEST_CASE(
        FragmentExtraBlocks,
        boost::unit_test::data::xrange(sizeof(MultiBlockTestInfos)/sizeof(*MultiBlockTestInfos)),
        testIndex)
{

    MultiBlockTestInfo & info = MultiBlockTestInfos[testIndex];

    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    for(std::vector<BlockTestInfo>::iterator it = info.beforeBlocks.begin(); it != info.beforeBlocks.end(); it++) {
        BlockTestInfo &bi = *it;
        bv.AppendMoveCanonicalBlock(buildCanonicalBlock(bi.body, bi.type, bi.flags));
    }

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    for(std::vector<BlockTestInfo>::iterator it = info.afterBlocks.begin(); it != info.afterBlocks.end(); it++) {
        BlockTestInfo &bi = *it;
        bv.AppendMoveCanonicalBlock(buildCanonicalBlock(bi.body, bi.type, bi.flags));
    }

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & first = *(it++);
    BundleViewV6 & middle = *(it++);
    BundleViewV6 & last = *(it++);

    CheckPrimaryBlock(first.m_primaryBlockView.header, 0, 14);
    CheckPrimaryBlock(middle.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(last.m_primaryBlockView.header, 12, 14);

    CheckPayload(first, 6, "helloB");
    CheckPayload(middle, 6, "igworl");
    CheckPayload(last, 2, "d!");

    BOOST_REQUIRE(first.m_listCanonicalBlockView.size() == 1 + info.beforeBlocks.size() + info.numReplicatedAfter());
    BOOST_REQUIRE(middle.m_listCanonicalBlockView.size() == 1 + info.numReplicated());
    BOOST_REQUIRE(last.m_listCanonicalBlockView.size() == 1 + info.afterBlocks.size() + info.numReplicatedBefore());

    CheckBlocks(first, info.beforeBlocks, info.GetReplicatedAfter());
    CheckBlocks(middle, info.GetReplicatedBefore(), info.GetReplicatedAfter());
    CheckBlocks(last, info.GetReplicatedBefore(), info.afterBlocks);

}
BOOST_DATA_TEST_CASE(
        DefragMulti,
        boost::unit_test::data::xrange(sizeof(MultiBlockTestInfos)/sizeof(*MultiBlockTestInfos)),
        testIndex)
{

    MultiBlockTestInfo & info = MultiBlockTestInfos[testIndex];

    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    for(std::vector<BlockTestInfo>::iterator it = info.beforeBlocks.begin(); it != info.beforeBlocks.end(); it++) {
        BlockTestInfo &bi = *it;
        bv.AppendMoveCanonicalBlock(buildCanonicalBlock(bi.body, bi.type, bi.flags));
    }

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    for(std::vector<BlockTestInfo>::iterator it = info.afterBlocks.begin(); it != info.afterBlocks.end(); it++) {
        BlockTestInfo &bi = *it;
        bv.AppendMoveCanonicalBlock(buildCanonicalBlock(bi.body, bi.type, bi.flags));
    }

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    BOOST_REQUIRE(Bpv6Fragmenter::Fragment(bv, sz, fragments));

    BundleViewV6 av;

    BOOST_REQUIRE(Bpv6Fragmenter::Assemble(fragments, av));
    BOOST_REQUIRE(av.Render(5000));

    BOOST_REQUIRE(bv.m_renderedBundle.size() == av.m_renderedBundle.size());

    size_t bundleLen = bv.m_renderedBundle.size();

    int cmp = memcmp(bv.m_renderedBundle.data(), av.m_renderedBundle.data(), bundleLen); 

    BOOST_REQUIRE(cmp == 0);
}

BOOST_AUTO_TEST_CASE(AssembleMissing)
{

    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "hello world!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 5;

    std::list<BundleViewV6> fragments;
    BOOST_REQUIRE(Bpv6Fragmenter::Fragment(bv, sz, fragments));

    // Remove middle fragment
    std::list<BundleViewV6>::iterator it = fragments.begin();
    it++;
    fragments.erase(it);

    BundleViewV6 av;

    BOOST_REQUIRE(!Bpv6Fragmenter::Assemble(fragments, av));
}

BOOST_AUTO_TEST_CASE(AssembleDifferent)
{

    BundleViewV6 a, b;
    std::string body = "hello world!";

    { // Build a
        Bpv6CbhePrimaryBlock & primary = a.m_primaryBlockView.header;
        buildPrimaryBlock(primary);
        a.m_primaryBlockView.SetManuallyModified();

        a.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

        BOOST_REQUIRE(a.Render(5000));
    }
    { // Build b, different timestamp
        Bpv6CbhePrimaryBlock & primary = b.m_primaryBlockView.header;
        buildPrimaryBlock(primary);
        primary.m_creationTimestamp.sequenceNumber++;
        b.m_primaryBlockView.SetManuallyModified();

        b.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

        BOOST_REQUIRE(b.Render(5000));
    }
    size_t sz = 5;


    std::list<BundleViewV6> fragmentsA, fragmentsB;
    BOOST_REQUIRE(Bpv6Fragmenter::Fragment(a, sz, fragmentsA));
    BOOST_REQUIRE(Bpv6Fragmenter::Fragment(b, sz, fragmentsB));

    BOOST_REQUIRE(fragmentsA.size() == 3);
    BOOST_REQUIRE(fragmentsB.size() == 3);

    // Remove middle fragment of A, replace with middle of B
    std::list<BundleViewV6>::iterator itBackA, itA = fragmentsA.begin();
    itA++;
    itBackA = fragmentsA.erase(itA);

    std::list<BundleViewV6>::iterator itB = fragmentsB.begin();
    itB++;
    fragmentsA.splice(itBackA, fragmentsB, itB);

    BundleViewV6 av;

    BOOST_REQUIRE(!Bpv6Fragmenter::Assemble(fragmentsA, av));
}

BOOST_AUTO_TEST_CASE(AssembleNotAFragment)
{

    std::list<BundleViewV6> notFragments;
    notFragments.emplace_back();
    BundleViewV6 & bv = notFragments.front();

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "hello world!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));

    BOOST_REQUIRE(notFragments.size() == 1);

    BundleViewV6 av;

    BOOST_REQUIRE(!Bpv6Fragmenter::Assemble(notFragments, av));
}

BOOST_AUTO_TEST_CASE(AssembleEmpty)
{

    std::list<BundleViewV6> emptyFragments;

    BundleViewV6 av;
    BOOST_REQUIRE(!Bpv6Fragmenter::Assemble(emptyFragments, av));
}

BOOST_AUTO_TEST_CASE(FragmentManagerNullData)
{
    Bpv6FragmentManager mgr;
    BundleViewV6 bv;
    bool isComplete = false;

    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(NULL, 0, isComplete, bv) == false);
}

BOOST_AUTO_TEST_CASE(FragmentManagerNotABundle)
{
    Bpv6FragmentManager mgr;
    BundleViewV6 bv;
    bool isComplete = false;
    std::vector<uint8_t> data(5, 0);

    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(data.data(), data.size(), isComplete, bv) == false);
}

BOOST_AUTO_TEST_CASE(FragmentManagerNotAFragment)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));

    Bpv6FragmentManager mgr;
    BundleViewV6 av;
    bool isComplete = false;

    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(bv.m_frontBuffer.data(), bv.m_frontBuffer.size(), isComplete, av) == false);
}

BOOST_AUTO_TEST_CASE(FragmentManager)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Hello World!";
    bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

    BOOST_REQUIRE(bv.Render(5000));

    std::list<BundleViewV6> fragments;
    bool ret = Bpv6Fragmenter::Fragment(bv, 4, fragments);
    BOOST_REQUIRE(ret == true);
    BOOST_REQUIRE(fragments.size() == 3);

    Bpv6FragmentManager mgr;
    BundleViewV6 av;
    bool isComplete = true;

    std::list<BundleViewV6>::iterator it = fragments.begin();

    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(it->m_frontBuffer.data(), it->m_frontBuffer.size(), isComplete, av) == true);
    BOOST_REQUIRE(isComplete == false);
    it++;
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(it->m_frontBuffer.data(), it->m_frontBuffer.size(), isComplete, av) == true);
    BOOST_REQUIRE(isComplete == false);
    it++;
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(it->m_frontBuffer.data(), it->m_frontBuffer.size(), isComplete, av) == true);
    BOOST_REQUIRE(isComplete == true);

    BOOST_REQUIRE(av.m_renderedBundle.size() == bv.m_renderedBundle.size());
    BOOST_REQUIRE(memcmp(av.m_renderedBundle.data(), bv.m_renderedBundle.data(), av.m_renderedBundle.size()) == 0);

}

BOOST_AUTO_TEST_CASE(FragmentManagerMulti)
{
    BundleViewV6 bv, cv;
    std::list<BundleViewV6> bFragments, cFragments;

    {
        Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        buildPrimaryBlock(primary);
        bv.m_primaryBlockView.SetManuallyModified();

        std::string body = "HelloWorld";
        bv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

        BOOST_REQUIRE(bv.Render(5000));

        bool ret = Bpv6Fragmenter::Fragment(bv, 5, bFragments);
        BOOST_REQUIRE(ret == true);
        BOOST_REQUIRE(bFragments.size() == 2);
    }

    {
        Bpv6CbhePrimaryBlock & primary = cv.m_primaryBlockView.header;
        buildPrimaryBlock(primary);
        cv.m_primaryBlockView.header.m_sourceNodeId.serviceId++;
        cv.m_primaryBlockView.SetManuallyModified();

        std::string body = "foobar";
        cv.AppendMoveCanonicalBlock(buildPrimaryBlock(body));

        BOOST_REQUIRE(cv.Render(5000));

        bool ret = Bpv6Fragmenter::Fragment(cv, 3, cFragments);
        BOOST_REQUIRE(ret == true);
        BOOST_REQUIRE(cFragments.size() == 2);
    }

    Bpv6FragmentManager mgr;
    BundleViewV6 bav, cav;
    bool isComplete = true;

    // First B fragment
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(bFragments.front().m_frontBuffer.data(), bFragments.front().m_frontBuffer.size(), isComplete, bav) == true);
    BOOST_REQUIRE(isComplete == false);
    // First C fragment
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(cFragments.front().m_frontBuffer.data(), cFragments.front().m_frontBuffer.size(), isComplete, cav) == true);
    BOOST_REQUIRE(isComplete == false);
    // Second B fragment
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(bFragments.back().m_frontBuffer.data(), bFragments.back().m_frontBuffer.size(), isComplete, bav) == true);
    BOOST_REQUIRE(isComplete == true);
    BOOST_REQUIRE(bav.m_renderedBundle.size() == bv.m_renderedBundle.size());
    BOOST_REQUIRE(memcmp(bav.m_renderedBundle.data(), bv.m_renderedBundle.data(), bav.m_renderedBundle.size()) == 0);
    // Second C fragment
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(cFragments.back().m_frontBuffer.data(), cFragments.back().m_frontBuffer.size(), isComplete, cav) == true);
    BOOST_REQUIRE(isComplete == true);
    BOOST_REQUIRE(cav.m_renderedBundle.size() == cv.m_renderedBundle.size());
    BOOST_REQUIRE(memcmp(cav.m_renderedBundle.data(), cv.m_renderedBundle.data(), cav.m_renderedBundle.size()) == 0);

    BundleViewV6 bDontCare, cDontCare;

    // Check B removed
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(bFragments.front().m_frontBuffer.data(), bFragments.front().m_frontBuffer.size(), isComplete, bDontCare) == true);
    BOOST_REQUIRE(isComplete == false);
    // Check C removed
    BOOST_REQUIRE(mgr.AddFragmentAndGetComplete(cFragments.front().m_frontBuffer.data(), cFragments.front().m_frontBuffer.size(), isComplete, cDontCare) == true);
    BOOST_REQUIRE(isComplete == false);

}

struct CalcNumFragsTestData {
    uint64_t payloadSize;
    uint64_t fragmentSize;
    uint64_t expected;
};

std::vector<CalcNumFragsTestData> CalcNumFragsTestVec = {
    { 2, 1, 2},
    { 30, 10, 3},
    { 30, 15, 2},
    { 30, 29, 2},
    { 30, 14, 3},
    { 30, 9, 4},
};

BOOST_DATA_TEST_CASE(
        TestCalcNumFragments,
        boost::unit_test::data::xrange(CalcNumFragsTestVec.size()),
        testIndex)
{
    CalcNumFragsTestData &test = CalcNumFragsTestVec[testIndex];
    uint64_t numFragments = Bpv6Fragmenter::CalcNumFragments(test.payloadSize, test.fragmentSize);
    BOOST_REQUIRE(numFragments == test.expected);
}

/* Compare with reference fragments */

BOOST_AUTO_TEST_CASE(TestReferenceFragmentsSize20)
{
    const std::string unfragmentedHex(
            "06811014811501811501811501000082"
            "e3c9823b01822c0005110a69706e0031"
            "34392e30001401010001091b61626364"
            "65666768696a6b6c6d6e6f7071727374"
            "75767778797a0a"
            );
    const std::string fragAHex(
            "06811116811501811501811501000082"
            "e3c9823b01822c00001b05110a69706e"
            "003134392e3000140101000109146162"
            "636465666768696a6b6c6d6e6f707172"
            "7374"
            );
    const std::string fragBHex(
            "06811116811501811501811501000082"
            "e3c9823b01822c00141b05110a69706e"
            "003134392e3000140101000109077576"
            "7778797a0a"
            );

    std::vector<uint8_t> ref, refA, refB;

    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(unfragmentedHex, ref));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragAHex, refA));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragBHex, refB));

    // Fragment
    {
        BundleViewV6 bv;

        BOOST_REQUIRE(bv.CopyAndLoadBundle(ref.data(), ref.size()));

        std::list<BundleViewV6> fragments;
        BOOST_REQUIRE(Bpv6Fragmenter::Fragment(bv, 20, fragments));

        BOOST_REQUIRE(fragments.size() == 2);

        BundleViewV6 &a = fragments.front(), &b = fragments.back();

        BOOST_REQUIRE(a.m_renderedBundle.size() == refA.size());
        BOOST_REQUIRE(b.m_renderedBundle.size() == refB.size());

        CheckPayload(a, 20, "abcdefghijklmnopqrst", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);
        CheckPayload(b, 7, "uvwxyz\n", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);

        BOOST_REQUIRE(!memcmp(a.m_renderedBundle.data(), refA.data(), refA.size()));
        BOOST_REQUIRE(!memcmp(b.m_renderedBundle.data(), refB.data(), refB.size()));
    }

    // Assemble
    {
        std::list<BundleViewV6> fragments;
        BundleViewV6 &a = *fragments.emplace(fragments.cend());
        BundleViewV6 &b = *fragments.emplace(fragments.cend());

        BOOST_REQUIRE(a.CopyAndLoadBundle(refA.data(), refA.size()));
        BOOST_REQUIRE(b.CopyAndLoadBundle(refB.data(), refB.size()));

        BundleViewV6 v;
        BOOST_REQUIRE(Bpv6Fragmenter::Assemble(fragments, v));
        BOOST_REQUIRE(v.Render(5000));

        BOOST_REQUIRE(v.m_renderedBundle.size() == ref.size());

        CheckPayload(v, 27, "abcdefghijklmnopqrstuvwxyz\n", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);

        BOOST_REQUIRE(!memcmp(v.m_renderedBundle.data(), ref.data(), ref.size()));
    }
}

BOOST_AUTO_TEST_CASE(TestReferenceFragmentsSize10)
{
    const std::string unfragmentedHex(
            "06811014811501811501811501000082"
            "e3c9856901822c0005110a69706e0031"
            "34392e30001401010001091b61626364"
            "65666768696a6b6c6d6e6f7071727374"
            "75767778797a0a"
            );
    const std::string fragAHex(
            "06811116811501811501811501000082"
            "e3c9856901822c00001b05110a69706e"
            "003134392e30001401010001090a6162"
            "636465666768696a"
            );
    const std::string fragBHex(
            "06811116811501811501811501000082"
            "e3c9856901822c000a1b05110a69706e"
            "003134392e30001401010001090a6b6c"
            "6d6e6f7071727374"
            );
    const std::string fragCHex(
            "06811116811501811501811501000082"
            "e3c9856901822c00141b05110a69706e"
            "003134392e3000140101000109077576"
            "7778797a0a"
            );

    std::vector<uint8_t> ref, refA, refB, refC;

    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(unfragmentedHex, ref));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragAHex, refA));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragBHex, refB));
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(fragCHex, refC));

    // Fragment
    {
        BundleViewV6 bv;

        BOOST_REQUIRE(bv.CopyAndLoadBundle(ref.data(), ref.size()));

        std::list<BundleViewV6> fragments;
        BOOST_REQUIRE(Bpv6Fragmenter::Fragment(bv, 10, fragments));

        BOOST_REQUIRE(fragments.size() == 3);

        std::list<BundleViewV6>::iterator it = fragments.begin();

        BundleViewV6 &a = *(it++);
        BundleViewV6 &b = *(it++);
        BundleViewV6 &c = *(it++);

        BOOST_REQUIRE(a.m_renderedBundle.size() == refA.size());
        BOOST_REQUIRE(b.m_renderedBundle.size() == refB.size());
        BOOST_REQUIRE(c.m_renderedBundle.size() == refC.size());

        CheckPayload(a, 10, "abcdefghij", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);
        CheckPayload(b, 10, "klmnopqrst", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);
        CheckPayload(c, 7, "uvwxyz\n", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);

        BOOST_REQUIRE(!memcmp(a.m_renderedBundle.data(), refA.data(), refA.size()));
        BOOST_REQUIRE(!memcmp(b.m_renderedBundle.data(), refB.data(), refB.size()));
        BOOST_REQUIRE(!memcmp(c.m_renderedBundle.data(), refC.data(), refC.size()));
    }

    // Assemble
    {
        std::list<BundleViewV6> fragments;
        BundleViewV6 &a = *fragments.emplace(fragments.cend());
        BundleViewV6 &b = *fragments.emplace(fragments.cend());
        BundleViewV6 &c = *fragments.emplace(fragments.cend());

        BOOST_REQUIRE(a.CopyAndLoadBundle(refA.data(), refA.size()));
        BOOST_REQUIRE(b.CopyAndLoadBundle(refB.data(), refB.size()));
        BOOST_REQUIRE(c.CopyAndLoadBundle(refC.data(), refC.size()));

        BundleViewV6 v;
        BOOST_REQUIRE(Bpv6Fragmenter::Assemble(fragments, v));
        BOOST_REQUIRE(v.Render(5000));

        BOOST_REQUIRE(v.m_renderedBundle.size() == ref.size());

        CheckPayload(v, 27, "abcdefghijklmnopqrstuvwxyz\n", BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT);

        BOOST_REQUIRE(!memcmp(v.m_renderedBundle.data(), ref.data(), ref.size()));
    }
}

BOOST_AUTO_TEST_SUITE_END()

