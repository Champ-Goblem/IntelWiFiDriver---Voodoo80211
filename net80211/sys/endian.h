/*	$OpenBSD: endian.h,v 1.18 2006/03/27 07:09:24 otto Exp $	*/

/*-
 * Copyright (c) 2011 Prashant Vaibhav.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Generic definitions for little- and big-endian systems.  Other endianesses
 * has to be dealt with in the specific machine/endian.h file for that port.
 *
 * This file is meant to be included from a little- or big-endian port's
 * machine/endian.h after setting _BYTE_ORDER to either 1234 for little endian
 * or 4321 for big..
 */

#ifndef _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_

#include <libkern/OSByteOrder.h>

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

__uint64_t	htobe64(__uint64_t);
__uint32_t	htobe32(__uint32_t);
__uint16_t	htobe16(__uint16_t);
__uint64_t	betoh64(__uint64_t);
__uint32_t	betoh32(__uint32_t);
__uint16_t	betoh16(__uint16_t);

__uint64_t	htole64(__uint64_t);
__uint32_t	htole32(__uint32_t);
__uint16_t	htole16(__uint16_t);
__uint64_t	letoh64(__uint64_t);
__uint32_t	letoh32(__uint32_t);
__uint16_t	letoh16(__uint16_t);

#define htobe16 OSSwapHostToBigInt16
#define htobe32 OSSwapHostToBigInt32
#define htobe64 OSSwapHostToBigInt64
#define betoh16 OSSwapBigToHostInt16
#define betoh32 OSSwapBigToHostInt32
#define betoh64 OSSwapBigToHostInt64

#define htole16 OSSwapHostToLittleInt16
#define htole32 OSSwapHostToLittleInt32
#define htole64 OSSwapHostToLittleInt64
#define letoh16 OSSwapLittleToHostInt16
#define letoh32 OSSwapLittleToHostInt32
#define letoh64 OSSwapLittleToHostInt64

#define swap16	OSSwapInt16

#endif /* _SYS_ENDIAN_H_ */
