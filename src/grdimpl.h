/*
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

#ifndef GRDIMPL__H__
#define GRDIMPL__H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

typedef int __attribute__((ms_abi))
    (*search_usb_device_callback)(const char* path, void* param);

/*
 * Communication to device (which grd_probe_device returned zero).
 */
int grd_ioctl_device(const char* dev_path, unsigned int prod_id,
                     size_t pack_size, void* in, size_t len_in, void* out, size_t len_out);

/*
 * Check device.
 * Return zero if device is Guardant Sign/Time or Guardant Code.
 */
int grd_probe_device(const char* dev_path, unsigned int* prod_id);

/*
 * Search for all USB devices. Call callback function for each USB device.
 * Return the count of non-zero values which were returned from callback.
 */
int search_usb_devices(search_usb_device_callback callback, void* param);

#endif /* !GRDIMPL__H__ */

