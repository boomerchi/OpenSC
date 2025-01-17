/**
 * cwa-dnie.c: DNIe data provider for CWA SM handling.
 *
 * Copyright (C) 2010 Juan Antonio Martinez <jonsito@terra.es>
 *
 * This work is derived from many sources at OpenSC Project site,
 * (see references) and the information made public by Spanish
 * Direccion General de la Policia y de la Guardia Civil
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define __SM_DNIE_C__
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(ENABLE_OPENSSL) && defined(ENABLE_SM)	/* empty file without openssl or sm */

#include <stdlib.h>
#include <string.h>

#include "opensc.h"
#include "cardctl.h"
#include "internal.h"
#include "cwa14890.h"

#include "cwa-dnie.h"

#include <openssl/ossl_typ.h>
#include <openssl/bn.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
# include <openssl/core_names.h>
# include <openssl/param_build.h>
#endif

#define MAX_RESP_BUFFER_SIZE 2048

/********************* Keys and certificates as published by DGP ********/

/**
 * Public Key modulus for the ROOT CA for DNIe (pk-RCAicc->n)
 */
static u8 icc_root_ca_modulus_0[] = {
	0xEA, 0xDE, 0xDA, 0x45, 0x53, 0x32, 0x94, 0x50, 0x39, 0xDA, 0xA4, 0x04,
	0xC8, 0xEB, 0xC4, 0xD3, 0xB7, 0xF5, 0xDC, 0x86, 0x92, 0x83, 0xCD, 0xEA,
	0x2F, 0x10, 0x1E, 0x2A, 0xB5, 0x4F, 0xB0, 0xD0, 0xB0, 0x3D, 0x8F, 0x03,
	0x0D, 0xAF, 0x24, 0x58, 0x02, 0x82, 0x88, 0xF5, 0x4C, 0xE5, 0x52, 0xF8,
	0xFA, 0x57, 0xAB, 0x2F, 0xB1, 0x03, 0xB1, 0x12, 0x42, 0x7E, 0x11, 0x13,
	0x1D, 0x1D, 0x27, 0xE1, 0x0A, 0x5B, 0x50, 0x0E, 0xAA, 0xE5, 0xD9, 0x40,
	0x30, 0x1E, 0x30, 0xEB, 0x26, 0xC3, 0xE9, 0x06, 0x6B, 0x25, 0x71, 0x56,
	0xED, 0x63, 0x9D, 0x70, 0xCC, 0xC0, 0x90, 0xB8, 0x63, 0xAF, 0xBB, 0x3B,
	0xFE, 0xD8, 0xC1, 0x7B, 0xE7, 0x67, 0x30, 0x34, 0xB9, 0x82, 0x3E, 0x97,
	0x7E, 0xD6, 0x57, 0x25, 0x29, 0x27, 0xF9, 0x57, 0x5B, 0x9F, 0xFF, 0x66,
	0x91, 0xDB, 0x64, 0xF8, 0x0B, 0x5E, 0x92, 0xCD
};

static u8 icc_root_ca_modulus_1[] = {
        0xb9, 0x72, 0x34, 0x5e, 0x35, 0xbc, 0xdd, 0x12, 0xdc, 0x2c, 0x8e, 0x85,
        0xf6, 0x22, 0x97, 0x97, 0x9f, 0x12, 0x2b, 0xb7, 0xc9, 0xc3, 0xed, 0x13,
        0xa0, 0xc4, 0xeb, 0x59, 0x34, 0xe7, 0x0c, 0xd6, 0xd0, 0x0c, 0x54, 0x06,
        0x18, 0x38, 0x6e, 0x42, 0xf2, 0xba, 0x00, 0x89, 0xc0, 0xdd, 0x80, 0x0e,
        0xba, 0x78, 0x3b, 0xdc, 0x9d, 0x93, 0xd9, 0xfb, 0xfc, 0x3c, 0x16, 0x9f,
        0x9a, 0xf6, 0x4e, 0x80, 0x10, 0x0f, 0xc6, 0x87, 0xcc, 0xa5, 0x62, 0xe7,
        0xfc, 0x84, 0xd1, 0x12, 0x92, 0xc2, 0x40, 0x4c, 0x59, 0xb8, 0xa8, 0x60,
        0xd3, 0x9e, 0x2d, 0x66, 0x54, 0x7d, 0xc7, 0xb2, 0xd4, 0x8c, 0xa7, 0x89,
        0x81, 0x4f, 0x43, 0x06, 0x26, 0x34, 0xe3, 0xe0, 0xc0, 0xd6, 0xbf, 0x5f,
        0x54, 0xba, 0x1d, 0x9c, 0x46, 0x64, 0x45, 0x83, 0x1d, 0xcd, 0xea, 0xb0,
        0x87, 0x08, 0xf3, 0xf6, 0x22, 0x0e, 0x07, 0x75
};

/**
 * Exponente de la clave publica de la Root CA del DNI electronico (pk-RCAicc->e)
 */
static u8 icc_root_ca_public_exponent[] = {
	0x01, 0x00, 0x01
};

/**
 * Terminal (IFD) key modulus for SM channel creation (dnieRealParam->sk-IFD-AUT->n)
 */
static u8 ifd_modulus_0[] = {
	0xdb, 0x2c, 0xb4, 0x1e, 0x11, 0x2b, 0xac, 0xfa, 0x2b, 0xd7, 0xc3, 0xd3,
	0xd7, 0x96, 0x7e, 0x84, 0xfb, 0x94, 0x34, 0xfc, 0x26, 0x1f, 0x9d, 0x09,
	0x0a, 0x89, 0x83, 0x94, 0x7d, 0xaf, 0x84, 0x88, 0xd3, 0xdf, 0x8f, 0xbd,
	0xcc, 0x1f, 0x92, 0x49, 0x35, 0x85, 0xe1, 0x34, 0xa1, 0xb4, 0x2d, 0xe5,
	0x19, 0xf4, 0x63, 0x24, 0x4d, 0x7e, 0xd3, 0x84, 0xe2, 0x6d, 0x51, 0x6c,
	0xc7, 0xa4, 0xff, 0x78, 0x95, 0xb1, 0x99, 0x21, 0x40, 0x04, 0x3a, 0xac,
	0xad, 0xfc, 0x12, 0xe8, 0x56, 0xb2, 0x02, 0x34, 0x6a, 0xf8, 0x22, 0x6b,
	0x1a, 0x88, 0x21, 0x37, 0xdc, 0x3c, 0x5a, 0x57, 0xf0, 0xd2, 0x81, 0x5c,
	0x1f, 0xcd, 0x4b, 0xb4, 0x6f, 0xa9, 0x15, 0x7f, 0xdf, 0xfd, 0x79, 0xec,
	0x3a, 0x10, 0xa8, 0x24, 0xcc, 0xc1, 0xeb, 0x3c, 0xe0, 0xb6, 0xb4, 0x39,
	0x6a, 0xe2, 0x36, 0x59, 0x00, 0x16, 0xba, 0x69
};

static u8 ifd_modulus_1[] = {
        0xbd, 0xef, 0xdb, 0x84, 0xec, 0xe6, 0x98, 0xb8, 0x28, 0x7f, 0x7f, 0xe6,
        0x29, 0x6d, 0x80, 0x72, 0x98, 0x3a, 0x1b, 0x3d, 0x3b, 0x9f, 0x57, 0xad,
        0x98, 0x4f, 0xba, 0x78, 0x58, 0x1f, 0xff, 0x52, 0xe9, 0x3d, 0x89, 0x6b,
        0xf5, 0x62, 0x25, 0xe9, 0xf8, 0x2e, 0x96, 0x95, 0x14, 0x00, 0x69, 0x98,
        0x2e, 0x5b, 0x5b, 0xce, 0x37, 0xad, 0x73, 0x16, 0x45, 0x02, 0xd8, 0xac,
        0xbd, 0x60, 0x5f, 0x69, 0x12, 0x4a, 0x3c, 0xf5, 0xaf, 0xe4, 0xb0, 0x18,
        0x60, 0x2d, 0xd4, 0xba, 0x04, 0xdb, 0xc9, 0x85, 0x88, 0x45, 0xe6, 0xa9,
        0xc4, 0x05, 0x5b, 0xc5, 0xbf, 0xa0, 0xed, 0xdb, 0x86, 0x67, 0x89, 0xf0,
        0xec, 0x6a, 0x80, 0xfc, 0xe5, 0x3c, 0x66, 0x08, 0xdf, 0xdc, 0x9b, 0x9f,
        0xe2, 0xed, 0x56, 0x75, 0x2c, 0xc6, 0x05, 0x51, 0x3b, 0xa3, 0xf1, 0x75,
        0x9c, 0xdd, 0x95, 0x22, 0x75, 0x3f, 0x18, 0xd7
};

/**
 * Terminal (IFD) key modulus for SM channel creation for PIN channel DNIe 3.0 (dnie30RealParamPIN->sk-IFD-AUT->n)
 */
