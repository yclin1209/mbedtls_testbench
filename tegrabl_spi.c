/*
 *  Copyright (c) 2020, Macronix, All rights reserved. 
 *  Copyright (c) 2020, Nvidia, All rights reserved.
 */

#if 0


#include <string.h>
#include <stdlib.h>
#include <tegrabl_debug.h>



#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md2.h"
#include "mbedtls/md4.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/aes.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/timing.h"

#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
//#define mbedtls_calloc     calloc
//#define mbedtls_free       free
//#define mbedtls_exit       exit
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif


#if defined MBEDTLS_SELF_TEST
/* Sanity check for malloc. This is not expected to fail, and is rather
 * intended to display potentially useful information about the platform,
 * in particular the behavior of malloc(0). */
static int calloc_self_test( int verbose )
{
    int failures = 0;
    void *empty1 = mbedtls_calloc( 0, 1 );
    void *empty2 = mbedtls_calloc( 0, 1 );
    void *buffer1 = mbedtls_calloc( 1, 1 );
    void *buffer2 = mbedtls_calloc( 1, 1 );
    uintptr_t old_buffer1;

    if( empty1 == NULL && empty2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (NULL)\n" );
    }
    else if( empty1 == NULL || empty2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): failed (mix of NULL and non-NULL)\n" );
        ++failures;
    }
    else if( empty1 == empty2 )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (same non-null)\n" );
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (distinct non-null)\n" );
    }

    if( buffer1 == NULL || buffer2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): failed (NULL)\n" );
        ++failures;
    }
    else if( buffer1 == buffer2 )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): failed (same buffer twice)\n" );
        ++failures;
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): passed\n" );
    }

    old_buffer1 = (uintptr_t) buffer1;
    mbedtls_free( buffer1 );
    buffer1 = mbedtls_calloc( 1, 1 );
    if( buffer1 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1 again): failed (NULL)\n" );
        ++failures;
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1 again): passed (%s address)\n",
                            (uintptr_t) old_buffer1 == (uintptr_t) buffer1 ?
                            "same" : "different" );
    }

    if( verbose )
        mbedtls_printf( "\n" );
    mbedtls_free( empty1 );
    mbedtls_free( empty2 );
    mbedtls_free( buffer1 );
    mbedtls_free( buffer2 );
    return( failures );
}
#endif /* MBEDTLS_SELF_TEST */

static int test_snprintf( size_t n, const char ref_buf[10], int ref_ret )
{
    int ret;
    char buf[10] = "xxxxxxxxx";
    const char ref[10] = "xxxxxxxxx";

    ret = mbedtls_snprintf( buf, n, "%s", "123" );
    if( ret < 0 || (size_t) ret >= n )
        ret = -1;

    if( strncmp( ref_buf, buf, sizeof( buf ) ) != 0 ||
        ref_ret != ret ||
        memcmp( buf + n, ref + n, sizeof( buf ) - n ) != 0 )
    {
        return( 1 );
    }

    return( 0 );
}

static int run_test_snprintf( void )
{
    return( test_snprintf( 0, "xxxxxxxxx",  -1 ) != 0 ||
            test_snprintf( 1, "",           -1 ) != 0 ||
            test_snprintf( 2, "1",          -1 ) != 0 ||
            test_snprintf( 3, "12",         -1 ) != 0 ||
            test_snprintf( 4, "123",         3 ) != 0 ||
            test_snprintf( 5, "123",         3 ) != 0 );
}

/*
 * Check if a seed file is present, and if not create one for the entropy
 * self-test. If this fails, we attempt the test anyway, so no error is passed
 * back.
 */
