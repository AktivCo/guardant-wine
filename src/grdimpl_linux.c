/*
 * Implementation of the GrdWine based
 * on Linux USB Device Filesystem and Linux USB HID Device Interface
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
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h> /* for PATH_MAX */
#include <stdio.h>  /* for snprintf */
#include <linux/usbdevice_fs.h>
#include <linux/hiddev.h>
#include "grdimpl.h"

#define GRD_VENDOR              0x0a89
#define GRD_PRODID_S3S          0x08 /* Guardant Sign/Time USB */
#define GRD_PRODID_S3S_HID      0x0C /* Guardant Sign/Time USB HID */
#define GRD_PRODID_S3S_WINUSB   0xC2 /* Guardant Sign/Time USB (WINUSB) */
#define GRD_PRODID_S3C          0x09 /* Guardant Code USB */
#define GRD_PRODID_S3C_HID      0x0D /* Guardant Code USB HID */
#define GRD_PRODID_S3C_WINUSB   0xC3 /* Guardant Code USB (WINSUB) */
#define USBFS_PATH_ENV          "USB_DEVFS_PATH"
#define GRD_IPC_NAME_ENV        "GRD_IPC_NAME"
#define USBFS_PATH_1            "/dev/bus/usb"
#define USBFS_PATH_2            "/proc/bus/usb"
#define GRDHID_PATH_HEAD        "/dev/grdhid"
#define GRDHID_MAX_COUNT        16

struct lock_descr
{
    int fd;
};

static int create_lock_path(const char* dev_path, char* buf, size_t buf_size)
{
    const char* name_prefix;
    const char* slash;
    int ret;
    size_t len, i, magic_num = 0;

    name_prefix = getenv(GRD_IPC_NAME_ENV);
    if (!name_prefix)
        name_prefix = "/tmp"; /* default */

    len = strlen(name_prefix);
    if (len == 0  ||  name_prefix[len - 1] != '/')
        slash = "/";
    else
        slash = "";

    assert(dev_path);
    for (i = 0; dev_path[i]; ++i)
    {
        magic_num += dev_path[i] * (i + 1);
        magic_num %= 0x61;
    }

    assert(buf);
    assert(name_prefix);
    ret = snprintf(buf, buf_size, "%s%sgrd%02d.lock", name_prefix, slash, magic_num);
    assert(ret > 0  &&  (size_t)ret < buf_size);
    if (ret > 0  &&  (size_t)ret < buf_size)
        return 0;
    return -1;
}

static int close_device(int fd, struct lock_descr* lock)
{
    int ret, ret_unlock;

    assert(fd >= 0);
    ret = close(fd); /* close device */

    assert(lock);
    assert(lock->fd >= 0);
    ret_unlock = close(lock->fd); /* process synchronization (unlock) */

    if (ret == 0)
        ret = ret_unlock;
    return ret;
}