static u8 ifd_pin_modulus_0[] = {
	0xF4, 0x27, 0x97, 0x8D, 0xA1, 0x59, 0xBA, 0x02, 0x79, 0x30, 0x8A, 0x6C,
	0x6A, 0x89, 0x50, 0x5A, 0xDA, 0x5A, 0x67, 0xC3, 0xDA, 0x26, 0x79, 0xEA,
	0xF4, 0xA1, 0xB0, 0x11, 0x9E, 0xDD, 0x4D, 0xF4, 0x6E, 0x78, 0x04, 0x24,
	0x71, 0xA9, 0xD1, 0x30, 0x1D, 0x3F, 0xB2, 0x8F, 0x38, 0xC5, 0x7D, 0x08,
	0x89, 0xF7, 0x31, 0xDB, 0x8E, 0xDD, 0xBC, 0x13, 0x67, 0xC1, 0x34, 0xE1,
	0xE9, 0x47, 0x78, 0x6B, 0x8E, 0xC8, 0xE4, 0xB9, 0xCA, 0x6A, 0xA7, 0xC2,
	0x4C, 0x86, 0x91, 0xC7, 0xBE, 0x2F, 0xD8, 0xC1, 0x23, 0x66, 0x0E, 0x98,
	0x65, 0xE1, 0x4F, 0x19, 0xDF, 0xFB, 0xB7, 0xFF, 0x38, 0x08, 0xC9, 0xF2,
	0x04, 0xE7, 0x97, 0xD0, 0x6D, 0xD8, 0x33, 0x3A, 0xC5, 0x83, 0x86, 0xEE,
	0x4E, 0xB6, 0x1E, 0x20, 0xEC, 0xA7, 0xEF, 0x38, 0xD5, 0xB0, 0x5E, 0xB1,
	0x15, 0x96, 0x6A, 0x5A, 0x89, 0xAD, 0x58, 0xA5
};

static u8 ifd_pin_modulus_1[] = {
        0xdf, 0x03, 0x93, 0x0d, 0x4f, 0x1d, 0x97, 0x15, 0xeb, 0xb0, 0x0f, 0xbd,
        0xae, 0x48, 0xaf, 0x9c, 0x9d, 0xbf, 0xd6, 0x99, 0xca, 0xb0, 0xbd, 0xbe,
        0x5c, 0xdb, 0x01, 0x34, 0x00, 0x0e, 0x46, 0x2e, 0x71, 0x3a, 0xe9, 0x7a,
        0x2f, 0x7e, 0x20, 0xaf, 0xbf, 0x84, 0xd3, 0xce, 0x73, 0x4f, 0xe2, 0x15,
        0x75, 0x7a, 0xaf, 0xa1, 0xe8, 0x9e, 0x64, 0x57, 0xea, 0xe2, 0xe8, 0x08,
        0x11, 0x03, 0x73, 0xe2, 0x56, 0x56, 0x34, 0x94, 0xfb, 0x5d, 0x10, 0x4f,
        0x0d, 0xcc, 0x88, 0x8d, 0x47, 0x96, 0x54, 0x3f, 0x03, 0x25, 0x4f, 0x4e,
        0x2c, 0xdf, 0x98, 0xb1, 0xe1, 0x26, 0x11, 0xe3, 0x98, 0x1f, 0x53, 0x33,
        0xdf, 0x98, 0xc8, 0x86, 0x01, 0x93, 0x75, 0x84, 0x0f, 0xac, 0x61, 0xdb,
        0x8f, 0x1b, 0xa3, 0xb5, 0x43, 0xdc, 0xea, 0x3d, 0x05, 0x9e, 0x6a, 0x41,
        0x4f, 0x6d, 0xd2, 0x9f, 0xc7, 0xc9, 0x9d, 0x8b
};

/**
 * Terminal (IFD) public exponent for SM channel creation
 */
static u8 ifd_public_exponent[] = {
	0x01, 0x00, 0x01
};

/**
 * Terminal (IFD) public exponent for SM channel creation for PIN channel DNIe 3.0
 */
static u8 ifd_pin_public_exponent[] = {
	0x01, 0x00, 0x01
};

/**
 * Terminal (IFD) private exponent for SM channel establishment (dnieRealParam->sk-IFD-AUT->d)
 */
static u8 ifd_private_exponent_0[] = {
	0x18, 0xb4, 0x4a, 0x3d, 0x15, 0x5c, 0x61, 0xeb, 0xf4, 0xe3, 0x26, 0x1c,
	0x8b, 0xb1, 0x57, 0xe3, 0x6f, 0x63, 0xfe, 0x30, 0xe9, 0xaf, 0x28, 0x89,
	0x2b, 0x59, 0xe2, 0xad, 0xeb, 0x18, 0xcc, 0x8c, 0x8b, 0xad, 0x28, 0x4b,
	0x91, 0x65, 0x81, 0x9c, 0xa4, 0xde, 0xc9, 0x4a, 0xa0, 0x6b, 0x69, 0xbc,
	0xe8, 0x17, 0x06, 0xd1, 0xc1, 0xb6, 0x68, 0xeb, 0x12, 0x86, 0x95, 0xe5,
	0xf7, 0xfe, 0xde, 0x18, 0xa9, 0x08, 0xa3, 0x01, 0x1a, 0x64, 0x6a, 0x48,
	0x1d, 0x3e, 0xa7, 0x1d, 0x8a, 0x38, 0x7d, 0x47, 0x46, 0x09, 0xbd, 0x57,
	0xa8, 0x82, 0xb1, 0x82, 0xe0, 0x47, 0xde, 0x80, 0xe0, 0x4b, 0x42, 0x21,
	0x41, 0x6b, 0xd3, 0x9d, 0xfa, 0x1f, 0xac, 0x03, 0x00, 0x64, 0x19, 0x62,
	0xad, 0xb1, 0x09, 0xe2, 0x8c, 0xaf, 0x50, 0x06, 0x1b, 0x68, 0xc9, 0xca,
	0xbd, 0x9b, 0x00, 0x31, 0x3c, 0x0f, 0x46, 0xed
};

static u8 ifd_private_exponent_1[] = {
        0xa0, 0x51, 0x55, 0x93, 0xd4, 0x36, 0x2b, 0x8f, 0xbd, 0xb7, 0x28, 0xa8,
        0x88, 0x2d, 0x42, 0x2e, 0xf7, 0xa8, 0x8c, 0x17, 0x5a, 0x3c, 0xfb, 0xcf,
        0xad, 0xf1, 0x15, 0xee, 0xc0, 0x4b, 0x79, 0xc2, 0x6c, 0xd6, 0xa1, 0x28,
        0xbb, 0xbd, 0x35, 0x4d, 0x50, 0x4b, 0x5a, 0x94, 0xc8, 0x86, 0x34, 0x9a,
        0xdb, 0xfe, 0x06, 0xf6, 0x7f, 0xee, 0x6a, 0x66, 0xd0, 0xa7, 0x3f, 0x66,
        0x46, 0x8e, 0x92, 0xd8, 0x73, 0xb6, 0x8e, 0xe2, 0xcb, 0x47, 0xb1, 0xa1,
        0x5a, 0x2a, 0xa7, 0xd8, 0xc6, 0xce, 0x8f, 0x3f, 0x14, 0x93, 0x0d, 0x56,
        0xb6, 0x32, 0x7f, 0x56, 0xcb, 0x21, 0x54, 0x69, 0xa5, 0x7a, 0x1e, 0xe0,
        0x18, 0x8f, 0xd6, 0xd2, 0x6d, 0x83, 0xa3, 0x80, 0xa6, 0xab, 0xd3, 0xa8,
        0x9f, 0x1b, 0x63, 0xc4, 0x99, 0x81, 0x90, 0x46, 0x53, 0x69, 0x35, 0xad,
        0xb2, 0xdb, 0x3c, 0x17, 0xcc, 0xbd, 0xaa, 0x51
};

/**
 * Terminal (IFD) private exponent for SM channel establishment for PIN channel DNIe 3.0 (dnie30RealParamDataPIN->sk-IFD-AUT->d)
 */
static u8 ifd_pin_private_exponent_0[] = {
	0xD2, 0x7A, 0x03, 0x23, 0x7C, 0x72, 0x2E, 0x71, 0x8D, 0x69, 0xF4, 0x1A,
	0xEC, 0x68, 0xBD, 0x95, 0xE4, 0xE0, 0xC4, 0xCD, 0x49, 0x15, 0x9C, 0x4A,
	0x99, 0x63, 0x7D, 0xB6, 0x62, 0xFE, 0xA3, 0x02, 0x51, 0xED, 0x32, 0x9C,
	0xFC, 0x43, 0x89, 0xEB, 0x71, 0x7B, 0x85, 0x02, 0x04, 0xCD, 0xF3, 0x30,
	0xD6, 0x46, 0xFC, 0x7B, 0x2B, 0x19, 0x29, 0xD6, 0x8C, 0xBE, 0x39, 0x49,
	0x7B, 0x62, 0x3A, 0x82, 0xC7, 0x64, 0x1A, 0xC3, 0x48, 0x79, 0x57, 0x3D,
	0xEA, 0x0D, 0xAB, 0xC7, 0xCA, 0x30, 0x9A, 0xE4, 0xB3, 0xED, 0xDA, 0xFA,
	0xEE, 0x55, 0xD5, 0x42, 0xF7, 0x80, 0x23, 0x03, 0x51, 0xE7, 0x5E, 0x7F,
	0x32, 0xDC, 0x65, 0x2E, 0xF1, 0xED, 0x47, 0xA5, 0x1C, 0x18, 0xD9, 0xDF,
	0x9F, 0xF4, 0x8D, 0x87, 0x8D, 0xB6, 0x22, 0xEA, 0x6E, 0x93, 0x70, 0xE9,
	0xC6, 0x3B, 0x35, 0x8B, 0x7C, 0x11, 0x5A, 0xA1
};