#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_ENTROPY_C)
#if defined(MBEDTLS_ENTROPY_NV_SEED) && !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
static void create_entropy_seed_file( void )
{
    int result;
    size_t output_len = 0;
    unsigned char seed_value[MBEDTLS_ENTROPY_BLOCK_SIZE];

    /* Attempt to read the entropy seed file. If this fails - attempt to write
     * to the file to ensure one is present. */
    result = mbedtls_platform_std_nv_seed_read( seed_value,
                                                    MBEDTLS_ENTROPY_BLOCK_SIZE );
    if( 0 == result )
        return;

    result = mbedtls_platform_entropy_poll( NULL,
                                            seed_value,
                                            MBEDTLS_ENTROPY_BLOCK_SIZE,
                                            &output_len );
    if( 0 != result )
        return;

    if( MBEDTLS_ENTROPY_BLOCK_SIZE != output_len )
        return;

    mbedtls_platform_std_nv_seed_write( seed_value, MBEDTLS_ENTROPY_BLOCK_SIZE );
}
#endif

int mbedtls_entropy_self_test_wrapper( int verbose )
{
#if defined(MBEDTLS_ENTROPY_NV_SEED) && !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
    create_entropy_seed_file( );
#endif
    return( mbedtls_entropy_self_test( verbose ) );
}
#endif

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
int mbedtls_memory_buffer_alloc_free_and_self_test( int verbose )
{
    if( verbose != 0 )
    {
#if defined(MBEDTLS_MEMORY_DEBUG)
        mbedtls_memory_buffer_alloc_status( );
#endif
    }
    mbedtls_memory_buffer_alloc_free( );
    return( mbedtls_memory_buffer_alloc_self_test( verbose ) );
}
#endif

typedef struct
{
    const char *name;
    int ( *function )( int );
} selftest_t;

const selftest_t selftests[] =
{
    {"calloc", calloc_self_test},
#if defined(MBEDTLS_MD2_C)
    {"md2", mbedtls_md2_self_test},
#endif
#if defined(MBEDTLS_MD4_C)
    {"md4", mbedtls_md4_self_test},
#endif
#if defined(MBEDTLS_MD5_C)
    {"md5", mbedtls_md5_self_test},
#endif
#if defined(MBEDTLS_RIPEMD160_C)
//    {"ripemd160", mbedtls_ripemd160_self_test},
#endif
#if defined(MBEDTLS_SHA1_C)
    {"sha1", mbedtls_sha1_self_test},
#endif
#if defined(MBEDTLS_SHA256_C)
    {"sha256", mbedtls_sha256_self_test},
#endif
#if defined(MBEDTLS_SHA512_C)
    {"sha512", mbedtls_sha512_self_test},
#endif
#if defined(MBEDTLS_ARC4_C)
//    {"arc4", mbedtls_arc4_self_test},
#endif
#if defined(MBEDTLS_DES_C)
//    {"des", mbedtls_des_self_test},
#endif
#if defined(MBEDTLS_AES_C)
    {"aes", mbedtls_aes_self_test},
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_AES_C)
    {"gcm", mbedtls_gcm_self_test},
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_AES_C)
//    {"ccm", mbedtls_ccm_self_test},
#endif
#if defined(MBEDTLS_NIST_KW_C) && defined(MBEDTLS_AES_C)
//    {"nist_kw", mbedtls_nist_kw_self_test},
#endif
#if defined(MBEDTLS_CMAC_C)
//    {"cmac", mbedtls_cmac_self_test},
#endif
#if defined(MBEDTLS_CHACHA20_C)
//    {"chacha20", mbedtls_chacha20_self_test},
#endif
#if defined(MBEDTLS_POLY1305_C)
//    {"poly1305", mbedtls_poly1305_self_test},
#endif
#if defined(MBEDTLS_CHACHAPOLY_C)
//    {"chacha20-poly1305", mbedtls_chachapoly_self_test},
#endif
#if defined(MBEDTLS_BASE64_C)
//    {"base64", mbedtls_base64_self_test},
#endif
#if defined(MBEDTLS_BIGNUM_C)
    {"mpi", mbedtls_mpi_self_test},
