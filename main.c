/*
 *  SPDX-FileCopyrightText: 2023 Ilya Pominov <ipominov@astralinux.ru>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <xcb/randr.h>

static xcb_window_t first_root(xcb_connection_t *connection)
{
    return xcb_setup_roots_iterator(xcb_get_setup(connection)).data->root;
}

static xcb_randr_monitor_info_t *first_monitor(xcb_connection_t *connection, xcb_window_t root)
{
    xcb_randr_monitor_info_t *ret = NULL;
    xcb_randr_get_monitors_cookie_t monitors_c = xcb_randr_get_monitors(connection, root, 0);
    xcb_randr_get_monitors_reply_t *monitors_r = xcb_randr_get_monitors_reply(connection, monitors_c, NULL);
    if (monitors_r) {
        xcb_randr_monitor_info_iterator_t monitor_iter = xcb_randr_get_monitors_monitors_iterator(monitors_r);
        while (monitor_iter.rem) {
            xcb_randr_monitor_info_t *monitor = monitor_iter.data;
            if (monitor->nOutput > 0) {
                int size = xcb_randr_monitor_info_sizeof(monitor);
                ret = malloc(size);
                memcpy(ret, monitor, size);
                break;
            }
            xcb_randr_monitor_info_next(&monitor_iter);
        }
    }
    free(monitors_r);
    return ret;
}

static char *monitor_name(xcb_connection_t *connection, xcb_randr_monitor_info_t *monitor)
{
    char *name = NULL;
    xcb_get_atom_name_cookie_t name_c = xcb_get_atom_name(connection, monitor->name);
    xcb_get_atom_name_reply_t *name_r = xcb_get_atom_name_reply(connection, name_c, NULL);
    if (name_r) {
        int size = xcb_get_atom_name_name_length(name_r);
        if (size > 0) {
            name = malloc(size + 1);
            memcpy(name, xcb_get_atom_name_name(name_r), size);
            name[size] = '\0';
        }
        free(name_r);
    }
    return name;
}

static void set_monitor_name(xcb_connection_t *connection, xcb_randr_monitor_info_t *monitor, const char *name)
{
    xcb_intern_atom_cookie_t name_c = xcb_intern_atom(connection, 0, strlen(name), name);
    xcb_intern_atom_reply_t *name_r = xcb_intern_atom_reply(connection, name_c, NULL);
    if (name_r) {
        monitor->name = name_r->atom;
        free(name_r);
    }
}

static void print_monitors(xcb_connection_t *connection)
{
    int screen_no = 0;
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(connection)); it.rem; xcb_screen_next(&it)) {
        ++screen_no;
        int monitor_no = 0;
        xcb_randr_get_monitors_cookie_t monitors_c = xcb_randr_get_monitors(connection, it.data->root, 0);
        xcb_randr_get_monitors_reply_t *monitors_r = xcb_randr_get_monitors_reply(connection, monitors_c, NULL);
        if (monitors_r) {
            xcb_randr_monitor_info_iterator_t monitor_iter = xcb_randr_get_monitors_monitors_iterator(monitors_r);
            while (monitor_iter.rem) {
                if (monitor_no == 0) {
                    printf("Screen %i monitors:\n", screen_no);
                }

                xcb_randr_monitor_info_t *monitor_info = monitor_iter.data;
                xcb_get_atom_name_cookie_t name_c = xcb_get_atom_name(connection, monitor_info->name);
                xcb_get_atom_name_reply_t *name_r = xcb_get_atom_name_reply(connection, name_c, NULL);

                printf("%i\t%.*s\t\t%u %u %u\t%u/%ux%u/%u+%i+%i\n",
                       ++monitor_no,
                       xcb_get_atom_name_name_length(name_r),
                       xcb_get_atom_name_name(name_r),
                       monitor_info->primary,
                       monitor_info->automatic,
                       monitor_info->nOutput,
                       monitor_info->width,
                       monitor_info->width_in_millimeters,
                       monitor_info->height,
                       monitor_info->height_in_millimeters,
                       monitor_info->x,
                       monitor_info->y);
                
                free(name_r);
                xcb_randr_monitor_info_next(&monitor_iter);
            }
            free(monitors_r);
        }
    }
}

static void set_monitor(xcb_connection_t *connection, xcb_randr_monitor_info_t *monitor, xcb_window_t root)
{
    char *name = monitor_name(connection, monitor);
    printf("Send set monitor %s\n", name);
    
    xcb_void_cookie_t set_monitor_c = xcb_randr_set_monitor_checked(connection, root, monitor);
    xcb_generic_error_t *error = xcb_request_check(connection, set_monitor_c);
    if (error) {
        printf("Error set monitor %s, code %u\n", name, error->error_code);
        free(error);
    } else {
        printf("Send set monitor %s done!\n", name);
    }
    
    free(name);
}

static void del_monitor(xcb_connection_t *connection, xcb_window_t root, const char *name, int silent)
{
    if (!silent) {
        printf("Send del monitor %s\n", name);
    }
    
    xcb_intern_atom_cookie_t name_c = xcb_intern_atom(connection, 0, strlen(name), name);
    xcb_intern_atom_reply_t *name_r = xcb_intern_atom_reply(connection, name_c, NULL);
    if (!name_r) {
        printf("Error del monitor, can\'t create atom for name %s\n", name);
        return;
    }
    xcb_atom_t atom = name_r->atom;
    free(name_r);
    
    xcb_void_cookie_t del_monitor_c = xcb_randr_delete_monitor_checked(connection, root, atom);
    xcb_generic_error_t *error = xcb_request_check(connection, del_monitor_c);
    if (error) {
        if (!silent) {
            printf("Error del monitor %s, code %u\n", name, error->error_code);
        }
        free(error);
        return;
    }
    
    printf("Send del monitor %s done!\n", name);
}

int main()
{
    xcb_connection_t *connection = xcb_connect(NULL, NULL);
    
    xcb_generic_error_t *error = NULL;
    xcb_randr_query_version_reply_t *version = xcb_randr_query_version_reply(connection,
                                                                             xcb_randr_query_version(connection, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION),
                                                                             &error);
    if (!version || error) {
        printf("Can't query RandR version\n");
        xcb_disconnect(connection);
        free(error);
        return 1;
    }
    
    if (version->major_version < 1 || version->minor_version < 5) {
        printf("RandR version (%u.%u) less then 1.5\n", version->major_version, version->minor_version);
        xcb_disconnect(connection);
        return 2;
    }
    
    printf("RandR version %u.%u\n", version->major_version, version->minor_version);
    free(version);
    
    print_monitors(connection);
    
    xcb_window_t root = first_root(connection);
    xcb_randr_monitor_info_t *monitor = first_monitor(connection, root);
    
    const char *new_name = "Monitor-1";
    del_monitor(connection, root, new_name, 1);
    set_monitor_name(connection, monitor, new_name);
    set_monitor(connection, monitor, root);
    print_monitors(connection);
    del_monitor(connection, root, new_name, 0);
    print_monitors(connection);
    
    free(monitor);
    xcb_disconnect(connection);
    
    return 0;
}