static u8 ifd_pin_private_exponent_1[] = {
        0x86, 0x6f, 0x0f, 0x2c, 0x0c, 0xaf, 0x17, 0xae, 0x7d, 0x1e, 0xea, 0xbe,
        0x3a, 0xdb, 0x52, 0x11, 0x24, 0xfe, 0xc9, 0x8c, 0x77, 0xa4, 0xc7, 0x1c,
        0x83, 0xb8, 0xf9, 0x26, 0xb1, 0x89, 0xe9, 0x40, 0x81, 0xbd, 0x33, 0x95,
        0x16, 0x1f, 0xff, 0xf0, 0x31, 0x91, 0x0e, 0x64, 0xfb, 0x1a, 0x02, 0x7d,
        0x51, 0x0e, 0x1d, 0xe5, 0x89, 0xe6, 0x41, 0x32, 0xc6, 0x42, 0xf6, 0x00,
        0x36, 0xd1, 0x4f, 0xfe, 0xd5, 0xd0, 0xce, 0x1f, 0x45, 0xe7, 0x11, 0x6f,
        0x13, 0xc4, 0xe6, 0x38, 0x8e, 0x25, 0xdd, 0x43, 0x83, 0x57, 0x78, 0x05,
        0x85, 0x73, 0xdc, 0x29, 0xad, 0x6a, 0x37, 0x32, 0x71, 0x6d, 0x08, 0x11,
        0x24, 0xb7, 0x52, 0x51, 0x40, 0xb1, 0xdd, 0xab, 0xe2, 0x51, 0xa4, 0x98,
        0x0c, 0xc5, 0xc0, 0x3a, 0x86, 0xa8, 0x2d, 0x17, 0x4f, 0xb7, 0xa8, 0x1d,
        0x24, 0x8d, 0x7c, 0xaa, 0x2b, 0x3d, 0x61, 0xd1
};

/**
 *  Intermediate CA certificate in CVC format (Card verifiable certificate) (c-CV-CA-CS-AUT)
 */
static u8 C_CV_CA_CS_AUT_cert_0[] = {
	0x7f, 0x21, 0x81, 0xce, 0x5f, 0x37, 0x81, 0x80, 0x3c, 0xba, 0xdc, 0x36,
	0x84, 0xbe, 0xf3, 0x20, 0x41, 0xad, 0x15, 0x50, 0x89, 0x25, 0x8d, 0xfd,
	0x20, 0xc6, 0x91, 0x15, 0xd7, 0x2f, 0x9c, 0x38, 0xaa, 0x99, 0xad, 0x6c,
	0x1a, 0xed, 0xfa, 0xb2, 0xbf, 0xac, 0x90, 0x92, 0xfc, 0x70, 0xcc, 0xc0,
	0x0c, 0xaf, 0x48, 0x2a, 0x4b, 0xe3, 0x1a, 0xfd, 0xbd, 0x3c, 0xbc, 0x8c,
	0x83, 0x82, 0xcf, 0x06, 0xbc, 0x07, 0x19, 0xba, 0xab, 0xb5, 0x6b, 0x6e,
	0xc8, 0x07, 0x60, 0xa4, 0xa9, 0x3f, 0xa2, 0xd7, 0xc3, 0x47, 0xf3, 0x44,
	0x27, 0xf9, 0xff, 0x5c, 0x8d, 0xe6, 0xd6, 0x5d, 0xac, 0x95, 0xf2, 0xf1,
	0x9d, 0xac, 0x00, 0x53, 0xdf, 0x11, 0xa5, 0x07, 0xfb, 0x62, 0x5e, 0xeb,
	0x8d, 0xa4, 0xc0, 0x29, 0x9e, 0x4a, 0x21, 0x12, 0xab, 0x70, 0x47, 0x58,
	0x8b, 0x8d, 0x6d, 0xa7, 0x59, 0x22, 0x14, 0xf2, 0xdb, 0xa1, 0x40, 0xc7,
	0xd1, 0x22, 0x57, 0x9b, 0x5f, 0x38, 0x3d, 0x22, 0x53, 0xc8, 0xb9, 0xcb,
	0x5b, 0xc3, 0x54, 0x3a, 0x55, 0x66, 0x0b, 0xda, 0x80, 0x94, 0x6a, 0xfb,
	0x05, 0x25, 0xe8, 0xe5, 0x58, 0x6b, 0x4e, 0x63, 0xe8, 0x92, 0x41, 0x49,
	0x78, 0x36, 0xd8, 0xd3, 0xab, 0x08, 0x8c, 0xd4, 0x4c, 0x21, 0x4d, 0x6a,
	0xc8, 0x56, 0xe2, 0xa0, 0x07, 0xf4, 0x4f, 0x83, 0x74, 0x33, 0x37, 0x37,
	0x1a, 0xdd, 0x8e, 0x03, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73,
	0x52, 0x44, 0x49, 0x60, 0x00, 0x06
};

static u8 C_CV_CA_CS_AUT_cert_1[] = {
        0x7f, 0x21, 0x81, 0xce, 0x5f, 0x37, 0x81, 0x80, 0x7a, 0xa0, 0x6c, 0x96,
        0x5e, 0x8f, 0xb2, 0x19, 0x61, 0xcf, 0xd4, 0x49, 0xd0, 0x9b, 0x9d, 0xaf,
        0x03, 0x04, 0x73, 0x01, 0x15, 0x69, 0x70, 0xb7, 0x73, 0xf1, 0x9c, 0x40,
        0xf1, 0x27, 0xd3, 0x38, 0xe3, 0xc1, 0x35, 0xeb, 0x21, 0x20, 0x56, 0x6d,
        0xc6, 0xf9, 0xf7, 0x45, 0xff, 0xb8, 0xf8, 0xe2, 0xb6, 0x1e, 0xe8, 0x16,
        0x6f, 0xfd, 0x06, 0xd2, 0x8c, 0xb4, 0x8c, 0x15, 0x2a, 0x1f, 0xa4, 0xf7,
        0xe9, 0xf6, 0x09, 0xd7, 0x52, 0x76, 0x33, 0x1c, 0xb7, 0x00, 0xb8, 0x4e,
        0x36, 0xac, 0x8a, 0x0a, 0x77, 0x74, 0x46, 0x8c, 0x3c, 0xf3, 0xd1, 0x47,
        0xa4, 0x9c, 0x97, 0x6e, 0x17, 0xab, 0x02, 0xda, 0x03, 0xea, 0x4a, 0xc1,
        0x51, 0x77, 0x7e, 0xdf, 0xbc, 0x35, 0xc2, 0x7d, 0x56, 0xfb, 0xa6, 0x85,
        0x75, 0x6e, 0xd6, 0x52, 0x85, 0x1d, 0xfd, 0xe7, 0x01, 0xbf, 0x87, 0x49,
        0x92, 0xdd, 0x4d, 0xe8, 0x5f, 0x38, 0x3d, 0x33, 0xe3, 0xd5, 0x2a, 0x4b,
        0x09, 0x40, 0xe3, 0x90, 0xcd, 0x1a, 0x64, 0x1f, 0xea, 0x2e, 0x9c, 0xdd,
        0x79, 0xd3, 0x87, 0x2d, 0xd6, 0xc5, 0x08, 0xd5, 0xef, 0x23, 0x9c, 0xb0,
        0x7e, 0xb5, 0x55, 0x68, 0xce, 0x18, 0x8b, 0x65, 0x13, 0xac, 0xb8, 0x84,
        0x14, 0xc9, 0xad, 0xf7, 0xa6, 0x4e, 0x2c, 0xc0, 0xb3, 0x14, 0xd1, 0x27,
        0x54, 0xae, 0xee, 0x67, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73,
        0x52, 0x44, 0x49, 0x62, 0x00, 0x18
};

/**
 * Terminal (IFD) certificate in CVC format (PK.IFD.AUT) (dnieRealParamData->c-CV-IFD-AUT)
 */
static u8 C_CV_IFDUser_AUT_cert_0[] = {
	0x7f, 0x21, 0x81, 0xcd, 0x5f, 0x37, 0x81, 0x80, 0x82, 0x5b, 0x69, 0xc6,
	0x45, 0x1e, 0x5f, 0x51, 0x70, 0x74, 0x38, 0x5f, 0x2f, 0x17, 0xd6, 0x4d,
	0xfe, 0x2e, 0x68, 0x56, 0x75, 0x67, 0x09, 0x4b, 0x57, 0xf3, 0xc5, 0x78,
	0xe8, 0x30, 0xe4, 0x25, 0x57, 0x2d, 0xe8, 0x28, 0xfa, 0xf4, 0xde, 0x1b,
	0x01, 0xc3, 0x94, 0xe3, 0x45, 0xc2, 0xfb, 0x06, 0x29, 0xa3, 0x93, 0x49,
	0x2f, 0x94, 0xf5, 0x70, 0xb0, 0x0b, 0x1d, 0x67, 0x77, 0x29, 0xf7, 0x55,
	0xd1, 0x07, 0x02, 0x2b, 0xb0, 0xa1, 0x16, 0xe1, 0xd7, 0xd7, 0x65, 0x9d,
	0xb5, 0xc4, 0xac, 0x0d, 0xde, 0xab, 0x07, 0xff, 0x04, 0x5f, 0x37, 0xb5,
	0xda, 0xf1, 0x73, 0x2b, 0x54, 0xea, 0xb2, 0x38, 0xa2, 0xce, 0x17, 0xc9,
	0x79, 0x41, 0x87, 0x75, 0x9c, 0xea, 0x9f, 0x92, 0xa1, 0x78, 0x05, 0xa2,
	0x7c, 0x10, 0x15, 0xec, 0x56, 0xcc, 0x7e, 0x47, 0x1a, 0x48, 0x8e, 0x6f,
	0x1b, 0x91, 0xf7, 0xaa, 0x5f, 0x38, 0x3c, 0xad, 0xfc, 0x12, 0xe8, 0x56,
	0xb2, 0x02, 0x34, 0x6a, 0xf8, 0x22, 0x6b, 0x1a, 0x88, 0x21, 0x37, 0xdc,
	0x3c, 0x5a, 0x57, 0xf0, 0xd2, 0x81, 0x5c, 0x1f, 0xcd, 0x4b, 0xb4, 0x6f,
	0xa9, 0x15, 0x7f, 0xdf, 0xfd, 0x79, 0xec, 0x3a, 0x10, 0xa8, 0x24, 0xcc,
	0xc1, 0xeb, 0x3c, 0xe0, 0xb6, 0xb4, 0x39, 0x6a, 0xe2, 0x36, 0x59, 0x00,
	0x16, 0xba, 0x69, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73, 0x53,
	0x44, 0x49, 0x60, 0x00, 0x06
};

