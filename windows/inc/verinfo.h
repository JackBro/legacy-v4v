/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */ 

/* Version information common to all files. */

#undef VER_COMPANYNAME_STR
#undef VER_PRODUCTNAME_STR
#undef VER_LEGALCOPYRIGHT_STR
#undef VER_MAJOR_VERSION
#undef VER_MINOR_VERSION
#undef VER_MICRO_VERSION
#undef VER_BUILD_NR

/* The product build system will insert branding here ... */
/*@IMPORT_BRANDING@*/

/* ... otherwise we provide sane defaults when doing a local build. */
#ifndef BRANDING_IMPORTED
#define VER_COMPANYNAME_STR    "Citrix Systems, Inc."
#define VER_PRODUCTNAME_STR    "Citrix Tools for Virtual Machines"
#define VER_LEGALCOPYRIGHT_STR "Copyright (C) Citrix Systems, Inc., 2009"
#define VER_MAJOR_VERSION       6
#define VER_MINOR_VERSION       0
#define VER_MICRO_VERSION       0
#define VER_BUILD_NR            99999
#endif

#define BUILD_VERSION_STRING(_major, _minor, _micro, _build)    \
        #_major ## "." ##                                       \
        #_minor ## "." ##                                       \
        #_micro ## "." ##                                       \
        #_build

#define VER_VERSION_STRING                      \
        BUILD_VERSION_STRING(VER_MAJOR_VERSION, \
                             VER_MINOR_VERSION, \
                             VER_MICRO_VERSION, \
                             VER_BUILD_NR)

