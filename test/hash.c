#include "../include/siphon/hash.h"
#include "../include/siphon/alloc.h"
#include "mu/mu.h"

static const char * metro_key_63 = "012345678901234567890123456789012345678901234567890123456789012";

static const uint8_t sip_vectors[64][8] = {
	{ 0x31, 0x0e, 0x0e, 0xdd, 0x47, 0xdb, 0x6f, 0x72, },
	{ 0xfd, 0x67, 0xdc, 0x93, 0xc5, 0x39, 0xf8, 0x74, },
	{ 0x5a, 0x4f, 0xa9, 0xd9, 0x09, 0x80, 0x6c, 0x0d, },
	{ 0x2d, 0x7e, 0xfb, 0xd7, 0x96, 0x66, 0x67, 0x85, },
	{ 0xb7, 0x87, 0x71, 0x27, 0xe0, 0x94, 0x27, 0xcf, },
	{ 0x8d, 0xa6, 0x99, 0xcd, 0x64, 0x55, 0x76, 0x18, },
	{ 0xce, 0xe3, 0xfe, 0x58, 0x6e, 0x46, 0xc9, 0xcb, },
	{ 0x37, 0xd1, 0x01, 0x8b, 0xf5, 0x00, 0x02, 0xab, },
	{ 0x62, 0x24, 0x93, 0x9a, 0x79, 0xf5, 0xf5, 0x93, },
	{ 0xb0, 0xe4, 0xa9, 0x0b, 0xdf, 0x82, 0x00, 0x9e, },
	{ 0xf3, 0xb9, 0xdd, 0x94, 0xc5, 0xbb, 0x5d, 0x7a, },
	{ 0xa7, 0xad, 0x6b, 0x22, 0x46, 0x2f, 0xb3, 0xf4, },
	{ 0xfb, 0xe5, 0x0e, 0x86, 0xbc, 0x8f, 0x1e, 0x75, },
	{ 0x90, 0x3d, 0x84, 0xc0, 0x27, 0x56, 0xea, 0x14, },
	{ 0xee, 0xf2, 0x7a, 0x8e, 0x90, 0xca, 0x23, 0xf7, },
	{ 0xe5, 0x45, 0xbe, 0x49, 0x61, 0xca, 0x29, 0xa1, },
	{ 0xdb, 0x9b, 0xc2, 0x57, 0x7f, 0xcc, 0x2a, 0x3f, },
	{ 0x94, 0x47, 0xbe, 0x2c, 0xf5, 0xe9, 0x9a, 0x69, },
	{ 0x9c, 0xd3, 0x8d, 0x96, 0xf0, 0xb3, 0xc1, 0x4b, },
	{ 0xbd, 0x61, 0x79, 0xa7, 0x1d, 0xc9, 0x6d, 0xbb, },
	{ 0x98, 0xee, 0xa2, 0x1a, 0xf2, 0x5c, 0xd6, 0xbe, },
	{ 0xc7, 0x67, 0x3b, 0x2e, 0xb0, 0xcb, 0xf2, 0xd0, },
	{ 0x88, 0x3e, 0xa3, 0xe3, 0x95, 0x67, 0x53, 0x93, },
	{ 0xc8, 0xce, 0x5c, 0xcd, 0x8c, 0x03, 0x0c, 0xa8, },
	{ 0x94, 0xaf, 0x49, 0xf6, 0xc6, 0x50, 0xad, 0xb8, },
	{ 0xea, 0xb8, 0x85, 0x8a, 0xde, 0x92, 0xe1, 0xbc, },
	{ 0xf3, 0x15, 0xbb, 0x5b, 0xb8, 0x35, 0xd8, 0x17, },
	{ 0xad, 0xcf, 0x6b, 0x07, 0x63, 0x61, 0x2e, 0x2f, },
	{ 0xa5, 0xc9, 0x1d, 0xa7, 0xac, 0xaa, 0x4d, 0xde, },
	{ 0x71, 0x65, 0x95, 0x87, 0x66, 0x50, 0xa2, 0xa6, },
	{ 0x28, 0xef, 0x49, 0x5c, 0x53, 0xa3, 0x87, 0xad, },
	{ 0x42, 0xc3, 0x41, 0xd8, 0xfa, 0x92, 0xd8, 0x32, },
	{ 0xce, 0x7c, 0xf2, 0x72, 0x2f, 0x51, 0x27, 0x71, },
	{ 0xe3, 0x78, 0x59, 0xf9, 0x46, 0x23, 0xf3, 0xa7, },
	{ 0x38, 0x12, 0x05, 0xbb, 0x1a, 0xb0, 0xe0, 0x12, },
	{ 0xae, 0x97, 0xa1, 0x0f, 0xd4, 0x34, 0xe0, 0x15, },
	{ 0xb4, 0xa3, 0x15, 0x08, 0xbe, 0xff, 0x4d, 0x31, },
	{ 0x81, 0x39, 0x62, 0x29, 0xf0, 0x90, 0x79, 0x02, },
	{ 0x4d, 0x0c, 0xf4, 0x9e, 0xe5, 0xd4, 0xdc, 0xca, },
	{ 0x5c, 0x73, 0x33, 0x6a, 0x76, 0xd8, 0xbf, 0x9a, },
	{ 0xd0, 0xa7, 0x04, 0x53, 0x6b, 0xa9, 0x3e, 0x0e, },
	{ 0x92, 0x59, 0x58, 0xfc, 0xd6, 0x42, 0x0c, 0xad, },
	{ 0xa9, 0x15, 0xc2, 0x9b, 0xc8, 0x06, 0x73, 0x18, },
	{ 0x95, 0x2b, 0x79, 0xf3, 0xbc, 0x0a, 0xa6, 0xd4, },
	{ 0xf2, 0x1d, 0xf2, 0xe4, 0x1d, 0x45, 0x35, 0xf9, },
	{ 0x87, 0x57, 0x75, 0x19, 0x04, 0x8f, 0x53, 0xa9, },
	{ 0x10, 0xa5, 0x6c, 0xf5, 0xdf, 0xcd, 0x9a, 0xdb, },
	{ 0xeb, 0x75, 0x09, 0x5c, 0xcd, 0x98, 0x6c, 0xd0, },
	{ 0x51, 0xa9, 0xcb, 0x9e, 0xcb, 0xa3, 0x12, 0xe6, },
	{ 0x96, 0xaf, 0xad, 0xfc, 0x2c, 0xe6, 0x66, 0xc7, },
	{ 0x72, 0xfe, 0x52, 0x97, 0x5a, 0x43, 0x64, 0xee, },
	{ 0x5a, 0x16, 0x45, 0xb2, 0x76, 0xd5, 0x92, 0xa1, },
	{ 0xb2, 0x74, 0xcb, 0x8e, 0xbf, 0x87, 0x87, 0x0a, },
	{ 0x6f, 0x9b, 0xb4, 0x20, 0x3d, 0xe7, 0xb3, 0x81, },
	{ 0xea, 0xec, 0xb2, 0xa3, 0x0b, 0x22, 0xa8, 0x7f, },
	{ 0x99, 0x24, 0xa4, 0x3c, 0xc1, 0x31, 0x57, 0x24, },
	{ 0xbd, 0x83, 0x8d, 0x3a, 0xaf, 0xbf, 0x8d, 0xb7, },
	{ 0x0b, 0x1a, 0x2a, 0x32, 0x65, 0xd5, 0x1a, 0xea, },
	{ 0x13, 0x50, 0x79, 0xa3, 0x23, 0x1c, 0xe6, 0x60, },
	{ 0x93, 0x2b, 0x28, 0x46, 0xe4, 0xd7, 0x06, 0x66, },
	{ 0xe1, 0x91, 0x5f, 0x5c, 0xb1, 0xec, 0xa4, 0x6c, },
	{ 0xf3, 0x25, 0x96, 0x5c, 0xa1, 0x6d, 0x62, 0x9f, },
	{ 0x57, 0x5f, 0xf2, 0x8e, 0x60, 0x38, 0x1b, 0xe5, },
	{ 0x72, 0x45, 0x06, 0xeb, 0x4c, 0x32, 0x8a, 0x95, }
};