static u8 C_CV_IFDUser_AUT_cert_1[] = {
        0x7f, 0x21, 0x81, 0xcd, 0x5f, 0x37, 0x81, 0x80, 0x5d, 0xa9, 0x4b, 0x6b,
        0x4e, 0xb8, 0x61, 0xec, 0xa6, 0x36, 0xd2, 0x67, 0x39, 0x74, 0x71, 0x1f,
        0x55, 0x63, 0x0f, 0x5b, 0x89, 0x03, 0x8c, 0x57, 0xd0, 0xbb, 0xbb, 0xc1,
        0xd2, 0xc6, 0x8c, 0xc3, 0xeb, 0x56, 0xd5, 0x30, 0x38, 0x00, 0xf5, 0xa9,
        0xf5, 0xe2, 0x96, 0x7f, 0xdf, 0x28, 0x91, 0x7b, 0xaf, 0xc8, 0x87, 0x63,
        0xb8, 0xec, 0x2c, 0x0e, 0xbe, 0x7a, 0xcb, 0x0b, 0xa4, 0xaf, 0xbf, 0xe6,
        0x6d, 0xb2, 0xa1, 0xed, 0xa1, 0x3e, 0x45, 0x64, 0xf7, 0x8e, 0x65, 0x58,
        0x6e, 0x51, 0x01, 0x76, 0xf1, 0x1c, 0x4c, 0x99, 0x36, 0x4a, 0xaf, 0x18,
        0x97, 0xd1, 0x1b, 0xf9, 0x8e, 0x9d, 0x1d, 0x0a, 0x12, 0xd0, 0x6a, 0xab,
        0x75, 0x76, 0x4a, 0xa8, 0xdc, 0x85, 0x8d, 0xf0, 0xf0, 0x03, 0xeb, 0x8b,
        0x4b, 0x3b, 0x56, 0xf5, 0xf9, 0x5f, 0xa6, 0x37, 0x53, 0x75, 0x19, 0xe4,
        0xc6, 0x55, 0x10, 0xf7, 0x5f, 0x38, 0x3c, 0x60, 0x2d, 0xd4, 0xba, 0x04,
        0xdb, 0xc9, 0x85, 0x88, 0x45, 0xe6, 0xa9, 0xc4, 0x05, 0x5b, 0xc5, 0xbf,
        0xa0, 0xed, 0xdb, 0x86, 0x67, 0x89, 0xf0, 0xec, 0x6a, 0x80, 0xfc, 0xe5,
        0x3c, 0x66, 0x08, 0xdf, 0xdc, 0x9b, 0x9f, 0xe2, 0xed, 0x56, 0x75, 0x2c,
        0xc6, 0x05, 0x51, 0x3b, 0xa3, 0xf1, 0x75, 0x9c, 0xdd, 0x95, 0x22, 0x75,
        0x3f, 0x18, 0xd7, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73, 0x53,
        0x44, 0x49, 0x62, 0x00, 0x18
};

/**
 * Terminal (IFD) certificate in CVC format (PK.IFD.AUT) for the PIN channel in DNIe 3.0 (dnie30RealParamDataPIN->c-CV-IFD-AUT)
 */
static u8 C_CV_IFDUser_AUT_pin_cert_0[] = {
	0x7f, 0x21, 0x81, 0xcd, 0x5f, 0x37, 0x81, 0x80, 0x69, 0xc4, 0xe4, 0x94,
	0xf0, 0x08, 0xe2, 0x42, 0x14, 0xb1, 0xc1, 0x31, 0xb6, 0x1f, 0xce, 0x9c,
	0x15, 0xfa, 0x3c, 0xb0, 0x61, 0xdd, 0x6f, 0x02, 0xd8, 0xa2, 0xcd, 0x30,
	0xd7, 0x2f, 0xb6, 0xdf, 0x89, 0x9a, 0xf1, 0x5b, 0x71, 0x78, 0x21, 0xbf,
	0xb1, 0xaf, 0x7d, 0x75, 0x85, 0x01, 0x6d, 0x8c, 0x36, 0xaf, 0x4a, 0xc2,
	0xa0, 0xb0, 0xc5, 0x2a, 0xd6, 0x5b, 0x69, 0x25, 0x67, 0x31, 0xc3, 0x4d,
	0x59, 0x02, 0x0e, 0x87, 0xab, 0x73, 0xa2, 0x30, 0xfa, 0x69, 0xee, 0x82,
	0xb3, 0x3a, 0x31, 0xdf, 0x04, 0x0c, 0xe9, 0x0f, 0x0a, 0xfc, 0x3a, 0x11,
	0x1d, 0x35, 0xda, 0x95, 0x66, 0xa8, 0xcd, 0xab, 0xea, 0x0e, 0x3f, 0x75,
	0x94, 0xc4, 0x40, 0xd3, 0x74, 0x50, 0x7a, 0x94, 0x35, 0x57, 0x59, 0xb3,
	0x9e, 0xc5, 0xe5, 0xfc, 0xb8, 0x03, 0x8d, 0x79, 0x3d, 0x5f, 0x9b, 0xa8,
	0xb5, 0xb1, 0x0b, 0x70, 0x5f, 0x38, 0x3c, 0x4c, 0x86, 0x91, 0xc7, 0xbe,
	0x2f, 0xd8, 0xc1, 0x23, 0x66, 0x0e, 0x98, 0x65, 0xe1, 0x4f, 0x19, 0xdf,
	0xfb, 0xb7, 0xff, 0x38, 0x08, 0xc9, 0xf2, 0x04, 0xe7, 0x97, 0xd0, 0x6d,
	0xd8, 0x33, 0x3a, 0xc5, 0x83, 0x86, 0xee, 0x4e, 0xb6, 0x1e, 0x20, 0xec,
	0xa7, 0xef, 0x38, 0xd5, 0xb0, 0x5e, 0xb1, 0x15, 0x96, 0x6a, 0x5a, 0x89,
	0xad, 0x58, 0xa5, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73, 0x53,
	0x44, 0x49, 0x60, 0x00, 0x06
};

static u8 C_CV_IFDUser_AUT_pin_cert_1[] = {
        0x7f, 0x21, 0x81, 0xcd, 0x5f, 0x37, 0x81, 0x80, 0x0a, 0x3d, 0xb4, 0xd1,
        0x57, 0x98, 0xf2, 0x34, 0xf6, 0x31, 0xfd, 0x94, 0xc9, 0x1d, 0x2a, 0x63,
        0x63, 0xd0, 0xe1, 0x8e, 0x1b, 0x56, 0xda, 0xbd, 0xe6, 0x22, 0xbc, 0x20,
        0x1f, 0xd7, 0xc7, 0xff, 0x59, 0xff, 0x66, 0xda, 0x6e, 0x43, 0x4f, 0xe2,
        0xf7, 0xf4, 0x6e, 0x42, 0xe4, 0xa6, 0x06, 0xea, 0x82, 0x39, 0xac, 0x1a,
        0xc3, 0x0c, 0x7d, 0xad, 0xe2, 0x29, 0x65, 0xdf, 0x60, 0x6d, 0x11, 0x5e,
        0x04, 0xc8, 0xef, 0xfc, 0x77, 0x2b, 0x8f, 0x5d, 0x48, 0x77, 0x3e, 0x34,
        0x95, 0x5f, 0x33, 0xf4, 0x64, 0xed, 0x85, 0xcc, 0x0e, 0xb1, 0xbc, 0x57,
        0x2a, 0xfa, 0xba, 0x47, 0x25, 0xfb, 0xf5, 0xbd, 0xcf, 0x1d, 0x8c, 0x38,
        0xc9, 0xfe, 0x9c, 0xd8, 0x53, 0x6f, 0x34, 0x0b, 0xce, 0x14, 0x1d, 0xf5,
        0x18, 0x7f, 0xa2, 0xe2, 0x37, 0x2d, 0x73, 0xbc, 0x7f, 0x89, 0x48, 0x35,
        0x0c, 0xba, 0xde, 0xf2, 0x5f, 0x38, 0x3c, 0x0d, 0xcc, 0x88, 0x8d, 0x47,
        0x96, 0x54, 0x3f, 0x03, 0x25, 0x4f, 0x4e, 0x2c, 0xdf, 0x98, 0xb1, 0xe1,
        0x26, 0x11, 0xe3, 0x98, 0x1f, 0x53, 0x33, 0xdf, 0x98, 0xc8, 0x86, 0x01,
        0x93, 0x75, 0x84, 0x0f, 0xac, 0x61, 0xdb, 0x8f, 0x1b, 0xa3, 0xb5, 0x43,
        0xdc, 0xea, 0x3d, 0x05, 0x9e, 0x6a, 0x41, 0x4f, 0x6d, 0xd2, 0x9f, 0xc7,
        0xc9, 0x9d, 0x8b, 0x00, 0x01, 0x00, 0x01, 0x42, 0x08, 0x65, 0x73, 0x53,
        0x44, 0x49, 0x62, 0x00, 0x18
};

/**
 * Root CA card key reference (pk-RCA-AUT-keyRef)
 */