static int open_device(const char* dev_path, struct lock_descr* lock_dscr)
{
    char lock_path[PATH_MAX];
    struct flock lock;
    mode_t mode;
    int fd, fd_dev, ret;
    long pid;

    assert(dev_path);
    if (create_lock_path(dev_path, lock_path, sizeof(lock_path)) != 0)
        return -1;

    mode = umask(0);
    fd = open(lock_path, O_RDWR | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    umask(mode);
    if (fd >= 0)
    {
        /* process synchronization (lock, wait) */
        lock.l_type = F_WRLCK;
        lock.l_start = 0;
        lock.l_whence = SEEK_SET;
        lock.l_len = 0;
        /*
         * FIXME: "while(...) { sleep() }" is compromise.
         * (for synchronization libgrdapi.a and grdwine.dll.so)
         */
        while ((ret = fcntl(fd, F_SETLKW, &lock)) == -1
               && (errno == EDEADLK || errno == EINTR || errno == ENOLCK)
               )
            sleep(1);
        if (ret != 0)
        {
            /* process synchronization failed */
            assert(fd >= 0);
            close(fd);
            fd = ret < 0 ? ret : -1;
        }
        else
        {
            pid = (long)getpid();
            ret = write(fd, (char*)&pid, sizeof(pid));
            assert(ret == sizeof(pid));
        }
    }
    if (fd >= 0)
    {
        assert(dev_path);
        fd_dev = open(dev_path, O_RDWR); /* open device */
        if (fd_dev >= 0)
        {
            assert(lock_dscr);
            lock_dscr->fd = fd;
        }
        else
        {
            ret = close(fd); /* process synchronization (unlock) */
            assert(ret == 0);
        }
        fd = fd_dev;
    }
    return fd;
}

static int ioctl_device_bulk(int fd, unsigned int ep, void* buf, size_t len)
{
    struct usbdevfs_bulktransfer packet;
    int ret;

    assert(fd >= 0);
    assert(buf);
    assert(len > 0);
    assert(len <= 16384 /* MAX_USBFS_BUFFER_SIZE */);
    packet.ep = ep;
    packet.len = (unsigned int)len;
    packet.timeout = 3000;
    packet.data = buf;
    ret = ioctl(fd, USBDEVFS_BULK, &packet);
    if (ret < 0 || (size_t)ret != len)
        return ret < 0 ? ret : -1;
    else
        return 0;
}

static int hiddevice_get_prodid(int fd, unsigned int* id)
{
    struct hiddev_devinfo devinfo;

    assert(fd >= 0);
    if (ioctl(fd, HIDIOCGDEVINFO, &devinfo) != 0)
        return -1;
    if (devinfo.vendor != GRD_VENDOR)
        return -1;
    if (devinfo.product != GRD_PRODID_S3S_HID && devinfo.product != GRD_PRODID_S3C_HID)
        return -1;
    assert(id);
    *id = devinfo.product;
    return 0;
}

static int hiddevice_write(int fd, void* buf, size_t len)
{
    const size_t report_len = 64;
    struct hiddev_usage_ref_multi ref;
    struct hiddev_report_info info;
    size_t i, n;

    if (len == 0 && buf == NULL)
        len = report_len; /* idle report */

    assert(len > 0);
    assert(len % report_len == 0);
    for (n = 0; n < len / report_len; ++n)
    {
        ref.uref.report_type = HID_REPORT_TYPE_OUTPUT;
        ref.uref.report_id = 0;
        ref.uref.field_index = 0;
        ref.uref.usage_index = 0;
        ref.uref.usage_code = 0xffa00004;
        ref.uref.value = 0;
        ref.num_values = report_len;
        assert(sizeof(ref.values) / sizeof(ref.values[0]) >= ref.num_values);
        for (i = 0; i < ref.num_values; ++i)
            if (!buf)
                ref.values[i] = 0;
            else
                ref.values[i] = ((unsigned char*)buf)[i + report_len * n];

        assert(fd >= 0);
        if (ioctl(fd, HIDIOCSUSAGES, &ref) != 0)
            return -1;

        info.report_type = HID_REPORT_TYPE_OUTPUT;
        info.report_id = 0;
        info.num_fields = 0;
        if (ioctl(fd, HIDIOCSREPORT, &info) != 0)
            return -1;
    }
    return 0;
}

static int hiddevice_read(int fd, void* buf, size_t len)
{
    const size_t report_len = 64;
    struct hiddev_usage_ref_multi ref;
    struct hiddev_report_info info;
    size_t i, n;
    fd_set rfds, efds;
    struct timeval tv;
    int ret;

    assert(len > 0);
    assert(len % report_len == 0);
    for (n = 0; n < len / report_len; ++n)
    {
        assert(fd >= 0);

        if (n > 0)
            /* write an idle report before reading the next report */
            if (hiddevice_write(fd, NULL, 0) != 0)
                return -1;

        FD_ZERO(&rfds);
        FD_ZERO(&efds);
        FD_SET(fd, &rfds);
        FD_SET(fd, &efds);
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        ret = select(fd + 1, &rfds, NULL, &efds, &tv);
        if (ret != 1 || !FD_ISSET(fd, &rfds) || FD_ISSET(fd, &efds))
            return -1;

        ret = read(fd, (char*)&ref.uref, sizeof(ref.uref));
        if (ret < 0 || (size_t)ret != sizeof(ref.uref))
            return -1;

        info.report_type = HID_REPORT_TYPE_INPUT;
        info.report_id = 0;
        info.num_fields = 0;
        if (ioctl(fd, HIDIOCGREPORT, &info) != 0)
            return -1;
        ref.uref.report_type = HID_REPORT_TYPE_INPUT;
        ref.uref.report_id = 0;
        ref.uref.field_index = 0;
        ref.uref.usage_index = 0;
        ref.uref.usage_code = 0xffa00003;
        ref.uref.value = 0;
        ref.num_values = report_len;
        assert(sizeof(ref.values) / sizeof(ref.values[0]) >= ref.num_values);
        if (ioctl(fd, HIDIOCGUSAGES, &ref) != 0)
            return -1;

        assert(buf);
        assert(ref.num_values == report_len);
        assert(sizeof(ref.values) / sizeof(ref.values[0]) >= ref.num_values);
        for (i = 0; i < ref.num_values; ++i)
            ((unsigned char*)buf)[i + report_len * n] = (unsigned char)ref.values[i];
    }
    return 0;
}

int grd_ioctl_device(const char* dev_path, unsigned int prod_id, size_t pack_size,
                     void* in, size_t len_in, void* out, size_t len_out)
{
    const int ishid = (prod_id == GRD_PRODID_S3S_HID || prod_id == GRD_PRODID_S3C_HID);
    struct lock_descr lock;
    int fd, r;
    int ret = -1, interface = 0, flags = HIDDEV_FLAG_UREF | HIDDEV_FLAG_REPORT;

    assert(dev_path);
    /* lock process and open device */
    fd = open_device(dev_path, &lock);
    if (fd < 0)
        return -1;

    assert(fd >= 0);
    if ((!ishid && ioctl(fd, USBDEVFS_CLAIMINTERFACE, &interface) == 0)
        ||  (ishid && ioctl(fd, HIDIOCSFLAG, &flags) == 0)
        )
    {
        assert(pack_size > 0);
        assert(len_out % pack_size == 0);
        assert(len_in % pack_size == 0);
        while (pack_size  &&  (len_out >= pack_size || len_in >= pack_size))
        {
            if (len_out >= pack_size)
            {
                /* write */
                assert(out);
                if (ishid)
                    r = hiddevice_write(fd, out, pack_size);
                else
                    r = ioctl_device_bulk(fd, 1, out, pack_size);
                if (r != 0)
                    break;
                len_out -= pack_size;
                out = (unsigned char*)out + pack_size;
            }
            else if (ishid)
            {
                /* write idle pack */
                if (hiddevice_write(fd, NULL, 0) != 0)
                    break;
            }
            /* read the latest pack after the last written pack */
            if ((len_in == pack_size && len_out < pack_size)
                || len_in > pack_size
                )
            {
                /* read */
                assert(in);
                if (ishid)
                    r = hiddevice_read(fd, in, pack_size);
                else
                    r = ioctl_device_bulk(fd, 0x81, in, pack_size);
                if (r != 0)
                    break;
                len_in -= pack_size;
                in = (unsigned char*)in + pack_size;
            }
        }
        if (len_out == 0 && len_in == 0)
            ret = 0;
        if (!ishid && ioctl(fd, USBDEVFS_RELEASEINTERFACE, &interface) != 0)
            ret = -1;
    }
    assert(fd >= 0);
    /* close device and unlock process */
    if (close_device(fd, &lock) != 0)
        ret = -1;
    return ret;
}

/*
 * If device (dev_path is usbfs path) is Guardant Sign/Time/Code,
 * or device (dev_path eq "GRDHID_PATH_HEAD + N") is
 * Guardant Sign/Time/Code HID  then return 0, else return -1
 */
int grd_probe_device(const char* dev_path, unsigned int* prod_id)
{
    unsigned char buf_tmpl[4] = {0x89, 0x0a, 0x00, 0x00};
    unsigned char buf[16];
    struct lock_descr lock;
    unsigned int id;
    int fd, ret;

    if (!dev_path || !prod_id)
        return -1;

    /* lock process and open usbdev_fs device */
    fd = open_device(dev_path, &lock);
    if (fd < 0)
        return -1;

    if (strncmp(dev_path, GRDHID_PATH_HEAD, sizeof(GRDHID_PATH_HEAD) - 1) == 0)
    {
        assert(fd >= 0);
        ret = hiddevice_get_prodid(fd, &id);
    }
    else
    {
        assert(fd >= 0);
        ret = read(fd, buf, sizeof(buf));
        if (ret < 0 || (size_t)ret != sizeof(buf))
            ret = -1;
        else
        {
            unsigned char p = 0;
            unsigned char prod_ids[4] = {GRD_PRODID_S3S, GRD_PRODID_S3S_WINUSB, GRD_PRODID_S3C, GRD_PRODID_S3C_WINUSB};
            ret = -1;
            assert(sizeof(buf_tmpl) == 4);
            for (; p < sizeof(prod_ids); ++p)
            {
                buf_tmpl[2] = prod_ids[p];
                if (!memcmp(buf + 8, buf_tmpl, sizeof(buf_tmpl)))
                {
                    id = prod_ids[p];
                    ret = 0;
                    break;
                }
            }
        }
    }
    assert(fd >= 0);
    /* close usbdev_fs device and unlock process */
    if (close_device(fd, &lock) != 0)
        ret = -1;
    if (ret == 0)
        *prod_id = id;
    return ret;
}

static int load_usbfs_path(char* buf, size_t size)
{
    const char* env;
    struct stat buf_stat;
    size_t path_len;
    const char* path = NULL;

    /* find path to usbfs (probe: getenv, some const path) */
    env = getenv(USBFS_PATH_ENV);
    if (env  &&  stat(env, &buf_stat) == 0)
    {
        path_len = strlen(env);
        path = env;
    }
    else if (stat(USBFS_PATH_1, &buf_stat) == 0)
    {
        path_len = sizeof(USBFS_PATH_1) - 1;
        path = USBFS_PATH_1;
    }
    else if (stat(USBFS_PATH_2, &buf_stat) == 0)
    {
        path_len = sizeof(USBFS_PATH_2) - 1;
        path = USBFS_PATH_2;
    }
    if (path)
    {
        assert(size > path_len);
        if (size > path_len)
        {
            assert(buf);
            memcpy(buf, path, path_len + 1);
            assert(strlen(buf) == path_len);
            return 0;
        }
    }
    return -1;
}

static size_t search_usbfs_devices(const char* usbfs_path,
                                   search_usb_device_callback callback, void* param)
{
    DIR* dir_bus;
    DIR* dir_dev;
    struct dirent* entry_bus;
    struct dirent* entry_dev;
    char dev_path[PATH_MAX];
    int ret;
    size_t count = 0;

    assert(usbfs_path);
    dir_bus = opendir(usbfs_path);
    while (dir_bus && (entry_bus = readdir(dir_bus)))
    {
        if (entry_bus->d_name[0] == '.')
            continue;

        ret = snprintf(dev_path, sizeof(dev_path), "%s/%s",
                       usbfs_path, entry_bus->d_name);
        assert(ret > 0  &&  (size_t)ret < sizeof(dev_path));
        if (ret < 0  ||  (size_t)ret >= sizeof(dev_path))
            continue;

        dir_dev = opendir(dev_path);
        while (dir_dev && (entry_dev = readdir(dir_dev)))
        {
            if (entry_dev->d_name[0] == '.')
                continue;

            ret = snprintf(dev_path, sizeof(dev_path), "%s/%s/%s",
                           usbfs_path, entry_bus->d_name, entry_dev->d_name);

            assert(ret > 0  &&  (size_t)ret < sizeof(dev_path));
            if (ret < 0  ||  (size_t)ret >= sizeof(dev_path))
                continue;

            assert(callback);
            if (callback(dev_path, param))
                ++count;
        }
        if (dir_dev)
            closedir(dir_dev);
    }
    if (dir_bus)
        closedir(dir_bus);
    return count;
}

static size_t search_grdhid_devices(search_usb_device_callback callback, void* param)
{
    char dev_path[PATH_MAX];
    struct stat buf;
    size_t i;
    int ret, count = 0;

    for (i = 0; i < GRDHID_MAX_COUNT; ++i)
    {
        ret = snprintf(dev_path, sizeof(dev_path), "%s%d",
                       GRDHID_PATH_HEAD, i);
        assert(ret > 0  &&  (size_t)ret < sizeof(dev_path));
        if (ret < 0  ||  (size_t)ret >= sizeof(dev_path))
            continue;

        if (stat(dev_path, &buf) != 0)
            continue;

        assert(callback);
        if (callback(dev_path, param))
            ++count;
    }
    return count;
}

int search_usb_devices(search_usb_device_callback callback, void* param)
{
    char usbfs_path[PATH_MAX];
    size_t count;

    if (!callback)
        return -1;
    if (load_usbfs_path(usbfs_path, sizeof(usbfs_path)) != 0)
        return -1;
    count = search_usbfs_devices(usbfs_path, callback, param);
    count += search_grdhid_devices(callback, param);
    return (int)count;
}