#endif
#if defined(MBEDTLS_RSA_C)
//    {"rsa", mbedtls_rsa_self_test},
#endif
#if defined(MBEDTLS_X509_USE_C)
//    {"x509", mbedtls_x509_self_test},
#endif
#if defined(MBEDTLS_XTEA_C)
//    {"xtea", mbedtls_xtea_self_test},
#endif
#if defined(MBEDTLS_CAMELLIA_C)
//    {"camellia", mbedtls_camellia_self_test},
#endif
#if defined(MBEDTLS_ARIA_C)
//    {"aria", mbedtls_aria_self_test},
#endif
#if defined(MBEDTLS_CTR_DRBG_C)
//    {"ctr_drbg", mbedtls_ctr_drbg_self_test},
#endif
#if defined(MBEDTLS_HMAC_DRBG_C)
    {"hmac_drbg", mbedtls_hmac_drbg_self_test},
#endif
#if defined(MBEDTLS_ECP_C)
    {"ecp", mbedtls_ecp_self_test},
#endif
#if defined(MBEDTLS_ECJPAKE_C)
//    {"ecjpake", mbedtls_ecjpake_self_test},
#endif
#if defined(MBEDTLS_DHM_C)
//    {"dhm", mbedtls_dhm_self_test},
#endif
#if defined(MBEDTLS_ENTROPY_C)
//    {"entropy", mbedtls_entropy_self_test_wrapper},
#endif
#if defined(MBEDTLS_PKCS5_C)
//    {"pkcs5", mbedtls_pkcs5_self_test},
#endif
/* Slower test after the faster ones */
#if defined(MBEDTLS_TIMING_C)
//    {"timing", mbedtls_timing_self_test},
#endif
/* Heap test comes last */
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
//    {"memory_buffer_alloc", mbedtls_memory_buffer_alloc_free_and_self_test},
#endif
    {NULL, NULL}
};
#endif /* MBEDTLS_SELF_TEST */

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
int mbedtls_hardware_poll( void *data, unsigned char *output,
                           size_t len, size_t *olen )
{
    size_t i;
    (void) data;
    for( i = 0; i < len; ++i )
        output[i] = rand();
    *olen = len;
    return( 0 );
}
#endif

//#define mbedtls_printf        tegrabl_printf

//int main( int argc, char *argv[] )
void selftest(int argc, char *argv[])
{
#if defined(MBEDTLS_SELF_TEST)
    const selftest_t *test;
#endif /* MBEDTLS_SELF_TEST */
    char **argp;
    int v = 1; /* v=1 for verbose mode */
    int exclude_mode = 0;
    int suites_tested = 0, suites_failed = 0;
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C) && defined(MBEDTLS_SELF_TEST)
    unsigned char buf[1000000];
#endif
    void *pointer;

//tegrabl_printf("===1===\n");

    /*
     * The C standard doesn't guarantee that all-bits-0 is the representation
     * of a NULL pointer. We do however use that in our code for initializing
     * structures, which should work on every modern platform. Let's be sure.
     */
    memset( &pointer, 0, sizeof( void * ) );
    if( pointer != NULL )
    {
        mbedtls_printf( "all-bits-zero is not a NULL pointer\n" );
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );
    }
//tegrabl_printf("===2===\n");
    /*
     * Make sure we have a snprintf that correctly zero-terminates
     */
    if( run_test_snprintf() != 0 )
    {
        mbedtls_printf( "the snprintf implementation is broken\n" );
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );
    }
//tegrabl_printf("===3===\n");
    for( argp = argv + ( argc >= 1 ? 1 : argc ); *argp != NULL; ++argp )
    {
        if( strcmp( *argp, "--quiet" ) == 0 ||
            strcmp( *argp, "-q" ) == 0 )
        {
            v = 0;
        }
        else if( strcmp( *argp, "--exclude" ) == 0 ||
                 strcmp( *argp, "-x" ) == 0 )
        {
            exclude_mode = 1;
        }
        else
            break;
    }