static u8 root_ca_keyref[] = { 0x02, 0x0f };


/**
 * ICC card private key reference (sk-ICC-AUT-keyRef)
 */
static u8 icc_priv_keyref[] = { 0x02, 0x1f };

/**
 * Intermediate CA card key reference (ifd-keyRef)
 */
static u8 cvc_intca_keyref_0[] = { 0x65, 0x73, 0x53, 0x44, 0x49, 0x60, 0x00, 0x06 };
static u8 cvc_intca_keyref_1[] = { 0x65, 0x73, 0x53, 0x44, 0x49, 0x62, 0x00, 0x18 };

/**
 * In memory key reference for selecting IFD sent certificate (dnieRealParamData->pk-IFD-AUT-keyRef)
 */
static u8 cvc_ifd_keyref_0[] = { 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static u8 cvc_ifd_keyref_1[] = { 0x00, 0x00, 0x00, 0x00, 0xd0, 0x02, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x02 };

/**
 * In memory key reference for selecting IFD sent certificate in PIN channel DNIe 3.0 (dnie30RealParamDataPIN->pk-IFD-AUT-keyRef)
 */
static u8 cvc_ifd_keyref_pin_0[] = { 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static u8 cvc_ifd_keyref_pin_1[] = { 0x00, 0x00, 0x00, 0x00, 0xd0, 0x02, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x04 };

/**
 * Serial number for IFD Terminal application (dnieRealParamData->sn-IFD)
 */
static u8 sn_ifd_0[] = { 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static u8 sn_ifd_1[] = { 0xd0, 0x02, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x02 };

/**
 * Serial number for IFD Terminal application in PIN channel DNIe 3.0 (dnie30RealParamDataPIN->sn-IFD)
 */
static u8 sn_ifd_pin_0[] = { 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
static u8 sn_ifd_pin_1[] = { 0xd0, 0x02, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x04 };

#define AC_RAIZ_COMPONENTES_OLD_IDX 0
#define AC_RAIZ_COMPONENTES_ISSUER "/C=ES/O=DIRECCION GENERAL DE LA POLICIA/OU=DNIE/OU=AC RAIZ COMPONENTES/CN=000000006573524449600006"
#define AC_RAIZ_COMPONENTES_2_NEW_IDX 1
#define AC_RAIZ_COMPONENTES_2_ISSUER "/C=ES/O=DIRECCION GENERAL DE LA POLICIA/OU=DNIE/organizationIdentifier=VATES-S2816015H/OU=AC RAIZ COMPONENTES 2/CN=000000006573524449620018"
#define AC_RAIZ_COMPONENTES_2_ISSUER_OU "/OU=AC RAIZ COMPONENTES 2/"

/**
 * The DNIe secure channel uses some static configuration.
 * Since DNIe 'BMP100001' it seems that the old values were
 * replaced by new certs and keys. So an array of configuration
 * values is going to be added that will be set to the card
 * private data. For the moment the issuer of the icc intermediate
 * CA cert will be used to assign one or the other array element.
 */
static dnie_channel_data_t channel_data[] = {
    {  /* AC_RAIZ_COMPONENTES_OLD_IDX: Channel data configuration for DNIe before BMP100001 */
        .icc_root_ca = {
            .modulus = { icc_root_ca_modulus_0, sizeof(icc_root_ca_modulus_0) },
            .exponent = { icc_root_ca_public_exponent, sizeof(icc_root_ca_public_exponent) }
        },
        .ifd = {
            .modulus = { ifd_modulus_0, sizeof(ifd_modulus_0) },
            .exponent = { ifd_public_exponent, sizeof(ifd_public_exponent) },
            .private = { ifd_private_exponent_0, sizeof(ifd_private_exponent_0) }
        },
        .ifd_pin = {
            .modulus = { .value = ifd_pin_modulus_0, sizeof(ifd_pin_modulus_0) },
            .exponent = { .value = ifd_pin_public_exponent, sizeof(ifd_pin_public_exponent) },
            .private = { .value = ifd_pin_private_exponent_0, sizeof(ifd_pin_private_exponent_0) }
        },
        .C_CV_CA_CS_AUT_cert = { .value = C_CV_CA_CS_AUT_cert_0, sizeof(C_CV_CA_CS_AUT_cert_0) },
        .C_CV_IFDUser_AUT_cert = { .value = C_CV_IFDUser_AUT_cert_0, sizeof(C_CV_IFDUser_AUT_cert_0) },
        .C_CV_IFDUser_AUT_pin_cert = { .value = C_CV_IFDUser_AUT_pin_cert_0, sizeof(C_CV_IFDUser_AUT_pin_cert_0) },
        .root_ca_keyref = { root_ca_keyref, sizeof(root_ca_keyref) },
        .icc_priv_keyref = { icc_priv_keyref, sizeof(icc_priv_keyref) },
        .cvc_intca_keyref = { cvc_intca_keyref_0, sizeof(cvc_intca_keyref_0) },
        .cvc_ifd_keyref = { cvc_ifd_keyref_0, sizeof(cvc_ifd_keyref_0) },
        .cvc_ifd_keyref_pin = { cvc_ifd_keyref_pin_0, sizeof(cvc_ifd_keyref_pin_0) },
        .sn_ifd = { sn_ifd_0, sizeof(sn_ifd_0) },
        .sn_ifd_pin = { sn_ifd_pin_0, sizeof(sn_ifd_pin_0) }
    },
    { /* AC_RAIZ_COMPONENTES_2_NEW_IDX: Channel data configuration for DNIe BMP100001 and newer */
        .icc_root_ca = {
            .modulus = { icc_root_ca_modulus_1, sizeof(icc_root_ca_modulus_1) },
            .exponent = { icc_root_ca_public_exponent, sizeof(icc_root_ca_public_exponent) }
        },
        .ifd = {
            .modulus = { ifd_modulus_1, sizeof(ifd_modulus_1) },
            .exponent = { ifd_public_exponent, sizeof(ifd_public_exponent) },
            .private = { ifd_private_exponent_1, sizeof(ifd_private_exponent_1) }
        },
        .ifd_pin = {
            .modulus = { .value = ifd_pin_modulus_1, sizeof(ifd_pin_modulus_1) },
            .exponent = { .value = ifd_pin_public_exponent, sizeof(ifd_pin_public_exponent) },
            .private = { .value = ifd_pin_private_exponent_1, sizeof(ifd_pin_private_exponent_1) }
        },
        .C_CV_CA_CS_AUT_cert = { .value = C_CV_CA_CS_AUT_cert_1, sizeof(C_CV_CA_CS_AUT_cert_1) },
        .C_CV_IFDUser_AUT_cert = { .value = C_CV_IFDUser_AUT_cert_1, sizeof(C_CV_IFDUser_AUT_cert_1) },
        .C_CV_IFDUser_AUT_pin_cert = { .value = C_CV_IFDUser_AUT_pin_cert_1, sizeof(C_CV_IFDUser_AUT_pin_cert_1) },
        .root_ca_keyref = { root_ca_keyref, sizeof(root_ca_keyref) },
        .icc_priv_keyref = { icc_priv_keyref, sizeof(icc_priv_keyref) },
        .cvc_intca_keyref = { cvc_intca_keyref_1, sizeof(cvc_intca_keyref_1) },
        .cvc_ifd_keyref = { cvc_ifd_keyref_1, sizeof(cvc_ifd_keyref_1) },
        .cvc_ifd_keyref_pin = { cvc_ifd_keyref_pin_1, sizeof(cvc_ifd_keyref_pin_1) },
        .sn_ifd = { sn_ifd_1, sizeof(sn_ifd_1) },
        .sn_ifd_pin = { sn_ifd_pin_1, sizeof(sn_ifd_pin_1) }
    }
};

/************ internal functions **********************************/

/**
 * Select a file from card, process fci and read data.
 *
 * This is done by mean of iso_select_file() and iso_read_binary()
 *
 * @param card pointer to sc_card data
 * @param path pathfile
 * @param file pointer to resulting file descriptor
 * @param buffer pointer to buffer where to store file contents
 * @param length length of buffer data
 * @return SC_SUCCESS if ok; else error code
 */
int dnie_read_file(sc_card_t * card,
		   const sc_path_t * path,
		   sc_file_t ** file, u8 ** buffer, size_t * length)
{
	u8 *data = NULL;
	char *msg = NULL;
	int res = SC_SUCCESS;
	size_t fsize = 0;	/* file size */
	sc_context_t *ctx = NULL;

	if (!card || !card->ctx)
		return SC_ERROR_INVALID_ARGUMENTS;
	ctx = card->ctx;
	LOG_FUNC_CALLED(card->ctx);
	if (!buffer || !length || !path)	/* check received arguments */
		LOG_FUNC_RETURN(ctx, SC_ERROR_INVALID_ARGUMENTS);
	/* select file by mean of iso7816 ops */
	res = card->ops->select_file(card, path, file);
	if (res != SC_SUCCESS || !file || !(*file)) {
		msg = "select_file failed";
		goto dnie_read_file_err;
	}
	/* iso's select file calls if needed process_fci, so arriving here
	 * we have file structure filled.
	 */
	if ((*file)->type == SC_FILE_TYPE_DF) {
		/* just a DF, no need to read_binary() */
		*buffer = NULL;
		*length = 0;
		res = SC_SUCCESS;
		msg = "File is a DF: no need to read_binary()";
		goto dnie_read_file_end;
	}
	fsize = (*file)->size;
	/* reserve enough space to read data from card */
	if (fsize <= 0) {
		res = SC_ERROR_FILE_TOO_SMALL;
		msg = "provided buffer size is too small";
		goto dnie_read_file_err;
	}
	data = calloc(fsize, sizeof(u8));
	if (data == NULL) {
		res = SC_ERROR_OUT_OF_MEMORY;
		msg = "cannot reserve requested buffer size";
		goto dnie_read_file_err;
	}
	/* call sc_read_binary() to retrieve data */
	sc_log(ctx, "read_binary(): expected '%"SC_FORMAT_LEN_SIZE_T"u' bytes",
	       fsize);
	res = sc_read_binary(card, 0, data, fsize, 0L);
	if (res < 0) {		/* read_binary returns number of bytes read */
		res = SC_ERROR_CARD_CMD_FAILED;
		msg = "read_binary() failed";
		goto dnie_read_file_err;
	}
	*buffer = data;
	*length = res;
	/* arriving here means success */
	res = SC_SUCCESS;
	goto dnie_read_file_end;
 dnie_read_file_err:
	if (data)
		free(data);
	if (file) {
		sc_file_free(*file);
		*file = NULL;
	}
 dnie_read_file_end:
	if (msg)
		sc_log(ctx, "%s", msg);
	LOG_FUNC_RETURN(ctx, res);
}

/**
 * Read SM required certificates from card.
 *
 * This function uses received path to read a certificate file from
 * card.
 * No validation is done except that received data is effectively a certificate
 * @param card Pointer to card driver structure
 * @param certpat path to requested certificate
 * @param cert where to store resulting data
 * @return SC_SUCCESS if ok, else error code
 */
static int dnie_read_certificate(sc_card_t * card, char *certpath, X509 ** cert)
{
	sc_file_t *file = NULL;
	sc_path_t path;
	u8 *buffer = NULL, *buffer2 = NULL;
	char *msg = NULL;
	size_t bufferlen = 0;
	int res = SC_SUCCESS;

	LOG_FUNC_CALLED(card->ctx);
	sc_format_path(certpath, &path);
	res = dnie_read_file(card, &path, &file, &buffer, &bufferlen);
	if (res != SC_SUCCESS) {
		msg = "Cannot get intermediate CA cert";
		goto read_cert_end;
	}
	buffer2 = buffer;
	*cert = d2i_X509(NULL, (const unsigned char **)&buffer2, bufferlen);
	if (*cert == NULL) {	/* received data is not a certificate */
		res = SC_ERROR_OBJECT_NOT_VALID;
		msg = "Read data is not a certificate";
		goto read_cert_end;
	}
	res = SC_SUCCESS;

 read_cert_end:
	if (buffer) {
		free(buffer);
		buffer = NULL;
		bufferlen = 0;
	}
	sc_file_free(file);
	file = NULL;
	if (msg)
		sc_log(card->ctx, "%s", msg);
	LOG_FUNC_RETURN(card->ctx, res);
}

/**
 * Method that sets the configuration channel data to use.
 * The configuration data is already set to the card private data.
 * Just created in case this will be modified.
 *
 * @param card Pointer to card driver structure
 * @param data The data for the channel will be assigned here
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_channel_data(sc_card_t * card, dnie_channel_data_t ** data) {
	dnie_private_data_t *priv_data = GET_DNIE_PRIV_DATA(card);
	LOG_FUNC_CALLED(card->ctx);
	if (!priv_data->channel_data) {
		sc_log(card->ctx, "Data channel configuration was not initialized");
		LOG_FUNC_RETURN(card->ctx, SC_ERROR_INTERNAL);
	}
	*data = priv_data->channel_data;
	LOG_FUNC_RETURN(card->ctx, SC_SUCCESS);
}

/**
 * Method to assign into the private data the secure channel
 * configuration to use. Right now the icc_intermediate_ca_cert
 * issuer is used. If it is the new one the new data is assigned
 * else the old data is set.
 *
 * @param card Pointer to card driver structure
 * @param icc_intermediate_ca_cert Pointer to the X509 icc intermediate CA certificate
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_set_channel_data(sc_card_t * card, X509 * icc_intermediate_ca_cert) {
	char *buf = NULL;
	dnie_private_data_t *priv_data = GET_DNIE_PRIV_DATA(card);
	LOG_FUNC_CALLED(card->ctx);

	X509_NAME *issuer = X509_get_issuer_name(icc_intermediate_ca_cert);
	if (issuer) {
		buf = X509_NAME_oneline(issuer, buf, 0);
		if (!buf) {
			LOG_FUNC_RETURN(card->ctx, SC_ERROR_OUT_OF_MEMORY);
		}
		sc_log(card->ctx, "icc_intermediate_ca_cert issuer %s", buf);
	}

	if (buf && strstr(buf, AC_RAIZ_COMPONENTES_2_ISSUER_OU)) {
		sc_log(card->ctx, "assigning new data channel configuration");
		priv_data->channel_data = &channel_data[AC_RAIZ_COMPONENTES_2_NEW_IDX];
	} else {
		sc_log(card->ctx, "assigning old data channel configuration");
		priv_data->channel_data = &channel_data[AC_RAIZ_COMPONENTES_OLD_IDX];
	}
	if (buf) {
		OPENSSL_free(buf);
	}
	LOG_FUNC_RETURN(card->ctx, SC_SUCCESS);
}

/************ implementation of cwa provider methods **************/

/**
 * Retrieve Root CA public key.
 *
 * Just returns (as local SM authentication) static data
 * @param card Pointer to card driver structure
 * @param root_ca_key pointer to resulting returned key
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_root_ca_pubkey(sc_card_t * card, EVP_PKEY ** root_ca_key)
{
	int res = SC_SUCCESS;
	BIGNUM *root_ca_rsa_n = NULL, *root_ca_rsa_e = NULL;
	dnie_channel_data_t *data;
#if OPENSSL_VERSION_NUMBER < 0x30000000L
	RSA *root_ca_rsa = NULL;
	root_ca_rsa = RSA_new();
	*root_ca_key = EVP_PKEY_new();
	if (!root_ca_rsa || !*root_ca_key) {
		if (root_ca_rsa)
			RSA_free(root_ca_rsa);
		if (*root_ca_key)
			EVP_PKEY_free(*root_ca_key);
#else
	EVP_PKEY_CTX *ctx = NULL;
	OSSL_PARAM_BLD *bld = NULL;
	OSSL_PARAM *params = NULL;

	ctx = EVP_PKEY_CTX_new_from_name(card->ctx->ossl3ctx->libctx, "RSA", NULL);
	if (!ctx) {
#endif
		sc_log(card->ctx, "Cannot create data for root CA public key");
		return SC_ERROR_OUT_OF_MEMORY;
	}

	LOG_FUNC_CALLED(card->ctx);

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	if (res < 0) {
#if OPENSSL_VERSION_NUMBER < 0x30000000L
		RSA_free(root_ca_rsa);
		EVP_PKEY_free(*root_ca_key);
#else
		EVP_PKEY_CTX_free(ctx);
#endif
	}
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	/* compose root_ca_public key with data provided by Dnie Manual */
	root_ca_rsa_n = BN_bin2bn(data->icc_root_ca.modulus.value, data->icc_root_ca.modulus.len, NULL);
	root_ca_rsa_e = BN_bin2bn(data->icc_root_ca.exponent.value, data->icc_root_ca.exponent.len, NULL);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
	if (RSA_set0_key(root_ca_rsa, root_ca_rsa_n, root_ca_rsa_e, NULL) != 1) {
		BN_free(root_ca_rsa_n);
		BN_free(root_ca_rsa_e);
		EVP_PKEY_free(*root_ca_key);
		RSA_free(root_ca_rsa);
		sc_log(card->ctx, "Cannot set RSA values for CA public key");
		return SC_ERROR_INTERNAL;
	}
	res = EVP_PKEY_assign_RSA(*root_ca_key, root_ca_rsa);
	if (!res) {
		RSA_free(root_ca_rsa);
#else
	if (!(bld = OSSL_PARAM_BLD_new()) ||
		OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, root_ca_rsa_n) != 1 ||
		OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, root_ca_rsa_e) != 1 ||
		!(params = OSSL_PARAM_BLD_to_param(bld))) {
		OSSL_PARAM_BLD_free(bld);
		OSSL_PARAM_free(params);
		EVP_PKEY_CTX_free(ctx);
		sc_log(card->ctx, "Cannot set RSA values for CA public key");
		return SC_ERROR_INTERNAL;
	}
	OSSL_PARAM_BLD_free(bld);

	if (EVP_PKEY_fromdata_init(ctx) != 1 ||
		EVP_PKEY_fromdata(ctx, root_ca_key, EVP_PKEY_PUBLIC_KEY, params) != 1) {
		EVP_PKEY_CTX_free(ctx);
		OSSL_PARAM_free(params);
#endif
		BN_free(root_ca_rsa_n);
		BN_free(root_ca_rsa_e);
		EVP_PKEY_free(*root_ca_key);
		sc_log(card->ctx, "Cannot compose root CA public key");
		return SC_ERROR_INTERNAL;
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY_CTX_free(ctx);
	OSSL_PARAM_free(params);
	BN_free(root_ca_rsa_n);
	BN_free(root_ca_rsa_e);
#endif
	LOG_FUNC_RETURN(card->ctx, SC_SUCCESS);
}

/**
 * Retrieve IFD (application) CVC intermediate CA certificate and length.
 *
 * Returns a byte array with the intermediate CA certificate
 * (in CardVerifiable Certificate format) to be sent to the
 * card in External Authentication process
 * As this is local provider, just points to provided static data,
 * and always return success
 *
 * @param card Pointer to card driver Certificate
 * @param cert Where to store resulting byte array
 * @param length len of returned byte array
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_cvc_ca_cert(sc_card_t * card, u8 ** cert, size_t * length)
{
	int res;
	dnie_channel_data_t *data;
	LOG_FUNC_CALLED(card->ctx);

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*cert = data->C_CV_CA_CS_AUT_cert.value;
	*length = data->C_CV_CA_CS_AUT_cert.len;
	LOG_FUNC_RETURN(card->ctx, res);
}

/**
 * Retrieve IFD (application) CVC certificate and length.
 *
 * Returns a byte array with the application's certificate
 * (in CardVerifiable Certificate format) to be sent to the
 * card in External Authentication process
 * As this is local provider, just points to provided static data,
 * and always return success
 *
 * @param card Pointer to card driver Certificate
 * @param cert Where to store resulting byte array
 * @param length len of returned byte array
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_cvc_ifd_cert(sc_card_t * card, u8 ** cert, size_t * length)
{
	int res;
	dnie_channel_data_t *data;
	LOG_FUNC_CALLED(card->ctx);

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*cert = data->C_CV_IFDUser_AUT_cert.value;
	*length = data->C_CV_IFDUser_AUT_cert.len;
	LOG_FUNC_RETURN(card->ctx, res);
}

/**
 * Retrieve IFD (application) CVC certificate and length for
 * the PIN channel.
 *
 * Returns a byte array with the application's certificate
 * (in CardVerifiable Certificate format) to be sent to the
 * card in External Authentication process
 * As this is local provider, just points to provided static data,
 * and always return success
 *
 * @param card Pointer to card driver Certificate
 * @param cert Where to store resulting byte array
 * @param length len of returned byte array
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_cvc_ifd_cert_pin(sc_card_t * card, u8 ** cert, size_t * length)
{
	int res;
	dnie_channel_data_t *data;
	LOG_FUNC_CALLED(card->ctx);

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*cert = data->C_CV_IFDUser_AUT_pin_cert.value;
	*length = data->C_CV_IFDUser_AUT_pin_cert.len;
	LOG_FUNC_RETURN(card->ctx, res);
}

/**
 * Get IFD (Terminal) private key data passing the three
 * arguments (modulus, public and private exponent).
 *
 * @param card pointer to card driver structure
 * @param ifd_privkey where to store IFD private key
 * @param modulus the byte array used as the modulus of the key
 * @param modulus_len the length of the modulus
 * @param public_exponent the byte array for the public exponent
 * @param public_exponent_len the length of the public exponent
 * @param private_exponent the byte array for the private exponent
 * @param private_exponent_len the length of the private exponent
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_privkey(sc_card_t * card, EVP_PKEY ** ifd_privkey,
                            u8 * modulus, int modulus_len,
                            u8 * public_exponent, int public_exponent_len,
                            u8 * private_exponent, int private_exponent_len)
{
	BIGNUM *ifd_rsa_n = NULL, *ifd_rsa_e = NULL, *ifd_rsa_d = NULL;

#if OPENSSL_VERSION_NUMBER < 0x30000000L
	int res = SC_ERROR_INTERNAL;
	RSA *ifd_rsa = NULL;

	LOG_FUNC_CALLED(card->ctx);
	ifd_rsa = RSA_new();
	*ifd_privkey = EVP_PKEY_new();

	if (!ifd_rsa || !*ifd_privkey) {
		if (ifd_rsa)
			RSA_free(ifd_rsa);
		if (*ifd_privkey)
			EVP_PKEY_free(*ifd_privkey);
#else
	OSSL_PARAM_BLD *bld = NULL;
	OSSL_PARAM *params = NULL;
	EVP_PKEY_CTX *ctx = NULL;

	LOG_FUNC_CALLED(card->ctx);
	ctx = EVP_PKEY_CTX_new_from_name(card->ctx->ossl3ctx->libctx, "RSA", NULL);

	if (!ctx) { 
#endif
		sc_log(card->ctx, "Cannot create data for IFD private key");
		return SC_ERROR_OUT_OF_MEMORY;
	}

	/* compose ifd_private key with data provided in Annex 3 of DNIe Manual */
	ifd_rsa_n = BN_bin2bn(modulus, modulus_len, NULL);
	ifd_rsa_e = BN_bin2bn(public_exponent, public_exponent_len, NULL);
	ifd_rsa_d = BN_bin2bn(private_exponent, private_exponent_len, NULL);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
	if (RSA_set0_key(ifd_rsa, ifd_rsa_n, ifd_rsa_e, ifd_rsa_d) != 1) {
		BN_free(ifd_rsa_n);
		BN_free(ifd_rsa_e);
		BN_free(ifd_rsa_d);
		RSA_free(ifd_rsa);
		EVP_PKEY_free(*ifd_privkey);
		sc_log(card->ctx, "Cannot set RSA values for IFD private key");
		return SC_ERROR_INTERNAL;
	}

	res = EVP_PKEY_assign_RSA(*ifd_privkey, ifd_rsa);
	if (!res) {
		RSA_free(ifd_rsa);
#else
	if (!(bld = OSSL_PARAM_BLD_new()) ||
		OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, ifd_rsa_n) != 1 ||
		OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, ifd_rsa_e) != 1 ||
		OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_D, ifd_rsa_d) != 1 ||
		!(params = OSSL_PARAM_BLD_to_param(bld))) {
		OSSL_PARAM_BLD_free(bld);
		OSSL_PARAM_free(params);
		EVP_PKEY_CTX_free(ctx);
		BN_free(ifd_rsa_n);
		BN_free(ifd_rsa_e);
		BN_free(ifd_rsa_d);
		sc_log(card->ctx, "Cannot set RSA values for CA public key");
		return SC_ERROR_INTERNAL;
	}
	OSSL_PARAM_BLD_free(bld);

	if (EVP_PKEY_fromdata_init(ctx) != 1 ||
		EVP_PKEY_fromdata(ctx, ifd_privkey, EVP_PKEY_KEYPAIR, params) != 1) {
		EVP_PKEY_CTX_free(ctx);
#endif
		BN_free(ifd_rsa_n);
		BN_free(ifd_rsa_e);
		BN_free(ifd_rsa_d);
		if (*ifd_privkey)
			EVP_PKEY_free(*ifd_privkey);	/* implies ifd_rsa free() */
		sc_log(card->ctx, "Cannot compose IFD private key");
		return SC_ERROR_INTERNAL;
	}
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_PARAM_free(params);
	EVP_PKEY_CTX_free(ctx);
	BN_free(ifd_rsa_n);
	BN_free(ifd_rsa_e);
	BN_free(ifd_rsa_d);
#endif
	LOG_FUNC_RETURN(card->ctx, SC_SUCCESS);
}

