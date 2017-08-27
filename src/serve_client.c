/*
 * serve_client.c
 * 
 * Copyright 2017 Unknown <atuser@Hyperion>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <serve_client.h>
#include <ip_convert.h>

int get_client_from_ip (struct manager * m_man, char* ip) {
    int i;
    for (i=0; i!=m_man->client_c; i++) {
        int j;
        for (j=0; j != m_man->clients[i].ip_c; j++) {
            if (strcmp (m_man->clients[i].ip[j], ip) == 0) {
                return i;
            }
        }
    }
    
    return -1;
}

int get_client_from_hostname  (struct manager * m_man, char * hostname) {
    int i;
    for (i=0; i != m_man->client_c; i++) {
        if (strcmp (m_man->clients[i].hostname, hostname) == 0) {
            return i;
        }
    }
    return -1;
}

/* Because a system is just a set of packages
 * All we need here is to setup a make.conf to emerge these packages
 */
void init_serve_client (struct manager m_man, struct serve_client conf) {
    char make_conf_buff [8192];
    char _ROOT_ [128];
    char EXTRA [2048];
    int i_c;
    for (i_c=0; i_c!=conf.extra_c; i_c++) {
        strcat (EXTRA, conf.EXTRA[i_c]);
        strcat (EXTRA, "\n");
    }
    sprintf (_ROOT_, "%s/%s/", m_man.root, conf.hostname);
    sprintf (make_conf_buff, "# This file has been generated by AutoGentoo\n\
# Do NOT change any settings in this file\n\
# Changes to this file could cause both\n\
# server and client system corruption!\n\
\n\
# System architecture configuration\n\
CFLAGS=\"%s\"\n\
CXXFLAGS=\"%s\"\n\
CHOST=\"%s\"\n\
\n\
# Portage system configuration\n\
SYS_ROOT=\"%s\"\n\
PORTAGE_TMPDIR=\"${SYS_ROOT}/%s\"\n\
PORTDIR=\"%s\"\n\
DISTDIR=\"%s\"\n\
PKGDIR=\"${SYS_ROOT}/%s\"\n\
PORT_LOGDIR=\"${SYS_ROOT}/%s\"\n\
\n\
# Portage package configuration\n\
USE=\"%s\"\n\
EMERGE_DEFAULT_OPTS=\"--buildpkg --usepkg --root=\'${SYS_ROOT}\' --config-root=\'${SYS_ROOT}/autogentoo/\' --autounmask-continue\"\n\
\n\
%s\
\n\0",  conf.CFLAGS, conf.CXXFLAGS, conf.CHOST,
    _ROOT_,
    conf.PORTAGE_TMPDIR,
    conf.PORTDIR,
    conf.DISTDIR,
    conf.PKGDIR,
    conf.PORT_LOGDIR,
    conf.USE,
    EXTRA
    );
    
    char *new_dirs [] = {
        conf.PORTAGE_TMPDIR,
        conf.PKGDIR,
        conf.PORT_LOGDIR,
        "autogentoo/etc/portage"
    };
    
    int i;
    for (i=0; i!=sizeof (new_dirs) / sizeof (*new_dirs); i++) {
        char TARGET_DIR[256];
        sprintf (TARGET_DIR, "%s/%s", _ROOT_, new_dirs[i]);
        _mkdir(TARGET_DIR);
    }
    
    char make_conf_file [256];
    sprintf (make_conf_file, "%s/autogentoo/etc/portage/make.conf", _ROOT_);
    FILE * fp_mc;
    
    fp_mc = fopen (make_conf_file, "w+");
    if (fp_mc != NULL)
    {
        fputs(make_conf_buff, fp_mc);
        fclose(fp_mc);
    }
    
    // Create the profile symlink
    char sym_buf_p1 [128];
    char sym_buf_p2 [128];
    sprintf (sym_buf_p1, "/usr/portage/profiles/%s/", conf.profile);
    sprintf (sym_buf_p2, "%s/autogentoo/etc/portage/make.profile", _ROOT_);
    if (symlink (sym_buf_p1, sym_buf_p2) != 0) {
        printf ("Failed to symlink profile!\n");
    }
    char sym_lib_p1[128];
    char sym_lib_p2[128];
    sprintf (sym_lib_p2, "%s/lib", _ROOT_);
    if ((size_t)-1 > 0xffffffffUL) {
        sprintf (sym_lib_p1, "%s/lib64", _ROOT_);
    }
    else {
        sprintf (sym_lib_p1, "%s/lib32", _ROOT_);
    }
    if (symlink (sym_lib_p1, sym_lib_p2) != 0) {
        printf ("Failed to symlink /lib!\n");
    }
}

void _mkdir(const char *dir) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, 0777);
                        *p = '/';
                }
        mkdir(tmp, 0777);
}

void write_serve (int fd, struct manager * m_man) {
    write (fd, m_man, sizeof (struct manager));
}

void read_serve (int fd, struct manager * m_man) { // You must malloc this pointer first
    read (fd, m_man, sizeof(struct manager));
}

struct link_srv link_methods [] = {
    L_CREATE,
    L_INIT,
    L_ADDIP,
    L_GETCLIENT
};

struct link_srv get_link_srv (serve_c c) {
    int i;
    for (i=0; i!=sizeof (link_methods) / sizeof (struct link_srv); i++) {
        if (c == link_methods[i].command)
            return link_methods[i];
    }
    return (struct link_srv) {c, 0}; // command out of range
};

void write_client (char *ip) {
    struct _client client;
    char *client_config = "/etc/portage/autogentoo.conf";
    int fd = open (client_config, O_RDWR);
    if (access (client_config, F_OK) == -1) {
        _mkdir ("/etc/portage");
        char message[256];
        char buffer[256];
        strcpy (buffer, "SRV 3 HTTP/1.0\n");
        serve_req (ip, buffer, &message[0]);
        printf ("%s\n", message);
        strtok (message, "\n");
        strcpy(client.CFLAGS, strtok (NULL, "\n"));
        strcpy(client.CXXFLAGS, strtok (NULL, "\n"));
        strcpy(client.CHOST, strtok (NULL, "\n"));
        strcpy(client.USE, strtok (NULL, "\n"));
        write (fd, &client, sizeof (struct _client));
    }
    else {
        read (fd, &client, sizeof (struct _client));
    }
    close (fd);
    fd = open ("/etc/portage/make.conf", O_RDWR);
    char make_conf_buff [1024];
    sprintf (make_conf_buff, "# This file has been generated by AutoGentoo\n\
# Do NOT change any settings in this file\n\
# Changes to this file could cause both\n\
# server and client system corruption!\n\
\n\
# System architecture configuration\n\
CFLAGS=\"%s\"\n\
CXXFLAGS=\"%s\"\n\
CHOST=\"%s\"\n\
\n\
# Portage package configuration\n\
USE=\"%s\"\n\
PORTAGE_BINHOST='http://%s:9490/\n\
\n\0", client.CFLAGS, client.CXXFLAGS, client.CHOST, client.USE, ip);
    write (fd, make_conf_buff, strlen (make_conf_buff));
    close (fd);
}