static const uint64_t xx64_vectors[64] = {
	0xf6e21e93442c06eb, 0x15b0bc23c6c631d5, 0xf56652a5bca27821, 0x543035237b13acd0,
	0xf2ed85bffbe88262, 0x6e1f7a8529627a3b, 0xbc749625f0061635, 0x7a99215b2e5aecc6,
	0xf50cb48908fc93d8, 0x8537622d94e90907, 0xf185012a11473238, 0xb7652fc64e8d913b,
	0x3ed289036ab94556, 0xb6adef33d29c64cb, 0x76a672373d6948e3, 0x5d24396ad1098b98,
	0xc953118d5ede9eed, 0x053199575e7df135, 0xf0df4dd87ff82429, 0xba067451dea25511,
	0x674a592be9e79f6f, 0xd241d9b4fece6aa8, 0x673a1ffd2bb634dd, 0x4d359aa1d458e385,
	0x83b63d6fa38ed56a, 0xb5e2041b65106238, 0x0e4f250db3c49bfd, 0x7e8cfb7d10a271a9,
	0x825ee1972909d0a3, 0x0a71ac283de2a941, 0x2ca12b792235f3a1, 0xb7d66086a3541b71,
	0x637f030da0cfad35, 0x0be79827d61ea96e, 0x65ef3a88be97669d, 0xb960acb73f11fc2e,
	0x70bc3aa8979b93a0, 0xd34c8030cb86833e, 0xe63ed0fe0e2d1b04, 0xd0679007d4e48f7e,
	0x174fcaa726046763, 0x1e1469904dda801a, 0x82dc134e1e26df21, 0x83b6ec3c58f97f4c,
	0x2aebf4d15b594f0d, 0x0d3199fd65700d13, 0x2d9b33e8fdec6be5, 0x8a5ebb0e396151ee,
	0x7f15cd0792ac9e3e, 0x9bbe7cb17ea90e19, 0x9935abf435b7f4fe, 0xabe757bdcd05f1dc,
	0x124a06a66f16cbe1, 0x667093a71c06a2e2, 0x8d913165b8f7c8d6, 0xc9c226d131ca832c,
	0x1da54234fa0faf22, 0x4b876cef4bf829cc, 0xd597a9953619e1f3, 0x2f543d947c7f7d3b,
	0x14b7818f92c4a477, 0x7b00cdf7c4d36437, 0x31f2acec6d85d06f, 0xa1de1074e300a95c
};