/**
 * Get IFD (Terminal) private key data
 *
 * As this is a local (in memory) provider, just get data specified in
 * DNIe's manual and compose an OpenSSL private key structure
 *
 * @param card pointer to card driver structure
 * @param ifd_privkey where to store IFD private key
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_ifd_privkey(sc_card_t * card, EVP_PKEY ** ifd_privkey)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	return dnie_get_privkey(card, ifd_privkey, data->ifd.modulus.value, data->ifd.modulus.len,
				data->ifd.exponent.value, data->ifd.exponent.len,
				data->ifd.private.value, data->ifd.private.len);
}

/**
 * Get IFD (Terminal) private key data for the PIN channel DNIe 3.0
 *
 * As this is a local (in memory) provider, just get data specified in
 * DNIe's manual and compose an OpenSSL private key structure
 *
 * @param card pointer to card driver structure
 * @param ifd_privkey where to store IFD private key
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_ifd_privkey_pin(sc_card_t * card, EVP_PKEY ** ifd_privkey)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	return dnie_get_privkey(card, ifd_privkey, data->ifd_pin.modulus.value, data->ifd_pin.modulus.len,
				data->ifd_pin.exponent.value, data->ifd_pin.exponent.len,
				data->ifd_pin.private.value, data->ifd_pin.private.len);
}

/**
 * Get ICC intermediate CA Certificate from card.
 *
 * @param card Pointer to card driver structure
 * @param cert where to store resulting certificate
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_icc_intermediate_ca_cert(sc_card_t * card, X509 ** cert)
{
	dnie_private_data_t *priv_data = GET_DNIE_PRIV_DATA(card);

	int res = dnie_read_certificate(card, "3F006020", cert);
	if (res == SC_SUCCESS && !priv_data->channel_data) {
		/* initialize the secure channel data using the issuer cert */
		res = dnie_set_channel_data(card, *cert);
	}
	return res;
}

