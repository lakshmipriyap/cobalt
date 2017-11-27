// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ev_root_ca_metadata.h"

#if defined(USE_NSS) || defined(OS_IOS)
#include <cert.h>
#include <pkcs11n.h>
#include <secerr.h>
#include <secoid.h>
#elif defined(OS_WIN)
#include <stdlib.h>
#endif

#include "base/lazy_instance.h"
#include "base/logging.h"
#if defined(USE_NSS) || defined(OS_IOS)
#include "crypto/nss_util.h"
#endif

namespace net {

// Raw metadata.
struct EVMetadata {
  // kMaxOIDsPerCA is the number of OIDs that we can support per root CA. At
  // least one CA has different EV policies for businuss vs government
  // entities and, in the case of cross-signing, we might need to list another
  // CA's policy OID under the cross-signing root.
  static const size_t kMaxOIDsPerCA = 2;
  // This is the maximum length of an OID string (including the trailing NUL).
  static const size_t kMaxOIDLength = 32;

  // The SHA-1 fingerprint of the root CA certificate, used as a unique
  // identifier for a root CA certificate.
  SHA1HashValue fingerprint;

  // The EV policy OIDs of the root CA.
  const char policy_oids[kMaxOIDsPerCA][kMaxOIDLength];
};

#if defined(USE_NSS) || defined(OS_IOS) || defined(OS_WIN)
static const EVMetadata ev_root_ca_metadata[] = {
  // AddTrust External CA Root
  // https://addtrustexternalcaroot-ev.comodoca.com
  { { { 0x02, 0xfa, 0xf3, 0xe2, 0x91, 0x43, 0x54, 0x68, 0x60, 0x78,
        0x57, 0x69, 0x4d, 0xf5, 0xe4, 0x5b, 0x68, 0x85, 0x18, 0x68 } },
    {
      "1.3.6.1.4.1.6449.1.2.1.5.1",
      // This is the Network Solutions EV OID. However, this root
      // cross-certifies NetSol and so we need it here too.
      "1.3.6.1.4.1.782.1.2.1.8.1",
    },
  },
  // AffirmTrust Commercial
  // https://commercial.affirmtrust.com/
  { { { 0xf9, 0xb5, 0xb6, 0x32, 0x45, 0x5f, 0x9c, 0xbe, 0xec, 0x57,
        0x5f, 0x80, 0xdc, 0xe9, 0x6e, 0x2c, 0xc7, 0xb2, 0x78, 0xb7 } },
    {"1.3.6.1.4.1.34697.2.1", ""},
  },
  // AffirmTrust Networking
  // https://networking.affirmtrust.com:4431
  { { { 0x29, 0x36, 0x21, 0x02, 0x8b, 0x20, 0xed, 0x02, 0xf5, 0x66,
        0xc5, 0x32, 0xd1, 0xd6, 0xed, 0x90, 0x9f, 0x45, 0x00, 0x2f } },
    {"1.3.6.1.4.1.34697.2.2", ""},
  },
  // AffirmTrust Premium
  // https://premium.affirmtrust.com:4432/
  { { { 0xd8, 0xa6, 0x33, 0x2c, 0xe0, 0x03, 0x6f, 0xb1, 0x85, 0xf6,
        0x63, 0x4f, 0x7d, 0x6a, 0x06, 0x65, 0x26, 0x32, 0x28, 0x27 } },
    {"1.3.6.1.4.1.34697.2.3", ""},
  },
  // AffirmTrust Premium ECC
  // https://premiumecc.affirmtrust.com:4433/
  { { { 0xb8, 0x23, 0x6b, 0x00, 0x2f, 0x1d, 0x16, 0x86, 0x53, 0x01,
        0x55, 0x6c, 0x11, 0xa4, 0x37, 0xca, 0xeb, 0xff, 0xc3, 0xbb } },
    {"1.3.6.1.4.1.34697.2.4", ""},
  },
  // CertPlus Class 2 Primary CA (KEYNECTIS)
  // https://www.keynectis.com/
  { { { 0x74, 0x20, 0x74, 0x41, 0x72, 0x9c, 0xdd, 0x92, 0xec, 0x79,
        0x31, 0xd8, 0x23, 0x10, 0x8d, 0xc2, 0x81, 0x92, 0xe2, 0xbb } },
    {"1.3.6.1.4.1.22234.2.5.2.3.1", ""},
  },
  // Certum Trusted Network CA
  // https://juice.certum.pl/
  { { { 0x07, 0xe0, 0x32, 0xe0, 0x20, 0xb7, 0x2c, 0x3f, 0x19, 0x2f,
        0x06, 0x28, 0xa2, 0x59, 0x3a, 0x19, 0xa7, 0x0f, 0x06, 0x9e } },
    {"1.2.616.1.113527.2.5.1.1", ""},
  },
  // COMODO Certification Authority
  // https://secure.comodo.com/
  { { { 0x66, 0x31, 0xbf, 0x9e, 0xf7, 0x4f, 0x9e, 0xb6, 0xc9, 0xd5,
        0xa6, 0x0c, 0xba, 0x6a, 0xbe, 0xd1, 0xf7, 0xbd, 0xef, 0x7b } },
    {"1.3.6.1.4.1.6449.1.2.1.5.1", ""},
  },
  // COMODO Certification Authority (reissued certificate with NotBefore of Jan
  // 1 00:00:00 2011 GMT)
  // https://secure.comodo.com/
  { { { 0xee, 0x86, 0x93, 0x87, 0xff, 0xfd, 0x83, 0x49, 0xab, 0x5a,
        0xd1, 0x43, 0x22, 0x58, 0x87, 0x89, 0xa4, 0x57, 0xb0, 0x12 } },
    {"1.3.6.1.4.1.6449.1.2.1.5.1", ""},
  },
  // COMODO ECC Certification Authority
  // https://comodoecccertificationauthority-ev.comodoca.com/
  { { { 0x9f, 0x74, 0x4e, 0x9f, 0x2b, 0x4d, 0xba, 0xec, 0x0f, 0x31,
        0x2c, 0x50, 0xb6, 0x56, 0x3b, 0x8e, 0x2d, 0x93, 0xc3, 0x11 } },
    {"1.3.6.1.4.1.6449.1.2.1.5.1", ""},
  },
  // Cybertrust Global Root
  // https://evup.cybertrust.ne.jp/ctj-ev-upgrader/evseal.gif
  { { { 0x5f, 0x43, 0xe5, 0xb1, 0xbf, 0xf8, 0x78, 0x8c, 0xac, 0x1c,
        0xc7, 0xca, 0x4a, 0x9a, 0xc6, 0x22, 0x2b, 0xcc, 0x34, 0xc6 } },
    {"1.3.6.1.4.1.6334.1.100.1", ""},
  },
  // DigiCert High Assurance EV Root CA
  // https://www.digicert.com
  { { { 0x5f, 0xb7, 0xee, 0x06, 0x33, 0xe2, 0x59, 0xdb, 0xad, 0x0c,
        0x4c, 0x9a, 0xe6, 0xd3, 0x8f, 0x1a, 0x61, 0xc7, 0xdc, 0x25 } },
    {"2.16.840.1.114412.2.1", ""},
  },
  // Entrust.net Secure Server Certification Authority
  // https://www.entrust.net/
  { { { 0x99, 0xa6, 0x9b, 0xe6, 0x1a, 0xfe, 0x88, 0x6b, 0x4d, 0x2b,
        0x82, 0x00, 0x7c, 0xb8, 0x54, 0xfc, 0x31, 0x7e, 0x15, 0x39 } },
    {"2.16.840.1.114028.10.1.2", ""},
  },
  // Entrust Root Certification Authority
  // https://www.entrust.net/
  { { { 0xb3, 0x1e, 0xb1, 0xb7, 0x40, 0xe3, 0x6c, 0x84, 0x02, 0xda,
        0xdc, 0x37, 0xd4, 0x4d, 0xf5, 0xd4, 0x67, 0x49, 0x52, 0xf9 } },
    {"2.16.840.1.114028.10.1.2", ""},
  },
  // Equifax Secure Certificate Authority (GeoTrust)
  // https://www.geotrust.com/
  { { { 0xd2, 0x32, 0x09, 0xad, 0x23, 0xd3, 0x14, 0x23, 0x21, 0x74,
        0xe4, 0x0d, 0x7f, 0x9d, 0x62, 0x13, 0x97, 0x86, 0x63, 0x3a } },
    {"1.3.6.1.4.1.14370.1.6", ""},
  },
  // GeoTrust Primary Certification Authority
  // https://www.geotrust.com/
  { { { 0x32, 0x3c, 0x11, 0x8e, 0x1b, 0xf7, 0xb8, 0xb6, 0x52, 0x54,
        0xe2, 0xe2, 0x10, 0x0d, 0xd6, 0x02, 0x90, 0x37, 0xf0, 0x96 } },
    {"1.3.6.1.4.1.14370.1.6", ""},
  },
  // GlobalSign Root CA - R2
  // https://www.globalsign.com/
  { { { 0x75, 0xe0, 0xab, 0xb6, 0x13, 0x85, 0x12, 0x27, 0x1c, 0x04,
        0xf8, 0x5f, 0xdd, 0xde, 0x38, 0xe4, 0xb7, 0x24, 0x2e, 0xfe } },
    {"1.3.6.1.4.1.4146.1.1", ""},
  },
  // GlobalSign Root CA
  { { { 0xb1, 0xbc, 0x96, 0x8b, 0xd4, 0xf4, 0x9d, 0x62, 0x2a, 0xa8,
        0x9a, 0x81, 0xf2, 0x15, 0x01, 0x52, 0xa4, 0x1d, 0x82, 0x9c } },
    {"1.3.6.1.4.1.4146.1.1", ""},
  },
  // GlobalSign Root CA - R3
  // https://2029.globalsign.com/
  { { { 0xd6, 0x9b, 0x56, 0x11, 0x48, 0xf0, 0x1c, 0x77, 0xc5, 0x45,
        0x78, 0xc1, 0x09, 0x26, 0xdf, 0x5b, 0x85, 0x69, 0x76, 0xad } },
    {"1.3.6.1.4.1.4146.1.1", ""},
  },
  // Go Daddy Class 2 Certification Authority
  // https://www.godaddy.com/
  { { { 0x27, 0x96, 0xba, 0xe6, 0x3f, 0x18, 0x01, 0xe2, 0x77, 0x26,
        0x1b, 0xa0, 0xd7, 0x77, 0x70, 0x02, 0x8f, 0x20, 0xee, 0xe4 } },
    {"2.16.840.1.114413.1.7.23.3", ""},
  },
  // GTE CyberTrust Global Root
  // https://www.cybertrust.ne.jp/
  { { { 0x97, 0x81, 0x79, 0x50, 0xd8, 0x1c, 0x96, 0x70, 0xcc, 0x34,
        0xd8, 0x09, 0xcf, 0x79, 0x44, 0x31, 0x36, 0x7e, 0xf4, 0x74 } },
    {"1.3.6.1.4.1.6334.1.100.1", ""},
  },
  // Izenpe.com - SHA256 root
  // The first OID is for businesses and the second for government entities.
  // These are the test sites, respectively:
  // https://servicios.izenpe.com
  // https://servicios1.izenpe.com
  { { { 0x2f, 0x78, 0x3d, 0x25, 0x52, 0x18, 0xa7, 0x4a, 0x65, 0x39,
        0x71, 0xb5, 0x2c, 0xa2, 0x9c, 0x45, 0x15, 0x6f, 0xe9, 0x19} },
    {"1.3.6.1.4.1.14777.6.1.1", "1.3.6.1.4.1.14777.6.1.2"},
  },
  // Izenpe.com - SHA1 root
  // Windows XP finds this, SHA1, root instead. The policy OIDs are the same as
  // for the SHA256 root, above.
  { { { 0x30, 0x77, 0x9e, 0x93, 0x15, 0x02, 0x2e, 0x94, 0x85, 0x6a,
        0x3f, 0xf8, 0xbc, 0xf8, 0x15, 0xb0, 0x82, 0xf9, 0xae, 0xfd} },
    {"1.3.6.1.4.1.14777.6.1.1", "1.3.6.1.4.1.14777.6.1.2"},
  },
  //  Network Solutions Certificate Authority
  //  https://www.networksolutions.com/website-packages/index.jsp
  { { { 0x74, 0xf8, 0xa3, 0xc3, 0xef, 0xe7, 0xb3, 0x90, 0x06, 0x4b,
        0x83, 0x90, 0x3c, 0x21, 0x64, 0x60, 0x20, 0xe5, 0xdf, 0xce } },
    {"1.3.6.1.4.1.782.1.2.1.8.1", ""},
  },
  //  Network Solutions Certificate Authority (reissued certificate with
  //  NotBefore of Jan  1 00:00:00 2011 GMT).
  //  https://www.networksolutions.com/website-packages/index.jsp
  { { { 0x71, 0x89, 0x9a, 0x67, 0xbf, 0x33, 0xaf, 0x31, 0xbe, 0xfd,
        0xc0, 0x71, 0xf8, 0xf7, 0x33, 0xb1, 0x83, 0x85, 0x63, 0x32 } },
    {"1.3.6.1.4.1.782.1.2.1.8.1", ""},
  },
  // QuoVadis Root CA 2
  // https://www.quovadis.bm/
  { { { 0xca, 0x3a, 0xfb, 0xcf, 0x12, 0x40, 0x36, 0x4b, 0x44, 0xb2,
        0x16, 0x20, 0x88, 0x80, 0x48, 0x39, 0x19, 0x93, 0x7c, 0xf7 } },
    {"1.3.6.1.4.1.8024.0.2.100.1.2", ""},
  },
  // SecureTrust CA, SecureTrust Corporation
  // https://www.securetrust.com
  // https://www.trustwave.com/
  { { { 0x87, 0x82, 0xc6, 0xc3, 0x04, 0x35, 0x3b, 0xcf, 0xd2, 0x96,
        0x92, 0xd2, 0x59, 0x3e, 0x7d, 0x44, 0xd9, 0x34, 0xff, 0x11 } },
    {"2.16.840.1.114404.1.1.2.4.1", ""},
  },
  // Secure Global CA, SecureTrust Corporation
  { { { 0x3a, 0x44, 0x73, 0x5a, 0xe5, 0x81, 0x90, 0x1f, 0x24, 0x86,
        0x61, 0x46, 0x1e, 0x3b, 0x9c, 0xc4, 0x5f, 0xf5, 0x3a, 0x1b } },
    {"2.16.840.1.114404.1.1.2.4.1", ""},
  },
  // Security Communication RootCA1
  // https://www.secomtrust.net/contact/form.html
  { { { 0x36, 0xb1, 0x2b, 0x49, 0xf9, 0x81, 0x9e, 0xd7, 0x4c, 0x9e,
        0xbc, 0x38, 0x0f, 0xc6, 0x56, 0x8f, 0x5d, 0xac, 0xb2, 0xf7 } },
    {"1.2.392.200091.100.721.1", ""},
  },
  // Security Communication EV RootCA1
  // https://www.secomtrust.net/contact/form.html
  { { { 0xfe, 0xb8, 0xc4, 0x32, 0xdc, 0xf9, 0x76, 0x9a, 0xce, 0xae,
        0x3d, 0xd8, 0x90, 0x8f, 0xfd, 0x28, 0x86, 0x65, 0x64, 0x7d } },
    {"1.2.392.200091.100.721.1", ""},
  },
  // StartCom Certification Authority
  // https://www.startssl.com/
  { { { 0x3e, 0x2b, 0xf7, 0xf2, 0x03, 0x1b, 0x96, 0xf3, 0x8c, 0xe6,
        0xc4, 0xd8, 0xa8, 0x5d, 0x3e, 0x2d, 0x58, 0x47, 0x6a, 0x0f } },
    {"1.3.6.1.4.1.23223.1.1.1", ""},
  },
  // Starfield Class 2 Certification Authority
  // https://www.starfieldtech.com/
  { { { 0xad, 0x7e, 0x1c, 0x28, 0xb0, 0x64, 0xef, 0x8f, 0x60, 0x03,
        0x40, 0x20, 0x14, 0xc3, 0xd0, 0xe3, 0x37, 0x0e, 0xb5, 0x8a } },
    {"2.16.840.1.114414.1.7.23.3", ""},
  },
  // SwissSign Gold CA - G2
  // https://testevg2.swisssign.net/
  { { { 0xd8, 0xc5, 0x38, 0x8a, 0xb7, 0x30, 0x1b, 0x1b, 0x6e, 0xd4,
        0x7a, 0xe6, 0x45, 0x25, 0x3a, 0x6f, 0x9f, 0x1a, 0x27, 0x61 } },
    {"2.16.756.1.89.1.2.1.1", ""},
  },
  // Thawte Premium Server CA
  // https://www.thawte.com/
  { { { 0x62, 0x7f, 0x8d, 0x78, 0x27, 0x65, 0x63, 0x99, 0xd2, 0x7d,
        0x7f, 0x90, 0x44, 0xc9, 0xfe, 0xb3, 0xf3, 0x3e, 0xfa, 0x9a } },
    {"2.16.840.1.113733.1.7.48.1", ""},
  },
  // thawte Primary Root CA
  // https://www.thawte.com/
  { { { 0x91, 0xc6, 0xd6, 0xee, 0x3e, 0x8a, 0xc8, 0x63, 0x84, 0xe5,
        0x48, 0xc2, 0x99, 0x29, 0x5c, 0x75, 0x6c, 0x81, 0x7b, 0x81 } },
    {"2.16.840.1.113733.1.7.48.1", ""},
  },
  // T-TeleSec GlobalRoot Class 3
  // http://www.telesec.de/ / https://root-class3.test.telesec.de/
  { { { 0x55, 0xa6, 0x72, 0x3e, 0xcb, 0xf2, 0xec, 0xcd, 0xc3, 0x23,
        0x74, 0x70, 0x19, 0x9d, 0x2a, 0xbe, 0x11, 0xe3, 0x81, 0xd1 } },
    {"1.3.6.1.4.1.7879.13.24.1", "" },
  },
  // UTN - DATACorp SGC
  { { { 0x58, 0x11, 0x9f, 0x0e, 0x12, 0x82, 0x87, 0xea, 0x50, 0xfd,
        0xd9, 0x87, 0x45, 0x6f, 0x4f, 0x78, 0xdc, 0xfa, 0xd6, 0xd4 } },
    {"1.3.6.1.4.1.6449.1.2.1.5.1", ""},
  },
  // UTN-USERFirst-Hardware
  { { { 0x04, 0x83, 0xed, 0x33, 0x99, 0xac, 0x36, 0x08, 0x05, 0x87,
        0x22, 0xed, 0xbc, 0x5e, 0x46, 0x00, 0xe3, 0xbe, 0xf9, 0xd7 } },
    {
      "1.3.6.1.4.1.6449.1.2.1.5.1",
      // This is the Network Solutions EV OID. However, this root
      // cross-certifies NetSol and so we need it here too.
      "1.3.6.1.4.1.782.1.2.1.8.1",
    },
  },
  // ValiCert Class 2 Policy Validation Authority
  { { { 0x31, 0x7a, 0x2a, 0xd0, 0x7f, 0x2b, 0x33, 0x5e, 0xf5, 0xa1,
        0xc3, 0x4e, 0x4b, 0x57, 0xe8, 0xb7, 0xd8, 0xf1, 0xfc, 0xa6 } },
    {"2.16.840.1.114413.1.7.23.3", "2.16.840.1.114414.1.7.23.3"},
  },
  // VeriSign Class 3 Public Primary Certification Authority
  // https://www.verisign.com/
  { { { 0x74, 0x2c, 0x31, 0x92, 0xe6, 0x07, 0xe4, 0x24, 0xeb, 0x45,
        0x49, 0x54, 0x2b, 0xe1, 0xbb, 0xc5, 0x3e, 0x61, 0x74, 0xe2 } },
    {"2.16.840.1.113733.1.7.23.6", ""},
  },
  // VeriSign Class 3 Public Primary Certification Authority - G5
  // https://www.verisign.com/
  { { { 0x4e, 0xb6, 0xd5, 0x78, 0x49, 0x9b, 0x1c, 0xcf, 0x5f, 0x58,
        0x1e, 0xad, 0x56, 0xbe, 0x3d, 0x9b, 0x67, 0x44, 0xa5, 0xe5 } },
    {"2.16.840.1.113733.1.7.23.6", ""},
  },
  // Wells Fargo WellsSecure Public Root Certificate Authority
  // https://nerys.wellsfargo.com/test.html
  { { { 0xe7, 0xb4, 0xf6, 0x9d, 0x61, 0xec, 0x90, 0x69, 0xdb, 0x7e,
        0x90, 0xa7, 0x40, 0x1a, 0x3c, 0xf4, 0x7d, 0x4f, 0xe8, 0xee } },
    {"2.16.840.1.114171.500.9", ""},
  },
  // XRamp Global Certification Authority
  { { { 0xb8, 0x01, 0x86, 0xd1, 0xeb, 0x9c, 0x86, 0xa5, 0x41, 0x04,
        0xcf, 0x30, 0x54, 0xf3, 0x4c, 0x52, 0xb7, 0xe5, 0x58, 0xc6 } },
    {"2.16.840.1.114404.1.1.2.4.1", ""},
  }
};
#endif  // defined(USE_NSS) || defined(OS_IOS) || defined(OS_WIN)
static base::LazyInstance<EVRootCAMetadata>::Leaky
    g_ev_root_ca_metadata = LAZY_INSTANCE_INITIALIZER;

// static
EVRootCAMetadata* EVRootCAMetadata::GetInstance() {
  return g_ev_root_ca_metadata.Pointer();
}

#if defined(USE_NSS) || defined(OS_IOS)
bool EVRootCAMetadata::IsEVPolicyOID(PolicyOID policy_oid) const {
  return policy_oids_.find(policy_oid) != policy_oids_.end();
}

bool EVRootCAMetadata::HasEVPolicyOID(
    const SHA1HashValue& fingerprint,
    PolicyOID policy_oid) const {
  PolicyOIDMap::const_iterator iter = ev_policy_.find(fingerprint);
  if (iter == ev_policy_.end())
    return false;
  for (std::vector<PolicyOID>::const_iterator
       j = iter->second.begin(); j != iter->second.end(); ++j) {
    if (*j == policy_oid)
      return true;
  }
  return false;
}

bool EVRootCAMetadata::AddEVCA(const SHA1HashValue& fingerprint,
                               const char* policy) {
  if (ev_policy_.find(fingerprint) != ev_policy_.end())
    return false;

  PolicyOID oid;
  if (!RegisterOID(policy, &oid))
    return false;

  ev_policy_[fingerprint].push_back(oid);
  policy_oids_.insert(oid);

  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA1HashValue& fingerprint) {
  PolicyOIDMap::iterator it = ev_policy_.find(fingerprint);
  if (it == ev_policy_.end())
    return false;
  PolicyOID oid = it->second[0];
  ev_policy_.erase(it);
  policy_oids_.erase(oid);
  return true;
}

// static
bool EVRootCAMetadata::RegisterOID(const char* policy,
                                   PolicyOID* out) {
  PRUint8 buf[64];
  SECItem oid_item;
  oid_item.data = buf;
  oid_item.len = sizeof(buf);
  SECStatus status = SEC_StringToOID(NULL, &oid_item, policy, 0);
  if (status != SECSuccess)
    return false;

  // Register the OID.
  SECOidData od;
  od.oid.len = oid_item.len;
  od.oid.data = oid_item.data;
  od.offset = SEC_OID_UNKNOWN;
  od.desc = policy;
  od.mechanism = CKM_INVALID_MECHANISM;
  od.supportedExtension = INVALID_CERT_EXTENSION;
  *out = SECOID_AddEntry(&od);
  return *out != SEC_OID_UNKNOWN;
}

#elif defined(OS_WIN)

bool EVRootCAMetadata::IsEVPolicyOID(PolicyOID policy_oid) const {
  for (size_t i = 0; i < arraysize(ev_root_ca_metadata); i++) {
    for (size_t j = 0; j < arraysize(ev_root_ca_metadata[i].policy_oids); j++) {
      if (ev_root_ca_metadata[i].policy_oids[j][0] == '\0')
        break;
      if (strcmp(policy_oid, ev_root_ca_metadata[i].policy_oids[j]) == 0)
        return true;
    }
  }

  for (ExtraEVCAMap::const_iterator i = extra_cas_.begin();
       i != extra_cas_.end(); i++) {
    if (i->second == policy_oid)
      return true;
  }

  return false;
}

bool EVRootCAMetadata::HasEVPolicyOID(const SHA1HashValue& fingerprint,
                                      PolicyOID policy_oid) const {
  for (size_t i = 0; i < arraysize(ev_root_ca_metadata); i++) {
    if (!fingerprint.Equals(ev_root_ca_metadata[i].fingerprint))
      continue;
    for (size_t j = 0; j < arraysize(ev_root_ca_metadata[i].policy_oids); j++) {
      if (ev_root_ca_metadata[i].policy_oids[j][0] == '\0')
        break;
      if (strcmp(policy_oid, ev_root_ca_metadata[i].policy_oids[j]) == 0)
        return true;
    }
    return false;
  }

  ExtraEVCAMap::const_iterator it = extra_cas_.find(fingerprint);
  return it != extra_cas_.end() && it->second == policy_oid;
}

bool EVRootCAMetadata::AddEVCA(const SHA1HashValue& fingerprint,
                               const char* policy) {
  for (size_t i = 0; i < arraysize(ev_root_ca_metadata); i++) {
    if (fingerprint.Equals(ev_root_ca_metadata[i].fingerprint))
      return false;
  }

  if (extra_cas_.find(fingerprint) != extra_cas_.end())
    return false;

  extra_cas_[fingerprint] = policy;
  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA1HashValue& fingerprint) {
  ExtraEVCAMap::iterator it = extra_cas_.find(fingerprint);
  if (it == extra_cas_.end())
    return false;
  extra_cas_.erase(it);
  return true;
}

#else

// These are just stub functions for platforms where we don't use this EV
// metadata.

bool EVRootCAMetadata::AddEVCA(const SHA1HashValue& fingerprint,
                               const char* policy) {
  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA1HashValue& fingerprint) {
  return true;
}

#endif

EVRootCAMetadata::EVRootCAMetadata() {
  // Constructs the object from the raw metadata in ev_root_ca_metadata.
#if defined(USE_NSS) || defined(OS_IOS)
  crypto::EnsureNSSInit();

  for (size_t i = 0; i < arraysize(ev_root_ca_metadata); i++) {
    const EVMetadata& metadata = ev_root_ca_metadata[i];
    for (size_t j = 0; j < arraysize(metadata.policy_oids); j++) {
      if (metadata.policy_oids[j][0] == '\0')
        break;
      const char* policy_oid = metadata.policy_oids[j];

      PolicyOID policy;
      if (!RegisterOID(policy_oid, &policy)) {
        LOG(ERROR) << "Failed to register OID: " << policy_oid;
        continue;
      }

      ev_policy_[metadata.fingerprint].push_back(policy);
      policy_oids_.insert(policy);
    }
  }
#endif
}

EVRootCAMetadata::~EVRootCAMetadata() { }

}  // namespace net