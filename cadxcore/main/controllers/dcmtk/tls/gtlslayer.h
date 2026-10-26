/*
 * This file is part of Ginkgo CADx
 *
 * Copyright (c) 2015-2016 Gert Wollny
 * Copyright (c) 2008-2014 MetaEmotion S.L. All rights reserved.
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


#pragma once

#include "tls.h"

#include <string>
#include <dcmtk/dcmtls/tlslayer.h>

/**
* Class derived from DcmTLSTransportLayer to accept inline certs/keys
**/
class GTLSTransportLayer : public DcmTLSTransportLayer
{
public:

        /** constructor.
        *  @param networkRole network role to be used by the application, influences
        *    the choice of the secure transport layer code.
        *  @param randFile path to file used to feed the random generator
        */
        GTLSTransportLayer(int networkRole, const char *randFile) :
           DcmTLSTransportLayer(static_cast<T_ASC_NetworkRole>(networkRole), randFile, OFTrue) {}

        /// destructor
        virtual ~GTLSTransportLayer();

        /** loads the private key used for authentication of this application from a string.
        *  @param key The private key, PEM encoded
        *  @return EC_Normal if successful, an error condition otherwise
        */
        OFCondition setPrivateKeyFromString(const std::string& key);

        /** loads the certificate (public key) used for authentication of this application from a string.
        *  @param cert The certificate, PEM encoded
        *  @return EC_Normal if successful, an error condition otherwise
        */
        OFCondition setCertificateFromString(const std::string& cert);

        /** adds the certificates the operating system trusts to the pool of trusted certificates.
        *  @return EC_Normal if successful, an error condition otherwise
        */
        OFCondition addSystemTrustedCertificates();
};