static void
test_metrohash (void)
{
	SpSeed s0 = { .u32 = 0 };
	SpSeed s1 = { .u32 = 1 };
	mu_assert_uint_eq (sp_metrohash64 (metro_key_63, 63, &s0), 0x400E735C4F048F65);
	mu_assert_uint_eq (sp_metrohash64 (metro_key_63, 63, &s1), 0x7B5356A8B0EB49AE);
}

static void
test_siphash (void)
{
	SpSeed seed = { .u128 = { 506097522914230528, 1084818905618843912 } };
	uint8_t in[sp_len (sip_vectors)] = { 0 };
	unsigned i;

	for (i = 0; i < sp_len (sip_vectors); ++i) {
		in[i] = i;
		uint64_t out = sp_siphash (in, i, &seed);
		mu_assert_msg (memcmp (&out, sip_vectors[i], 8) == 0, "siphash vector failed for %d bytes\n", i );
	}
}

static void
test_siphash_case (void)
{
	mu_assert_uint_eq (
			sp_siphash_case ("test", 4, SP_SEED_DEFAULT),
			sp_siphash_case ("TEST", 4, SP_SEED_DEFAULT));

	mu_assert_uint_eq (
			sp_siphash_case ("test", 4, SP_SEED_DEFAULT),
			sp_siphash_case ("Test", 4, SP_SEED_DEFAULT));

	mu_assert_uint_eq (
			sp_siphash_case ("test", 4, SP_SEED_DEFAULT),
			sp_siphash_case ("TesT", 4, SP_SEED_DEFAULT));

	mu_assert_uint_eq (
			sp_siphash_case ("longervalue", 11, SP_SEED_DEFAULT),
			sp_siphash_case ("LONGERVALUE", 11, SP_SEED_DEFAULT));

	mu_assert_uint_eq (
			sp_siphash_case ("longervalue", 11, SP_SEED_DEFAULT),
			sp_siphash_case ("LongerValue", 11, SP_SEED_DEFAULT));
}

static void
test_xx64 (void)
{
	SpSeed seed = { .u128 = { 506097522914230528, 1084818905618843912 } };
	uint8_t in[sp_len (xx64_vectors)] = { 0 };
	unsigned i;

	for (i = 0; i < sp_len (xx64_vectors); ++i) {
		in[i] = i;
		uint64_t out = sp_xxhash64 (in, i, &seed);
		mu_assert_uint_eq (xx64_vectors[i], out);
	}
}

int
main (void)
{
	mu_init ("hash");
	
	test_metrohash ();
	test_siphash ();
	test_siphash_case ();
	test_xx64 ();

	mu_assert (sp_alloc_summary ());
}

