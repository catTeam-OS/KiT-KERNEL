#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int main(void)
{
    const char *host = "example.com";
    const char *ca = "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem";

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    if (!SSL_CTX_load_verify_locations(ctx, ca, NULL)) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    SSL *ssl = SSL_new(ctx);
    BIO *bio = BIO_new_ssl_connect(ctx);
    if (!bio) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return 1;
    }

    BIO_get_ssl(bio, &ssl);
    SSL_set_tlsext_host_name(ssl, host);
    SSL_set1_host(ssl, host);
    BIO_set_conn_hostname(bio, "example.com:443");

    if (BIO_do_connect(bio) <= 0 || BIO_do_handshake(bio) <= 0) {
        ERR_print_errors_fp(stderr);
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return 1;
    }

    printf("TLS handshake successful.\n");
    printf("Certificate verification: %s\n",
           SSL_get_verify_result(ssl) == X509_V_OK ? "PASSED" : "FAILED");

    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    return 0;
}