/**
 * Get ICC (card) certificate.
 *
 * @param card Pointer to card driver structure
 * @param cert where to store resulting certificate
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_icc_cert(sc_card_t * card, X509 ** cert)
{
	return dnie_read_certificate(card, "3F00601F", cert);
}

/**
 * Retrieve key reference for Root CA to validate CVC intermediate CA certs.
 *
 * This is required in the process of On card external authenticate
 * @param card Pointer to card driver structure
 * @param buf where to store resulting key reference
 * @param len where to store buffer length
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_root_ca_pubkey_ref(sc_card_t * card, u8 ** buf,
				       size_t * len)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*buf = data->root_ca_keyref.value;
	*len = data->root_ca_keyref.len;
	return res;
}

/**
 * Retrieve public key reference for intermediate CA to validate IFD cert.
 *
 * This is required in the process of On card external authenticate
 * As this driver is for local SM authentication SC_SUCCESS is always returned
 *
 * @param card Pointer to card driver structure
 * @param buf where to store resulting key reference
 * @param len where to store buffer length
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_intermediate_ca_pubkey_ref(sc_card_t * card, u8 ** buf,
					       size_t * len)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*buf = data->cvc_intca_keyref.value;
	*len = data->cvc_intca_keyref.len;
	return res;
}

/**
 *  Retrieve public key reference for IFD certificate.
 *
 * This tells the card with in memory key reference is to be used
 * when CVC cert is sent for external auth procedure
 * As this driver is for local SM authentication SC_SUCCESS is always returned
 *
 * @param card pointer to card driver structure
 * @param buf where to store data to be sent
 * @param len where to store data length
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_ifd_pubkey_ref(sc_card_t * card, u8 ** buf, size_t * len)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*buf = data->cvc_ifd_keyref.value;
	*len = data->cvc_ifd_keyref.len;
	return res;
}

/**
 *  Retrieve public key reference for IFD certificate for the PIN channel.
 *
 * This tells the card with in memory key reference is to be used
 * when CVC cert is sent for external auth procedure
 * As this driver is for local SM authentication SC_SUCCESS is always returned
 *
 * @param card pointer to card driver structure
 * @param buf where to store data to be sent
 * @param len where to store data length
 * @return SC_SUCCESS if ok; else error code
 */