//tegrabl_printf("===4===\n");
    if( v != 0 )
        mbedtls_printf( "\n" );

#if defined(MBEDTLS_SELF_TEST)

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    mbedtls_memory_buffer_alloc_init( buf, sizeof(buf) );
#endif
//tegrabl_printf("===5===\n");
    if( *argp != NULL && exclude_mode == 0 )
    {
//tegrabl_printf("===6===\n");
        /* Run the specified tests */
        for( ; *argp != NULL; argp++ )
        {
            for( test = selftests; test->name != NULL; test++ )
            {
                if( !strcmp( *argp, test->name ) )
                {
                    if( test->function( v )  != 0 )
                    {
                        suites_failed++;
                    }
                    suites_tested++;
                    break;
                }
            }
            if( test->name == NULL )
            {
                mbedtls_printf( "  Test suite %s not available -> failed\n\n", *argp );
                suites_failed++;
            }
        }
    }
    else
    {
//tegrabl_printf("===7===\n");
        /* Run all the tests except excluded ones */
        for( test = selftests; test->name != NULL; test++ )
        {
//tegrabl_printf("===test->name:%s ===\n",test->name);
//tegrabl_printf("===7-1===\n");
            if( exclude_mode )
            {
//tegrabl_printf("===7-2===\n");
                char **excluded;
                for( excluded = argp; *excluded != NULL; ++excluded )
                {
                    if( !strcmp( *excluded, test->name ) )
                        break;
                }
                if( *excluded )
                {
                    if( v )
                        mbedtls_printf( "  Skip: %s\n", test->name );
                    continue;
                }
            }
//tegrabl_printf("===7-3===\n");
            if( test->function( v )  != 0 )
            {
//tegrabl_printf("===7-4===\n");
                suites_failed++;
            }
            suites_tested++;
        }
    }
//tegrabl_printf("===8===\n");
#else
    (void) exclude_mode;
    mbedtls_printf( " MBEDTLS_SELF_TEST not defined.\n" );
#endif

    if( v != 0 )
    {
//tegrabl_printf("===9===\n");
        mbedtls_printf( "  Executed %d test suites\n\n", suites_tested );

        if( suites_failed > 0)
        {
            mbedtls_printf( "  [ %d tests FAIL ]\n\n", suites_failed );
        }
        else
        {
            mbedtls_printf( "  [ All tests PASS ]\n\n" );
        }
    }
//tegrabl_printf("===10===\n");
    if( suites_failed > 0)
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );
//tegrabl_printf("===11===\n");
    /* return() is here to prevent compiler warnings */
    //return( MBEDTLS_EXIT_SUCCESS );
}


tegrabl_error_t tegrabl_spi_open(void)
{
	char arg0[] = "selftest";
	char *argv[] = { &arg0[0],NULL };
	int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

	tegrabl_printf("==== mbedtls selftest START===\n");
	
	selftest(argc,&argv[0]);	

	tegrabl_printf("==== mbedtls selftest END===\n");
	return 1;
}

#else


#include <tegrabl_debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mbedtls/cipher.h"
#include "mbedtls/md.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <tegrabl_malloc.h>
#include <sys/time.h>


extern time_t tegrabl_get_timestamp_us(void);

#define assert_exit(cond, ret) \
    do { if (!(cond)) { \
        printf("  !. assert: failed [line: %d, error: -0x%04X]\n", __LINE__, -ret); \
        goto cleanup; \
    } } while (0)


static uint8_t key[] =
{
    0xc9, 0x39, 0xcc, 0x13, 0x39, 0x7c, 0x1d, 0x37,
    0xde, 0x6a, 0xe0, 0xe1, 0xcb, 0x7c, 0x42, 0x3c
};

static uint8_t iv[] =
{
    0xb3, 0xd8, 0xcc, 0x01, 0x7c, 0xbb, 0x89, 0xb3,
    0x9e, 0x0f, 0x67, 0xe2,
};

