/*
 * srv_handle.c
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

#include <srv_handle.h>
#include <chroot.h>
#include <_string.h>

struct srv_f_link srv_methods[] = {
    {CREATE, _CREATE},
    {ACTIVATE, _ACTIVATE},
    {INIT, _INIT},
    {GETCLIENT, _GETCLIENT},
    {STAGE1, _STAGE1},
    {SYNC, _SYNC},
    {EDIT, _EDIT},
    {GETCLIENTS, _GETCLIENTS},
    {GETACTIVE, _GETACTIVE},
    {GETSPEC, _GETSPEC},
    {SCREMOVE, _SCREMOVE},
    {MNTCHROOT, _MNTCHROOT}
};

void update_config (struct manager* m_man) {
    if(!m_man->debug) {
        FILE * _fd = fopen (m_man->_config, "w+");
        write_serve (fileno(_fd), m_man);
        fclose (_fd);
    }
}

response_t srv_handle (int sockfd, request_t type, struct manager* m_man, char** args, int n) {
    int i;
    char* ip = get_ip_from_fd (sockfd);
    response_t res = METHOD_NOT_ALLOWED;
    for (i = 0; i != sizeof (srv_methods) / sizeof (struct srv_f_link); i++) {
        if (type == srv_methods[i].type) {
            res = srv_methods[i].func (m_man, ip, sockfd, args, n);
        }
    }
    
    rsend (sockfd, res);
    return res;
}

response_t _CREATE (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    strcpy(m_man->clients[m_man->client_c].hostname, args[0]);
    strcpy(m_man->clients[m_man->client_c].profile, args[1]);
    strcpy(m_man->clients[m_man->client_c].CHOST, args[2]);
    strcpy(m_man->clients[m_man->client_c].CFLAGS, args[3]);
    strcpy(m_man->clients[m_man->client_c].CXXFLAGS, "${CFLAGS}");
    strcpy(m_man->clients[m_man->client_c].USE, args[4]);
    for (m_man->clients[m_man->client_c].extra_c=0; m_man->clients[m_man->client_c].extra_c != n - 5; m_man->clients[m_man->client_c].extra_c++) {
        strcpy (m_man->clients[m_man->client_c].EXTRA[m_man->clients[m_man->client_c].extra_c], args[m_man->clients[m_man->client_c].extra_c+5]);
    }
    strcpy(m_man->clients[m_man->client_c].PORTAGE_TMPDIR, "/autogentoo/tmp");
    strcpy(m_man->clients[m_man->client_c].PORTDIR, "/usr/portage");
    strcpy(m_man->clients[m_man->client_c].DISTDIR, "/usr/portage/distfiles");
    strcpy(m_man->clients[m_man->client_c].PKGDIR, "/autogentoo/pkg");
    strcpy(m_man->clients[m_man->client_c].PORT_LOGDIR, "/autogentoo/log");
    strcpy (m_man->clients[m_man->client_c].PORTAGE_DIR, "/usr/portage");
    strcpy (m_man->clients[m_man->client_c].resolv_conf, "/etc/resolv.conf");
    strcpy (m_man->clients[m_man->client_c].locale, "en_US.utf8");
    
    gen_id (m_man->clients[m_man->client_c].id, 14); // Leave extra space for buf and 1 for \0
    
    _ip_activate (m_man, ip, m_man->clients[m_man->client_c].id);
    m_man->client_c++;
    update_config (m_man);
    
    write (sockfd, m_man->clients[m_man->client_c - 1].id, strlen(m_man->clients[m_man->client_c - 1].id));
    write (sockfd, "\n", 1);
    
    return OK;
}

response_t _ACTIVATE (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_id (m_man, args[0]);
    if (sc_no == -1) {
        return BAD_REQUEST;
    }
    else {
        _ip_activate (m_man, ip, m_man->clients[sc_no].id);
    }
    
    update_config (m_man);
    
    return OK;
}

response_t _INIT (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_ip (m_man, ip);
    if (sc_no == -1) {
        return FORBIDDEN;
    }
    
    init_serve_client (m_man, sc_no);
    return OK;
}

response_t _GETCLIENT (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_id (m_man, args[0]);
    
    if (sc_no == -1) {
        return NOT_FOUND;
    }
    
    char c_buff[4196];
    char EXTRA [2048];
    
    sprintf (EXTRA, "");
    
    int i_c;
    for (i_c=0; i_c!=m_man->clients[sc_no].extra_c; i_c++) {
        strcat (EXTRA, m_man->clients[sc_no].EXTRA[i_c]);
        strcat (EXTRA, "\n");
    }
    sprintf (c_buff, "%d\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
             m_man->clients[sc_no].extra_c,
             m_man->clients[sc_no].CFLAGS,
             m_man->clients[sc_no].CXXFLAGS,
             m_man->clients[sc_no].CHOST,
             m_man->clients[sc_no].USE,
             m_man->clients[sc_no].hostname,
             m_man->clients[sc_no].profile,
             EXTRA);
    write (sockfd, c_buff, strlen (c_buff));
    return OK;
}

response_t _STAGE1 (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_ip (m_man, ip);
    if (sc_no == -1) {
        return FORBIDDEN;
    }
    char pkgs[8192];
    FILE * fp = fopen ("/usr/portage/profiles/default/linux/packages.build", "r");
    char line[255];
    strcat (pkgs, "-u "); // Dont emerge packages that are already binaries
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *pos;
        if ((pos=strchr(line, '\n')) != NULL)
            *pos = '\0';
        if (line[0] == '#' || line[0] == '\n' || strcmp (line, "") == 0) {
            continue;
        }
        strcat (pkgs, line);
        strcat (pkgs, " ");
    }
    fclose (fp);
    
    response_t res = __m_install (pkgs, m_man, sc_no, ip, 1);
    m_man->clients[sc_no].state = STAGE3;
    
    m_man->clients[sc_no].chroot = chroot_new (m_man, sc_no);
    chroot_mount (m_man->clients[sc_no].chroot);
    m_man->clients[sc_no].state = CHROOT;
    
    update_config (m_man);
    
    return res;
}

response_t _SYNC (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    return system("emerge --sync") == 0 ? OK : INTERNAL_ERROR;
}

response_t _EDIT (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_id (m_man, args[0]);
    if (sc_no == -1) {
        return NOT_FOUND;
    }
    
    strcpy(m_man->clients[sc_no].hostname, args[1]);
    strcpy(m_man->clients[sc_no].profile, args[2]);
    strcpy(m_man->clients[sc_no].CHOST, args[3]);
    strcpy(m_man->clients[sc_no].CFLAGS, args[4]);
    strcpy(m_man->clients[sc_no].CXXFLAGS, "${CFLAGS}");
    strcpy(m_man->clients[sc_no].USE, args[5]);
    for (m_man->clients[sc_no].extra_c=0; m_man->clients[sc_no].extra_c != n - 6;m_man->clients[sc_no].extra_c++) {
        strcpy (m_man->clients[sc_no].EXTRA[m_man->clients[sc_no].extra_c], args[m_man->clients[sc_no].extra_c+6]);
    }
    strcpy(m_man->clients[sc_no].PORTAGE_TMPDIR, "/autogentoo/tmp");
    strcpy(m_man->clients[sc_no].PORTDIR, "/usr/portage");
    strcpy(m_man->clients[sc_no].DISTDIR, "/usr/portage/distfiles");
    strcpy(m_man->clients[sc_no].PKGDIR, "/autogentoo/pkg");
    strcpy(m_man->clients[sc_no].PORT_LOGDIR, "/autogentoo/log");
    write_make_conf (*m_man, m_man->clients[sc_no]);
    
    update_config (m_man);
}

response_t _GETCLIENTS (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    char tmp[12]={0x0};
    sprintf(tmp,"%d", m_man->client_c);
    write(sockfd, tmp, strlen(tmp));
    write (sockfd, "\n", sizeof(char));
    int i;
    for (i=0; i!=m_man->client_c; i++) {
        write (sockfd, m_man->clients[i].id, 14);
        write (sockfd, "\n", sizeof(char));
    }
    
    return OK;
}

response_t _GETACTIVE (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_ip (m_man, ip);
    if (sc_no == -1) {
        write (sockfd, "invalid\n", 8);
        return UNAUTHORIZED;
    }
    
    write (sockfd, m_man->clients[sc_no].id, strlen(m_man->clients[sc_no].id));
    write (sockfd, "\n", 1);
    
    return OK;
}

response_t _GETSPEC (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    system ("lscpu > build.spec");
    FILE *lspcu_fp = fopen("build.spec", "r");
    char symbol;
    if(lspcu_fp != NULL)
    {
        while((symbol = getc(lspcu_fp)) != EOF)
        {
            write (sockfd, &symbol, sizeof (char));
        }
        fclose(lspcu_fp);
        remove ("build.spec");
    }
    
    return OK;
}
response_t _SCREMOVE (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_id (m_man, args[0]);
    if (sc_no == -1) {
        return NOT_FOUND;
    }
    remove_client (m_man, sc_no);
    
    update_config (m_man);
    return OK;
}
response_t _MNTCHROOT (struct manager* m_man, char* ip, int sockfd, char** args, int n) {
    int sc_no = get_client_from_ip (m_man, ip);
    if (sc_no == -1) {
        return FORBIDDEN;
    }
    
    if (m_man->clients[sc_no].chroot == NULL) {
        m_man->clients[sc_no].chroot = chroot_new (m_man, sc_no);
    }
    
    chroot_mount (m_man->clients[sc_no].chroot);
    m_man->clients[sc_no].state = CHROOT;
    
    update_config (m_man);
    return OK;
}