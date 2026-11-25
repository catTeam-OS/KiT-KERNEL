#include <stdio.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

int main(void)
{
    int ret;
    const char *host = "example.com";
    const char *pers = "KiT HTTPS test";
    unsigned char buffer[4096];

    mbedtls_net_context server;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;

    mbedtls_net_init(&server);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_x509_crt_init(&cacert);

    ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char *)pers, strlen(pers));

    if (ret != 0) goto error;

    ret = mbedtls_x509_crt_parse_file(
        &cacert,
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem");

    if (ret < 0) goto error;

    ret = mbedtls_net_connect(
        &server, host, "443", MBEDTLS_NET_PROTO_TCP);

    if (ret != 0) goto error;

    ret = mbedtls_ssl_config_defaults(
        &conf, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);

    if (ret != 0) goto error;

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) goto error;

    ret = mbedtls_ssl_set_hostname(&ssl, host);
    if (ret != 0) goto error;

    mbedtls_ssl_set_bio(
        &ssl, &server,
        mbedtls_net_send, mbedtls_net_recv, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            goto error;
    }

    if (mbedtls_ssl_get_verify_result(&ssl) != 0) {
        fprintf(stderr, "Certificate verification failed.\n");
        ret = -1;
        goto cleanup;
    }

    printf("TLS handshake successful.\n");
    printf("Certificate verification: PASSED\n");

    const char request[] =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n\r\n";

    ret = mbedtls_ssl_write(
        &ssl, (const unsigned char *)request,
        sizeof(request) - 1);

    if (ret < 0) goto error;

    ret = mbedtls_ssl_read(&ssl, buffer, sizeof(buffer) - 1);

    if (ret > 0) {
        buffer[ret] = '\0';
        printf("\nHTTP response:\n%.500s\n", buffer);
    } else {
        goto error;
    }

    ret = 0;
    goto cleanup;

error:
    {
        char errbuf[256];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "mbedTLS error: %s (%d)\n", errbuf, ret);
    }

cleanup:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&server);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret != 0 ? 1 : 0;
}