static int dnie_get_ifd_pubkey_ref_pin(sc_card_t * card, u8 ** buf, size_t * len)
{
	int res;
	dnie_channel_data_t *data;
	LOG_FUNC_CALLED(card->ctx);

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*buf = data->cvc_ifd_keyref_pin.value;
	*len = data->cvc_ifd_keyref_pin.len;
	return res;
}

/**
 * Retrieve key reference for ICC privkey.
 *
 * In local SM establishment, just retrieve key reference from static
 * data tables and just return success
 *
 * @param card pointer to card driver structure
 * @param buf where to store data
 * @param len where to store data length
 * @return SC_SUCCESS if ok; else error
 */
static int dnie_get_icc_privkey_ref(sc_card_t * card, u8 ** buf, size_t * len)
{
	int res;
	dnie_channel_data_t *data;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	*buf = data->icc_priv_keyref.value;
	*len = data->icc_priv_keyref.len;
	return res;
}

/**
 * Retrieve SN.IFD (8 bytes left padded with zeroes if required).
 *
 * In DNIe local SM procedure, just read it from static data and
 * return SC_SUCCESS
 *
 * @param card pointer to card structure
 * @param buf where to store result (8 bytes)
 * @return SC_SUCCESS if ok; else error
 */
static int dnie_get_sn_ifd(sc_card_t * card)
{
	int res;
	dnie_channel_data_t *data;
	struct sm_cwa_session * sm = &card->sm_ctx.info.session.cwa;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	memcpy(sm->ifd.sn, data->sn_ifd.value, data->sn_ifd.len);
	return res;
}

/**
 * Retrieve SN.IFD (8 bytes left padded with zeroes if required)
 * for the PIN channel DNIe 3.0.
 *
 * In DNIe local SM procedure, just read it from static data and
 * return SC_SUCCESS
 *
 * @param card pointer to card structure
 * @return SC_SUCCESS if ok; else error
 */
static int dnie_get_sn_ifd_pin(sc_card_t * card)
{
	int res;
	dnie_channel_data_t *data;
	struct sm_cwa_session * sm = &card->sm_ctx.info.session.cwa;

	/* obtain the data channel info for the card */
	res = dnie_get_channel_data(card, &data);
	LOG_TEST_RET(card->ctx, res, "Error getting the card channel data");

	memcpy(sm->ifd.sn, data->sn_ifd_pin.value, data->sn_ifd_pin.len);
	return res;
}

/* Retrieve SN.ICC (8 bytes left padded with zeroes if needed).
 *
 * As DNIe reads serial number at startup, no need to read again
 * Just retrieve it from cache and return success
 *
 * @param card pointer to card structure
 * @return SC_SUCCESS if ok; else error
 */
static int dnie_get_sn_icc(sc_card_t * card)
{
	int res=SC_SUCCESS;
	sc_serial_number_t serial;
	struct sm_cwa_session * sm = &card->sm_ctx.info.session.cwa;

	res = sc_card_ctl(card, SC_CARDCTL_GET_SERIALNR, &serial);
	LOG_TEST_RET(card->ctx, res, "Error in getting serial number");
	/* copy into sn_icc buffer.Remember that dnie sn has 7 bytes length */
	memset(sm->icc.sn, 0, sizeof(sm->icc.sn));
	memcpy(&sm->icc.sn[1], serial.value, 7);
	return SC_SUCCESS;
}

/**
 * CWA-14890 SM stablisment pre-operations.
 *
 * DNIe needs to get icc serial number at the begin of the sm creation
 * (to avoid breaking key references) so get it an store into serialnr
 * cache here.
 *
 * In this way if get_sn_icc is called(), we make sure that no APDU
 * command is to be sent to card, just retrieve it from cache
 *
 * @param card pointer to card driver structure
 * @param provider pointer to SM data provider for DNIe
 * @return SC_SUCCESS if OK. else error code
 */
static int dnie_create_pre_ops(sc_card_t * card, cwa_provider_t * provider)
{
	sc_serial_number_t serial;

	/* make sure that this cwa provider is used with a working DNIe card */
	if (card->type != SC_CARD_TYPE_DNIE_USER)
		LOG_FUNC_RETURN(card->ctx, SC_ERROR_INVALID_CARD);

	/* ensure that Card Serial Number is properly cached */
	return sc_card_ctl(card, SC_CARDCTL_GET_SERIALNR, &serial);
}

/**
 * Main entry point for DNIe CWA14890 SM data provider.
 *
 * Return a pointer to DNIe data provider with proper function pointers
 *
 * @param card pointer to card driver data structure
 * @return cwa14890 DNIe data provider if success, null on error
 */
cwa_provider_t *dnie_get_cwa_provider(sc_card_t * card)
{

	cwa_provider_t *res = cwa_get_default_provider(card);
	if (!res)
		return NULL;

	/* set up proper data */

	/* pre and post operations */
	res->cwa_create_pre_ops = dnie_create_pre_ops;

	/* Get ICC intermediate CA  path */
	res->cwa_get_icc_intermediate_ca_cert = dnie_get_icc_intermediate_ca_cert;
	/* Get ICC certificate path */
	res->cwa_get_icc_cert = dnie_get_icc_cert;

	/* Obtain RSA public key from RootCA */
	res->cwa_get_root_ca_pubkey = dnie_get_root_ca_pubkey;
	/* Obtain RSA IFD private key */
	res->cwa_get_ifd_privkey = dnie_get_ifd_privkey;

	/* Retrieve CVC intermediate CA certificate and length */
	res->cwa_get_cvc_ca_cert = dnie_get_cvc_ca_cert;
	/* Retrieve CVC IFD certificate and length */
	res->cwa_get_cvc_ifd_cert = dnie_get_cvc_ifd_cert;

	/* Get public key references for Root CA to validate intermediate CA cert */
	res->cwa_get_root_ca_pubkey_ref = dnie_get_root_ca_pubkey_ref;

	/* Get public key reference for IFD intermediate CA certificate */
	res->cwa_get_intermediate_ca_pubkey_ref = dnie_get_intermediate_ca_pubkey_ref;

	/* Get public key reference for IFD CVC certificate */
	res->cwa_get_ifd_pubkey_ref = dnie_get_ifd_pubkey_ref;

	/* Get ICC private key reference */
	res->cwa_get_icc_privkey_ref = dnie_get_icc_privkey_ref;

	/* Get IFD Serial Number */
	res->cwa_get_sn_ifd = dnie_get_sn_ifd;

	/* Get ICC Serial Number */
	res->cwa_get_sn_icc = dnie_get_sn_icc;

	return res;
}

/**
 * Changes the provider to use the common secure (DNIe 2.0)
 * channel.
 *
 * @param card the card to change the cwa provider for
 */
void dnie_change_cwa_provider_to_secure(sc_card_t * card)
{
	cwa_provider_t * res = GET_DNIE_PRIV_DATA(card)->cwa_provider;

	/* redefine different IFD data for secure channel */
	res->cwa_get_cvc_ifd_cert = dnie_get_cvc_ifd_cert;
	res->cwa_get_ifd_privkey = dnie_get_ifd_privkey;
	res->cwa_get_ifd_pubkey_ref = dnie_get_ifd_pubkey_ref;
	res->cwa_get_sn_ifd = dnie_get_sn_ifd;
}

/**
 * Changes the provider to use the new PIN (DNIe 3.0)
 * channel.
 *
 * @param card the card to change the cwa provider for
 */
void dnie_change_cwa_provider_to_pin(sc_card_t * card)
{
	cwa_provider_t * res = GET_DNIE_PRIV_DATA(card)->cwa_provider;

	/* redefine different IFD data for PIN channel */
	res->cwa_get_cvc_ifd_cert = dnie_get_cvc_ifd_cert_pin;
	res->cwa_get_ifd_privkey = dnie_get_ifd_privkey_pin;
	res->cwa_get_ifd_pubkey_ref = dnie_get_ifd_pubkey_ref_pin;
	res->cwa_get_sn_ifd = dnie_get_sn_ifd_pin;
}

void dnie_format_apdu(sc_card_t *card, sc_apdu_t *apdu,
			int cse, int ins, int p1, int p2, int le, int lc,
			unsigned char * resp, size_t resplen,
			const unsigned char * data, size_t datalen)
{
	sc_format_apdu(card, apdu, cse, ins, p1, p2);
	apdu->le = le;
	apdu->lc = lc;
	if (resp != NULL) {
		apdu->resp = resp;
		apdu->resplen = resplen;
	}
	if (data != NULL) {
		apdu->data = data;
		apdu->datalen = datalen;
	}
}

#endif				/* HAVE_OPENSSL */
/* _ end of cwa-dnie.c - */