static uint8_t pt[] =
{
    0xc3, 0xb3, 0xc4, 0x1f, 0x11, 0x3a, 0x31, 0xb7, 
    0x3d, 0x9a, 0x5c, 0xd4, 0x32, 0x10, 0x30, 0x69
};

static uint8_t add[] = 
{
    0x24, 0x82, 0x56, 0x02, 0xbd, 0x12, 0xa9, 0x84, 
    0xe0, 0x09, 0x2d, 0x3e, 0x44, 0x8e, 0xda, 0x5f
};

static uint8_t ct[] =
{
    0x93, 0xfe, 0x7d, 0x9e, 0x9b, 0xfd, 0x10, 0x34, 
    0x8a, 0x56, 0x06, 0xe5, 0xca, 0xfa, 0x73, 0x54
};

static uint8_t tag[] =
{
    0x00, 0x32, 0xa1, 0xdc, 0x85, 0xf1, 0xc9, 0x78, 
    0x69, 0x25, 0xa2, 0xe7, 0x1d, 0x82, 0x72, 0xdd
};

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    mbedtls_printf("%s", info);
    for (unsigned int i = 0; i < len; i++) {
        mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ",
                        buf[i], i == len - 1 ? "\n":"");
    }
}
static int entropy_source(void *data, uint8_t *output, size_t len, size_t *olen)
{
    uint32_t seed;

    //seed = sys_rand32_get();
	//time_t t = tegrabl_get_timestamp_us();  // get uptime in nanoseconds
  	// tv->tv_sec = t / 1000000;  // convert to seconds

	srand( (unsigned int) tegrabl_get_timestamp_us() );
	seed = rand(); //| rand();	// 32bits of rand num
    if (len > sizeof(seed)) {
        len = sizeof(seed);
    }

    memcpy(output, &seed, len);

    *olen = len;
    return 0;
}


tegrabl_error_t tegrabl_spi_open(void)
{
	//
	// 1. AES-128-GCM
	//
 	int ret=0;
    size_t len;
    uint8_t buf[16], tag_buf[16];

    mbedtls_cipher_context_t c_ctx;
    const mbedtls_cipher_info_t *gcm_info;

    mbedtls_cipher_init(&c_ctx);
    gcm_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_GCM);

    mbedtls_cipher_setup(&c_ctx, gcm_info);
    mbedtls_printf("\n  cipher info setup, name: %s, block size: %d\n", 
                        mbedtls_cipher_get_name(&c_ctx), 
                        mbedtls_cipher_get_block_size(&c_ctx));

    mbedtls_cipher_setkey(&c_ctx, key, sizeof(key)*8, MBEDTLS_ENCRYPT);

    ret = mbedtls_cipher_auth_encrypt(&c_ctx, iv, sizeof(iv), add, sizeof(add),
                                        pt, sizeof(pt), buf, &len, tag_buf, 16);
    assert_exit(ret == 0, ret);
    assert_exit(memcmp(buf, ct, sizeof(ct)) == 0, -1);
    assert_exit(memcmp(tag_buf, tag, 16) == 0, -1);
    dump_buf("\n  cipher gcm auth encrypt:", buf, 16);
    dump_buf("\n  cipher gcm auth tag:", tag_buf, 16);

    mbedtls_cipher_setkey(&c_ctx, key, sizeof(key)*8, MBEDTLS_DECRYPT);
    ret = mbedtls_cipher_auth_decrypt(&c_ctx, iv, sizeof(iv), add, sizeof(add),
                                        ct, sizeof(ct), buf, &len, tag, 16);
    assert_exit(ret == 0, ret);
    assert_exit(memcmp(buf, pt, sizeof(pt)) == 0, -1);                                                                                                           
    dump_buf("\n  cipher gcm auth decrypt:", buf, 16);

