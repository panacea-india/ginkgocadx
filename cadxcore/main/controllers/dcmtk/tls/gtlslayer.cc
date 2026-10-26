/*
 *
 *  $Id$
 *  Ginkgo CADx Project
 *
 *  Copyright 2008-14 MetaEmotion S.L. All rights reserved.
 *  http://ginkgo-cadx.com
 *
 *  This file is licensed under LGPL v3 license.
 *  See License.txt for details
 *
 */
#include "tls.h"
#include "gtlslayer.h"
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

BEGIN_EXTERN_C
#ifdef HAVE_WINDOWS_H
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>
#include <winbase.h>
#endif
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
END_EXTERN_C


#include "dcmtk/dcmtls/tlstrans.h"
#include "dcmtk/dcmnet/dicom.h"

GTLSTransportLayer::~GTLSTransportLayer()
{

}

OFCondition GTLSTransportLayer::setPrivateKeyFromString(const std::string& keyStr)
{
        SSL_CTX *ctx = getNativeHandle();
        if (ctx == NULL) {
                return makeOFCondition(OFM_dcmtls, 1, OF_error, "TLS context not available");
        }

        BIO *bio = BIO_new_mem_buf(keyStr.c_str(), static_cast<int>(keyStr.size()));
        if (bio == NULL) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }
        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (pkey == NULL) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }

        // SSL_CTX_use_PrivateKey takes its own reference to the key
        int ret = SSL_CTX_use_PrivateKey(ctx, pkey);
        EVP_PKEY_free(pkey);
        if (ret <= 0) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }
        return EC_Normal;
}

OFCondition GTLSTransportLayer::setCertificateFromString(const std::string& certStr)
{
        SSL_CTX *ctx = getNativeHandle();
        if (ctx == NULL) {
                return makeOFCondition(OFM_dcmtls, 1, OF_error, "TLS context not available");
        }

        BIO *bio = BIO_new_mem_buf(certStr.c_str(), static_cast<int>(certStr.size()));
        if (bio == NULL) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }
        X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (cert == NULL) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }

        // SSL_CTX_use_certificate takes its own reference to the certificate
        int ret = SSL_CTX_use_certificate(ctx, cert);
        X509_free(cert);
        if (ret <= 0) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }
        return EC_Normal;
}

#ifdef _WIN32
//agrega los certificados de windows
void X509_STORE_load_windows_systemstore(X509_STORE *store)
{
        HCERTSTORE hStore;
        PCCERT_CONTEXT pContext = NULL;
        X509 *x509;

        hStore = CertOpenSystemStoreA(0, "ROOT");
        if(!hStore)
                return;

        while ( (pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL ) {
                x509 = NULL;
                x509 = d2i_X509(NULL, (const unsigned char**)(&pContext->pbCertEncoded), pContext->cbCertEncoded);
                if (x509) {
                        X509_STORE_add_cert(store, x509);
                        X509_free(x509);
                }
        }

        CertFreeCertificateContext(pContext);
        CertCloseStore(hStore, 0);
}
#endif

void LoadSystemTrustedCertificates(X509_STORE *store)
{
        X509_STORE_set_default_paths(store);
#ifdef _WIN32
        X509_STORE_load_windows_systemstore(store);
#endif
}

OFCondition GTLSTransportLayer::addSystemTrustedCertificates()
{
        SSL_CTX *ctx = getNativeHandle();
        if (ctx == NULL) {
                return makeOFCondition(OFM_dcmtls, 1, OF_error, "TLS context not available");
        }

        X509_STORE *almacen = SSL_CTX_get_cert_store(ctx);
        if (almacen == NULL) {
                return convertOpenSSLError(ERR_get_error(), OFTrue);
        }
        LoadSystemTrustedCertificates(almacen);
        return EC_Normal;
}
