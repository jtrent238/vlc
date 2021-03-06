/*****************************************************************************
 * access.c: HTTP/TLS VLC access plug-in
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_plugin.h>

#include "connmgr.h"
#include "file.h"

struct access_sys_t
{
    struct vlc_http_mgr *manager;
    struct vlc_http_file *file;
};

static block_t *Read(access_t *access)
{
    access_sys_t *sys = access->p_sys;

    block_t *b = vlc_http_file_read(sys->file);
    if (b == NULL)
        access->info.b_eof = true;
    return b;
}

static int Seek(access_t *access, uint64_t pos)
{
    access_sys_t *sys = access->p_sys;
    access->info.b_eof = false;

    if (vlc_http_file_seek(sys->file, pos))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case ACCESS_CAN_SEEK:
            *va_arg(args, bool *) = vlc_http_file_can_seek(sys->file);
            break;

        case ACCESS_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            break;

        case ACCESS_GET_SIZE:
        {
            uintmax_t val = vlc_http_file_get_size(sys->file);
            if (val >= UINT64_MAX)
                return VLC_EGENERIC;

            *va_arg(args, uint64_t *) = val;
            break;
        }

        case ACCESS_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = var_InheritInteger(access,
                                                          "network-caching");
            break;

        case ACCESS_GET_CONTENT_TYPE:
            *va_arg(args, char **) = vlc_http_file_get_type(sys->file);
            break;

        case ACCESS_SET_PAUSE_STATE:
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = malloc(sizeof (*sys));
    int ret = VLC_ENOMEM;

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->manager = NULL;
    sys->file = NULL;

    sys->manager = vlc_http_mgr_create(obj);
    if (sys->manager == NULL)
        goto error;

    char *ua = var_InheritString(obj, "http-user-agent");
    char *ref = var_InheritString(obj, "http-referrer");

    sys->file = vlc_http_file_create(sys->manager, access->psz_url, ua, ref);
    free(ref);
    free(ua);
    if (sys->file == NULL)
        goto error;

    char *redir = vlc_http_file_get_redirect(sys->file);
    if (redir != NULL)
    {
        access->psz_url = redir;
        ret = VLC_ACCESS_REDIRECT;
        goto error;
    }

    ret = VLC_EGENERIC;

    int status = vlc_http_file_get_status(sys->file);
    if (status < 0)
    {
        msg_Err(access, "HTTP connection failure");
        goto error;
    }
    if (status >= 300)
    {
        msg_Err(access, "HTTP %d error", status);
        goto error;
    }

    access->info.b_eof = false;
    access->pf_read = NULL;
    access->pf_block = Read;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;
    return VLC_SUCCESS;

error:
    if (sys->file != NULL)
        vlc_http_file_destroy(sys->file);
    if (sys->manager != NULL)
        vlc_http_mgr_destroy(sys->manager);
    free(sys);
    return ret;
}

static void Close(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_http_file_destroy(sys->file);
    vlc_http_mgr_destroy(sys->manager);
    free(sys);
}

vlc_module_begin()
    set_description(N_("HTTP/TLS input"))
    set_shortname(N_("HTTPS"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_capability("access", 0)
    add_shortcut("https")
    set_callbacks(Open, Close)
vlc_module_end()
