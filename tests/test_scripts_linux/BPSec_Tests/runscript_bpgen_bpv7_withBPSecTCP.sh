#!/bin/sh

# path variables
config_files=$HDTN_SOURCE_ROOT/config_files
hdtn_config=$config_files/hdtn/hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json
sink_config=$config_files/inducts/bpsink_one_tcpcl_port4558.json
gen_config=$config_files/outducts/bpgen_one_tcpcl_port4556.json
bpsink_bpsec_config=$config_files/bpsec/ipn2.1_con_plus_int.json
bpgen_bpsec_config=$config_files/bpsec/ipn1.1_con_plus_int.json
hdtn_bpsec_config=$config_files/bpsec/ipn10.1_con_plus_int.json

cd $HDTN_SOURCE_ROOT

#bpsink
./build/common/bpcodec/apps/bpsink-async --my-uri-eid=ipn:2.1 --inducts-config-file=$sink_config --bpsec-config-file=$bpsink_bpsec_config &
sleep 3

# HDTN one process
# use the option --use-unix-timestamp when using a contact plan with unix timestamp
# use the option --use-mgr to use Multigraph Routing Algorithm (the default routing Algorithm is CGR Dijkstra)
./build/module/hdtn_one_process/hdtn-one-process --contact-plan-file=contactPlan.json --hdtn-config-file=$hdtn_config  --bpsec-config-file=$hdtn_bpsec_config &
sleep 6

# Bpgen
./build/common/bpcodec/apps/bpgen-async  --use-bp-version-7 --bundle-rate=0 --bundle-size=50 --duration=50 --my-uri-eid=ipn:1.1 --dest-uri-eid=ipn:2.1 --outducts-config-file=$gen_config --bpsec-config-file=$bpgen_bpsec_config &
sleep 8


