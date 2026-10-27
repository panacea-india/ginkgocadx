/*
 * This file is part of Ginkgo CADx
 *
 * Ginkgo CADx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with Ginkgo CADx; if not, see <http://www.gnu.org/licenses/>.
 *
 */

// GTLSTransportLayer loads the client certificate and key configured for a
// PACS from strings. Before 2026 it silently ignored them and reported
// success; these tests check that valid ones load and bad ones are rejected.

#include <gtest/gtest.h>

#include <main/controllers/dcmtk/tls/gtlslayer.h>
#include <dcmtk/dcmnet/assoc.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <string>

namespace
{

std::string KeyToPEM(EVP_PKEY* key)
{
        BIO* bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(bio, key, NULL, NULL, 0, NULL, NULL);
        char* data = NULL;
        const long len = BIO_get_mem_data(bio, &data);
        std::string pem(data, len);
        BIO_free(bio);
        return pem;
}

std::string SelfSignedCertificatePEM(EVP_PKEY* key)
{
        X509* cert = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
        X509_gmtime_adj(X509_getm_notBefore(cert), 0);
        X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
        X509_set_pubkey(cert, key);
        X509_NAME* name = X509_get_subject_name(cert);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("ginkgo-test"), -1, -1, 0);
        X509_set_issuer_name(cert, name);
        X509_sign(cert, key, EVP_sha256());

        BIO* bio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(bio, cert);
        char* data = NULL;
        const long len = BIO_get_mem_data(bio, &data);
        std::string pem(data, len);
        BIO_free(bio);
        X509_free(cert);
        return pem;
}

class TLSLayer : public ::testing::Test
{
protected:
        static void SetUpTestSuite()
        {
                key = EVP_RSA_gen(2048);
                otherKey = EVP_RSA_gen(2048);
                keyPEM = KeyToPEM(key);
                otherKeyPEM = KeyToPEM(otherKey);
                certPEM = SelfSignedCertificatePEM(key);
        }

        static void TearDownTestSuite()
        {
                EVP_PKEY_free(key);
                EVP_PKEY_free(otherKey);
        }

        static EVP_PKEY* key;
        static EVP_PKEY* otherKey;
        static std::string keyPEM;
        static std::string otherKeyPEM;
        static std::string certPEM;
};

EVP_PKEY* TLSLayer::key = NULL;
EVP_PKEY* TLSLayer::otherKey = NULL;
std::string TLSLayer::keyPEM;
std::string TLSLayer::otherKeyPEM;
std::string TLSLayer::certPEM;

}

TEST_F(TLSLayer, ValidCertificateAndKeyLoad)
{
        GTLSTransportLayer layer(DICOM_APPLICATION_REQUESTOR, NULL);
        EXPECT_TRUE(layer.setCertificateFromString(certPEM).good());
        EXPECT_TRUE(layer.setPrivateKeyFromString(keyPEM).good());
        EXPECT_TRUE(layer.checkPrivateKeyMatchesCertificate());
}

TEST_F(TLSLayer, GarbageCertificateIsRejected)
{
        GTLSTransportLayer layer(DICOM_APPLICATION_REQUESTOR, NULL);
        EXPECT_TRUE(layer.setCertificateFromString("not a certificate").bad());
}

TEST_F(TLSLayer, GarbageKeyIsRejected)
{
        GTLSTransportLayer layer(DICOM_APPLICATION_REQUESTOR, NULL);
        EXPECT_TRUE(layer.setPrivateKeyFromString("not a key").bad());
}

TEST_F(TLSLayer, KeyThatDoesNotMatchCertificateIsCaught)
{
        // OpenSSL may refuse the key outright, or accept it and fail the
        // match check; either way it must not look like a working pair.
        GTLSTransportLayer layer(DICOM_APPLICATION_REQUESTOR, NULL);
        ASSERT_TRUE(layer.setCertificateFromString(certPEM).good());
        const bool keyLoaded = layer.setPrivateKeyFromString(otherKeyPEM).good();
        EXPECT_FALSE(keyLoaded && layer.checkPrivateKeyMatchesCertificate());
}

TEST_F(TLSLayer, SystemTrustedCertificatesLoad)
{
        GTLSTransportLayer layer(DICOM_APPLICATION_REQUESTOR, NULL);
        EXPECT_TRUE(layer.addSystemTrustedCertificates().good());
}
