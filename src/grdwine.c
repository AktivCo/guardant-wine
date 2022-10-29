/*
 * GrdWine - Guardant usb dongle helper library for Wine
 *
 * Copyright (C) 2008 Aktiv Co. (Aleksey Samsonov)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */
#include <stdarg.h> /* for #include "winbase.h" */
#include "windef.h"     /* <wine/windows/windef.h> */
#include "winbase.h"    /* <wine/windows/winbase.h> */
#include "winnt.h"      /* <wine/windows/winnt.h> */
#include "wine/debug.h" /* <wine/debug.h> */
#include "grdimpl.h"

#define GRD_DRIVER_VERSION      0x0540

WINE_DEFAULT_DEBUG_CHANNEL(grdwine);

typedef BOOL (__attribute__((ms_abi)) * GrdWine_SearchUsbDevices_Callback)(LPCSTR lpDevName, LPVOID lpParam);

DWORD WINAPI GrdWine_GetVersion()
{
    TRACE("() Version 0x%x\n", GRD_DRIVER_VERSION);
    return GRD_DRIVER_VERSION;
}

DWORD WINAPI GrdWine_SearchUsbDevices(GrdWine_SearchUsbDevices_Callback Func, LPVOID lpParam)
{
    search_usb_device_callback func = (search_usb_device_callback)Func;
    void* param = (void*)lpParam;
    int ret;

    TRACE("(%p, %p)\n", (void*)Func, param);
    if (!func || !param)
        return FALSE;

    TRACE("Call search_usb_devices(%p, %p)\n", (void*)func, param);
    ret = search_usb_devices(func, param);
    TRACE("Ret search_usb_devices %d\n", ret);

    return ret > 0 ? (DWORD)ret : 0;
}

BOOL WINAPI GrdWine_DeviceProbe(LPCSTR lpDevName, LPDWORD pProdId)
{
    char* path = (char*)lpDevName;
    unsigned int* prod_id = (unsigned int*)pProdId;
    int ret;

    TRACE("(%s, %p)\n", lpDevName, pProdId);
    if (!path || !prod_id)
        return FALSE;

    TRACE("Call grd_probe_device(%s, %p)\n", path, prod_id);
    ret = grd_probe_device(path, prod_id);
    TRACE("Ret grd_probe_device %d\n", ret);

    return ret == 0 ? TRUE : FALSE;
}

BOOL WINAPI GrdWine_DeviceIoctl(LPCSTR lpDevName, DWORD ProdId, DWORD dwPackSize,
                                LPVOID lpIn, DWORD nInSize, LPVOID lpOut, DWORD nOutSize)
{
    char* path = (char*)lpDevName;
    void* in = lpIn, * out = lpOut;
    size_t in_size = (size_t)nInSize, out_size = (size_t)nOutSize;
    size_t pack_size = (size_t)dwPackSize;
    unsigned int prod_id = (unsigned int)ProdId;
    int ret;

    TRACE("(%s, %u, %u, %p, %u, %p, %u)\n", lpDevName, ProdId, dwPackSize, lpIn, nInSize, lpOut, nOutSize);
    if (!path || !in || !out)
        return FALSE;

    TRACE("Call grd_ioctl_device(%s, %u, %u, %p, %u, %p, %u)\n",
          path, prod_id, pack_size, in, in_size, out, out_size);
    ret = grd_ioctl_device(path, prod_id, pack_size, in, in_size, out, out_size);
    TRACE("Ret grd_ioctl_device %d\n", ret);

    return ret == 0 ? TRUE : FALSE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(%p, %d, %p)\n", (void*)hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // DisableThreadLibraryCalls(hinstDLL);
        break;
    }
    return TRUE;
}

