#pragma once
struct AmeriPublicVector{const char* kind;const char* body;unsigned long long counter;long long timestamp;const char* nonce;const char* canonical;const char* signature;};
static const char* AMERI_TEST_CONTROL="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char* AMERI_TEST_DERIVED="930fea37a29940704b6205f92e4f557f261316d1c97d9447e80a59c6ff149107";
static const AmeriPublicVector AMERI_TEST_VECTORS[]={
{"poll","{\"board\":\"upb-cell-v1\",\"busy\":false,\"chip_id\":0,\"clock_ready\":true,\"family\":\"cell\",\"ota_supported\":true,\"pending_records\":0,\"project\":\"CeldaAmbiental\",\"rollback_supported\":true,\"slot_bytes\":1966080,\"valve_closed\":true,\"version\":\"2.0.0\"}",1,1791500000,"00000000000000000000000000000001","ameri-ota-1-poll\ncelda_1\n1\n1791500000\n00000000000000000000000000000001\n8b9023aa048d73a920bc5b4f561508f0fb51270b7bbdc3b3c626b196a8997e2a","eaaae684e60943f2b04d98d56ac8e6465218dc8a6157bc2a8fd20a6cedaf19c0"},
{"ack","{\"error_code\":\"none\",\"job_id\":\"11111111111111111111111111111111\",\"progress\":40,\"state\":\"downloading\"}",2,1791500000,"00000000000000000000000000000002","ameri-ota-1-ack\ncelda_1\n2\n1791500000\n00000000000000000000000000000002\nddba151994e6d3cb7367e414b2e5f551720b9bf6dc4973a498276ebd07b8f052","0d6eed2fae0d998e497ba009f37334c52189772b5f2aeac50e9afa697159a70e"},
{"response","{\"action\":\"none\",\"note\":\"prueba UTF-8: á ✓\"}",3,1791500000,"00000000000000000000000000000003","ameri-ota-1-response\ncelda_1\n3\n1791500000\n00000000000000000000000000000003\n57cd05f2ce3e5ca23f5179d06791b30ccc34ed95638448cec5b9e08e4e548802","208d965a3cb55f04b1d2ed66922674e38ba51c2da39444027b5c2d6e93a32627"},
};