//cleanup_1:
    mbedtls_cipher_free(&c_ctx);

	//
	// 2. HMAC-SHA256
	//

    uint8_t mac[32];
    char *secret = "Jefe";
    char *msg = "what do ya want for nothing?";

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;

    //mbedtls_platform_set_printf(printf);

    mbedtls_md_init(&ctx);
    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_printf("\n  md info setup, name: %s, digest size: %d\n", 
                   mbedtls_md_get_name(info), mbedtls_md_get_size(info));

    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)secret, strlen(secret));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)msg, strlen(msg));
    mbedtls_md_hmac_finish(&ctx, mac);

    dump_buf("\n  md hmac-sha-256 mac:", mac, sizeof(mac));

    mbedtls_md_free(&ctx);

	//
	// 3. ECDH (SECP256R1)
	//

	//int ret = 0;
	ret = 0;
    size_t olen;
    char buf2[65];
    mbedtls_ecp_group grp;
    mbedtls_mpi cli_secret, srv_secret;
    mbedtls_mpi cli_pri, srv_pri;
    mbedtls_ecp_point cli_pub, srv_pub;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    //uint8_t *pers = "simple_ecdh";
	char *pers = "simple_ecdh";
    
    //mbedtls_platform_set_printf(printf);

    mbedtls_mpi_init(&cli_pri); 
    mbedtls_mpi_init(&srv_pri);
    mbedtls_mpi_init(&cli_secret); 
    mbedtls_mpi_init(&srv_secret);
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&cli_pub); 
    mbedtls_ecp_point_init(&srv_pub);
    mbedtls_entropy_init(&entropy); 
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                       MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, 
                                (const uint8_t *) pers, strlen((const char *) pers));
    mbedtls_printf("\n  . setup rng ... ok\n");

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_printf("\n  . select ecp group SECP256R1 ... ok\n");

    ret = mbedtls_ecdh_gen_public(&grp, &cli_pri, &cli_pub, 
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&grp, &cli_pub, 
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, (uint8_t *)buf2, sizeof(buf2));
    dump_buf("  1. ecdh client generate public parameter:",(uint8_t *) buf2, olen);

    ret = mbedtls_ecdh_gen_public(&grp, &srv_pri, &srv_pub, 
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&grp, &srv_pub, 
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, (uint8_t *)buf2, sizeof(buf2));
    dump_buf("  2. ecdh server generate public parameter:", (uint8_t *)buf2, olen);

    ret = mbedtls_ecdh_compute_shared(&grp, &cli_secret, &srv_pub, &cli_pri, 
                                        mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_mpi_write_binary(&cli_secret, (uint8_t *)buf2, mbedtls_mpi_size(&cli_secret));
    dump_buf("  3. ecdh client generate secret:", (uint8_t *)buf2, mbedtls_mpi_size(&cli_secret));

    ret = mbedtls_ecdh_compute_shared(&grp, &srv_secret, &cli_pub, &srv_pri, 
                                        mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_mpi_write_binary(&srv_secret, (uint8_t *)buf2, mbedtls_mpi_size(&srv_secret));
    dump_buf("  4. ecdh server generate secret:", (uint8_t *)buf2, mbedtls_mpi_size(&srv_secret));

    ret = mbedtls_mpi_cmp_mpi(&cli_secret, &srv_secret);
    assert_exit(ret == 0, ret);
    mbedtls_printf("  5. ecdh checking secrets ... ok\n");

cleanup:
    mbedtls_mpi_free(&cli_pri); 
    mbedtls_mpi_free(&srv_pri);
    mbedtls_mpi_free(&cli_secret); 
    mbedtls_mpi_free(&srv_secret);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&cli_pub); 
    mbedtls_ecp_point_free(&srv_pub);
    mbedtls_entropy_free(&entropy); 
    mbedtls_ctr_drbg_free(&ctr_drbg);

	return 0;

}

#endif
