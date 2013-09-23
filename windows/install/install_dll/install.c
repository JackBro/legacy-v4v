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

#pragma warning(disable: 4201)
#include <stdio.h> 
#include <tchar.h>
#include <windows.h>  
#include <newdev.h>
#include <setupapi.h>

#define MAX_CLASS_NAME_LEN 32

BOOL FindExistingDevice(IN LPTSTR HardwareId)
{
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
    BOOL Found;
    ULONG Error;

    DeviceInfoSet = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT ); 
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
        MessageBox(NULL, "GetClassDevs failed", "Error", MB_OK);
    
    Found = FALSE;
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0; SetupDiEnumDeviceInfo(DeviceInfoSet, i, &DeviceInfoData); i++)
    {
        DWORD DataT;
        LPTSTR p,buffer = NULL;
        DWORD buffersize = 0;

        while (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                                                 &DeviceInfoData,
                                                 SPDRP_HARDWAREID,
                                                 &DataT,
                                                 (PBYTE)buffer,
                                                 buffersize,
                                                 &buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA)
            {
                break;
            }
            else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                if (buffer) 
                    LocalFree(buffer);
                buffer = LocalAlloc(LPTR,buffersize);
            }
            else
            {
                MessageBox(NULL, "GetDeviceRegistryProperty failed", "Error", MB_OK);
                goto cleanup;
            }            
        }
        
        if (GetLastError() == ERROR_INVALID_DATA) 
            continue;
        
        for (p = buffer; (*p&&(p<&buffer[buffersize])); p += lstrlen(p) + sizeof(TCHAR))
        {   
            if (!_tcsicmp(HardwareId,p))
            {
                Found = TRUE;
                break;
            }
        }
        
        if (buffer)
            LocalFree(buffer);
        if (Found)
            break;
    }
    
    Error = GetLastError();
    if (!(Error == NO_ERROR || Error == ERROR_NO_MORE_ITEMS))
        MessageBox(NULL, "EnumDeviceInfo failed", "Error", MB_OK);
    
cleanup:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    SetLastError(err);
    
    return Found;
}

int IsAmd64(void)
{
    BOOL res;
    BOOL (*f)(HANDLE handle, PBOOL res);
    HANDLE k32;

    k32 = GetModuleHandle("kernel32");
    if (k32 == NULL) {
        MessageBox(NULL, "weird, can't find kernel32", "Error", MB_OK);
        return 0;
    }
    f = (BOOL (*)(HANDLE, PBOOL))GetProcAddress(k32, "IsWow64Process");
    if (!f)
        return 0;
    if (!f(GetCurrentProcess(), &res)) {
        MessageBox(NULL, "IsWow64Process returned error.", "Error", MB_OK);
        return 0;
    } else {
        return res;
    }
}


