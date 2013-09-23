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

#include <windows.h>
#include <setupapi.h>
#include <stdlib.h>
#include <stdio.h>

MyStringToGuid(
    LPTSTR GuidString,
    LPGUID Guid
    )
{
    //
    // The code below looks odd but I believe there is a bug in sscanf where 
    // when you copy a %02x it copys a full ULONG, so copying it directly into
    // a unsigned short or unsigned char can lead to memory corruption.
    //
    ULONG  Value[10];
    sscanf(GuidString, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", 
           &(Guid->Data1), &Value[0], &Value[1], 
           &Value[2], &Value[3], &Value[4], &Value[5],
           &Value[6], &Value[7], &Value[8], &Value[9]);
    Guid->Data2 = (unsigned short)Value[0];
    Guid->Data3 = (unsigned short)Value[1];
    Guid->Data4[0] = (unsigned char)Value[2];
    Guid->Data4[1] = (unsigned char)Value[3];
    Guid->Data4[2] = (unsigned char)Value[4];
    Guid->Data4[3] = (unsigned char)Value[5];
    Guid->Data4[4] = (unsigned char)Value[6];
    Guid->Data4[5] = (unsigned char)Value[7];
    Guid->Data4[6] = (unsigned char)Value[8];
    Guid->Data4[7] = (unsigned char)Value[9];
}

int
RemoveDevice(
    LPTSTR HardwareId
    )
{
    int ret = 0;
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD index;
    CHAR buffer[4096];
    SP_REMOVEDEVICE_PARAMS removeParameters;

    DeviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);

    if (DeviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, "Cannot enumerate devices on system!\n", "Error", MB_OK);
        goto clean;
    }

    index = 0;
    DeviceInfoData.cbSize = sizeof(DeviceInfoData);
    while (SetupDiEnumDeviceInfo(DeviceInfoSet, index++, &DeviceInfoData)) {
        char *p;

        SetupDiGetDeviceRegistryProperty(DeviceInfoSet,
                                         &DeviceInfoData,
                                         SPDRP_HARDWAREID,
                                         NULL,
                                         buffer,
                                         sizeof(buffer),
                                         NULL);
        p = buffer;
        while (*p && _stricmp(p, HardwareId)) {
            p += strlen(p) + 1;
        }
        if (!*p)
            continue;

        removeParameters.ClassInstallHeader.cbSize =
            sizeof(removeParameters.ClassInstallHeader);
        removeParameters.ClassInstallHeader.InstallFunction =
            DIF_REMOVE;
        removeParameters.Scope = DI_REMOVEDEVICE_GLOBAL;
        removeParameters.HwProfile = 0;
        SetupDiSetClassInstallParams(DeviceInfoSet, &DeviceInfoData,
                                     &removeParameters.ClassInstallHeader,
                                     sizeof(removeParameters));
        if (!SetupDiCallClassInstaller(DIF_REMOVE, DeviceInfoSet, &DeviceInfoData)) {
            LPVOID lpMsgBuf;
            DWORD error = GetLastError();
            
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0,
                NULL);
            MessageBox(NULL, lpMsgBuf, "Failed to remove device", MB_OK);
            LocalFree(lpMsgBuf);
        }
    }

    ret = 0;

clean:
    if (DeviceInfoSet != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    }
    return ret;
}

int
RemoveFilterDriver(
    LPTSTR ClassGuidString,
    LPTSTR FilterType,
    LPTSTR Filter
    )
{
    int ret = 1;
    HKEY hClassKey = INVALID_HANDLE_VALUE;
    GUID ClassGuid;
    DWORD dwType;
    DWORD cbData;
    PTSTR FilterList = NULL;
    PTSTR NewFilterList = NULL;
    DWORD cbFilterList;
    PTSTR CurFilter, CurNewFilter;
    
    MyStringToGuid(ClassGuidString, &ClassGuid);

    hClassKey = SetupDiOpenClassRegKey(&ClassGuid, KEY_READ | KEY_WRITE);
    if (hClassKey == INVALID_HANDLE_VALUE) {
        goto clean;
    }

    //
    // Get the size of the filter list, allocate a couple buffers and then
    // get the filter list.
    //
    if (RegQueryValueEx(hClassKey,
                        FilterType,
                        NULL,
                        &dwType,
                        NULL,
                        &cbData) != ERROR_SUCCESS) {
        goto clean;
    }

    cbFilterList = cbData + (2 * sizeof(WCHAR));
    FilterList = (PTSTR)LocalAlloc(LPTR, cbFilterList);
    NewFilterList = (PTSTR)LocalAlloc(LPTR, cbFilterList);
    if ((FilterList == NULL) ||
        (NewFilterList == NULL)) {
        goto clean;
    }

    if (RegQueryValueEx(hClassKey,
                        FilterType,
                        NULL,
                        &dwType,
                        (LPBYTE)FilterList,
                        &cbData)) {
        goto clean;
    }
    
    //
    // Copy all of the current filters over to the new filter buffer excluding 
    // the specified filter that is to be removed.
    //
    CurNewFilter = NewFilterList;
    for (CurFilter=FilterList; *CurFilter; CurFilter+=lstrlen(CurFilter)+1) {
        if (lstrcmpi(CurFilter, Filter)) {
            //
            // This isn't the filter we want to remove so add it to the NewFilterList
            //
            strcpy(CurNewFilter, CurFilter);
            CurNewFilter+=lstrlen(CurFilter)+1;
        }
    }

    //
    // Determine the size of the new multi-sz filter list and write it back out to the 
    // registry.
    //
    cbData = 1;
    for (CurNewFilter=NewFilterList; *CurNewFilter; CurNewFilter+=lstrlen(CurNewFilter)+1) {
        cbData+=lstrlen(CurNewFilter)+1;
    }
    
    RegSetValueEx(hClassKey,
                  FilterType,
                  0,
                  REG_MULTI_SZ,
                  (BYTE*)NewFilterList,
                  cbData);

    ret = 0;

    /* Encourage Windows to write the updated filter list out as soon
       as possible, because if we crash with the filter .sys removed
       and the registry key still in place we're pretty much boned. */
    RegFlushKey(hClassKey);

clean:
    
    if (FilterList != NULL) {
        LocalFree(FilterList);
    }
    if (NewFilterList != NULL) {
        LocalFree(NewFilterList);
    }
    if (hClassKey != INVALID_HANDLE_VALUE) {
        RegCloseKey(hClassKey);
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////
//
// argv[1] must be one of the following flags:
//  '/d' -> argv[2] is the HardwareId of the device(s) to uninstall.
//
//  '/f' -> argv[2] is the Class GUID (in string form)
//          argv[3] is either 'LowerFilters' or 'UpperFilters'
//          argv[4] is the filter to remove
// 
//////////////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char *argv[])
{
    int ret = 0;

    UNREFERENCED_PARAMETER(argc);

    if (!lstrcmpi(argv[1], "/d")) {
        ret = RemoveDevice(argv[2]);
    } else if (!lstrcmpi(argv[1], "/f")) {
        ret = RemoveFilterDriver(argv[2], argv[3], argv[4]);
    } else {
        MessageBox(NULL, "Invalid device uninstall type passed in", "Error", MB_OK);
        ret = 1;
    }

    return ret;
}
