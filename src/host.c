#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <host.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

HostTemplate host_templates[] = {
    {"alpha", "alpha", "-mieee -pipe -O2 -mcpu=ev4", "alpha-unknown-linux-gnu", {"PORTDIR=\"/space/catalyst/portage\""}, PORTDIR},
    {"amd64", "amd64", "-O2 -pipe", "x86_64-pc-linux-gnu", {"CPU_FLAGS_X86=\"mmx sse sse2\""}, 0},
    {"armv4tl", "arm", ""},
    {"armv5tel", "arm"},
    {"armv6j", "arm"},
    {"armv6j_hardfp", "arm"},
    {"armv7a", "arm"},
    {"armv7a_hardfp", "arm", "-O2 -pipe -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard", "armv7a-hardfloat-linux-gnueabi", NULL, 0},
    {"arm64", "-O2 -pipe", "aarch64-unknown-linux-gnu", NULL, 0},
    {"hppa", "-O2 -pipe -march=1.1", "hppa1.1-unknown-linux-gnu", {"CXXFLAGS=\"-O2 -pipe\""}, CXXFLAGS},
};

host_id host_id_new () {
    int len = 15;
    host_id out = malloc (len + 1);
    
    srandom(time(NULL));  // Correct seeding function for random()
    char c;
    int i;
    for (i=0; i != len; i++) {
        c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"[random () % 62];
        out[i] = c;
    }
    out[len] = 0;
    
    return out;
}

Host* host_new (Server* server, host_id id) {
    Host* out = malloc (sizeof (Host));
    out->parent = server;
    out->id = id; // Dont need to dup, never accessed elsewhere
    out->status = NOT_INITED;
    out->chroot_status = CHR_NOT_MOUNTED;
    
    out->hostname = NULL;
    out->profile = NULL;
    out->makeconf.cflags = NULL;
    out->makeconf.cxxflags = NULL;
    out->makeconf.use = NULL;
    out->makeconf.chost = NULL;
    out->makeconf.extra = NULL;
    out->binhost.portage_tmpdir = NULL;
    out->binhost.portdir = NULL;
    out->binhost.distdir = NULL;
    out->binhost.pkgdir = NULL;
    out->binhost.port_logdir = NULL;
    
    return out;
}

void host_get_path (Host* host, char* dest) {
    char buf[PATH_MAX];
    char* dest_temp = realpath(host->parent->location, buf);
    if (dest_temp == NULL) {
        lerror ("Failed to get realpath()");
        return;
    }
    
    sprintf (dest, "%s/%s", dest_temp, host->id);
}

void host_free (Host* host) {
    free(host->id);
    free(host->profile);
    free(host->makeconf.cflags);
    free(host->makeconf.cxxflags);
    free(host->makeconf.use);
    free(host->makeconf.chost);
    string_vector_free (host->makeconf.extra);
    free(host->binhost.portage_tmpdir);
    free(host->binhost.portdir);
    free(host->binhost.distdir);
    free(host->binhost.pkgdir);
    free(host->binhost.port_logdir);
    free(host);
}

arch_t determine_arch (char* chost) {
    char* dest = strdup (chost);
    char* to_clear = strchr (dest, '-');
    if (to_clear == NULL) {
        return _INVALIDBIT;
    }
    *to_clear = 0;
    
    char* supported_32bit[] = {
        "i386",
        "i486",
        "i586",
        "i686"
    };
    
    char* supported_64bit[] = {
        "x86_64"
    };
    
    if (string_find(supported_32bit, dest, sizeof(supported_32bit) / sizeof(*supported_32bit)) != -1) {
        free (dest);
        return _32BIT;
    }
    
    if (string_find(supported_64bit, dest, sizeof(supported_64bit) / sizeof(*supported_64bit)) != -1) {
        free (dest);
        return _64BIT;
    }
    
    free (dest);
    return _INVALIDBIT;
}

int host_write_make_conf (Host* host, char* path) {
    char cbuild[64];
    FILE* cbuild_stream = popen ("gcc -dumpmachine", "r");
    if(!cbuild_stream){
        lerror("Could not determine CBUILD");
        return 1;
    }
    
    fgets(cbuild, 64, cbuild_stream);
    *strchr (cbuild, '\n') = '\0';
    pclose (cbuild_stream);
    
    char make_conf_file [256];
    sprintf (make_conf_file, "%s/etc/portage/make.conf", path);
    FILE * fp_mc;
    
    fp_mc = fopen (make_conf_file, "w+");
    if (fp_mc == NULL) {
        lerror ("Failed to open make.conf");
        return 1;
    }
    
    if (host->status == READY) {
        path = "/";
    }
    
    fprintf (fp_mc, "# This file has been generated by AutoGentoo\n\
# Do NOT change any settings in this file\n\
# Changes to this file could cause both\n\
# server and client system corruption!\n\
\n\
# System architecture configuration\n\
CFLAGS=\"%s\"\n\
CXXFLAGS=\"%s\"\n\
CHOST=\"%s\"\n\
CBUILD=\"%s\"\n\
\n\
# Portage system configuration\n\
SYS_ROOT=\"%s\"\n\
PORTAGE_TMPDIR=\"${SYS_ROOT}%s\"\n\
PORTDIR=\"%s\"\n\
DISTDIR=\"%s\"\n\
PKGDIR=\"${SYS_ROOT}%s\"\n\
PORT_LOGDIR=\"${SYS_ROOT}%s\"\n\
\n\
# Portage package configuration\n\
USE=\"%s\"\n\
EMERGE_DEFAULT_OPTS=\"--buildpkg --usepkg --autounmask-continue\"\n\
\n",    host->makeconf.cflags, host->makeconf.cxxflags, host->makeconf.chost, cbuild,
        path,
        host->binhost.portage_tmpdir,
        host->binhost.portdir,
        host->binhost.distdir,
        host->binhost.pkgdir,
        host->binhost.port_logdir,
        host->makeconf.use
    );
    
    int i;
    for (i = 0; i != host->makeconf.extra->n; i++) {
        fputs (string_vector_get (host->makeconf.extra, i), fp_mc);
        fputc('\n', fp_mc);
    }
    
    fclose(fp_mc);
    
    return 0;
}

/* Request calls */
void prv_mkdir(const char *dir) {
    char tmp[256];
    char* p = NULL;
    size_t len;

    snprintf (tmp, sizeof(tmp),"%s",dir);
    len = strlen (tmp);
    if(tmp[len-1] == '/')
            tmp[len-1] = 0;
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

response_t host_init (Host* host) {
    char host_path[256];
    host_path[0] = 0;
    host_get_path(host, host_path);
    
    if (host_path[0] == 0) {
        return INTERNAL_ERROR;
    }
    
    linfo ("Initializing host in %s", host_path);
    
    char *new_dirs [] = {
        host->binhost.portage_tmpdir,
        host->binhost.pkgdir,
        host->binhost.port_logdir,
        host->binhost.portdir,
        "etc/portage",
        "usr",
        "lib32",
        "lib64",
        "usr/lib32",
        "usr/lib64",
        "proc",
        "sys",
        "dev"
    };
    
    char target_dir[256];
    
    int i;
    for (i=0; i!=sizeof (new_dirs) / sizeof (new_dirs[0]); i++) {
        target_dir[0] = 0;
        sprintf (target_dir, "%s/%s", host_path, new_dirs[i]);
        prv_mkdir(target_dir);
    }
    
    linfo ("Writing make.conf");
    if (host_write_make_conf (host, host_path)) {
        lerror ("Failed writing make.conf!");
        return INTERNAL_ERROR;
    }
    
    arch_t current_arch = determine_arch (host->makeconf.chost);
    if (current_arch == _INVALIDBIT) {
        lerror ("invalid/unsupported chost: %s", host->makeconf.chost);
        return INTERNAL_ERROR;
    }
    
    linfo ("Linking directories/profile");
    pid_t link_fork = fork();
    if (link_fork == -1) {
        exit (-1);
    }
    if (link_fork == 0) {
        chdir (host_path);
        
        int ret = 0;
        
        // Create the profile symlink
        char sym_buf_p1 [128];
        sprintf (sym_buf_p1, "/usr/portage/profiles/%s/", host->profile);
        
        char* lib_dest = current_arch == _32BIT ? "lib32" : "lib64";
        
        char* links[][2] = {
            {sym_buf_p1, "etc/portage/make.profile"},
            {lib_dest, "lib"},
            {lib_dest, "usr/lib"}
        };
        
        // Check if symlinks exist and remove them
        struct stat __sym_buff;
        
        int i;
        for (i = 0; i != sizeof (links) / sizeof (*links); i++) {
            if (lstat (links[i][1], &__sym_buff) == 0) {
                unlink (links[i][1]);
            }
            
            linfo ("Linking %s to %s", links[i][0], links[i][1]);
            if (symlink (links[i][0], links[i][1]) != 0) {
                lwarning("Failed to symlink %s!", links[i][1]);
                lwarning ("%d", errno);
                ret = 1;
            }
        }
        
        exit(ret);
    }
    
    int link_return;
    waitpid (link_fork, &link_return, 0); // Wait until finished
    
    host->status = INIT;
    
    return link_return ? INTERNAL_ERROR : OK;
}
/*
response_t host_stage1_install (Host* host, char* arg) {
    String* cmd_full = string_new (128);
    string_append(cmd_full, "emerge --autounmask-continue --buildpkg --root=\'");
    string_append(cmd_full, host->parent->location);
    string_append_c(cmd_full, '/');
    string_append(cmd_full, host->id);
    string_append(cmd_full, "\' --config-root=\'");
    string_append(cmd_full, host->parent->location);
    string_append_c(cmd_full, '/');
    string_append(cmd_full, host->id);
    string_append(cmd_full, "\' ");
    string_append(cmd_full, arg);
    
    printf ("%s\n", cmd_full->ptr);
    
    char* args[64];
    int i;
    for (i = 0, args[i] = strtok(cmd_full->ptr, " "); args[i] != NULL; i++, args[i] = strtok (NULL, " "));
    
    pid_t install_pid = fork ();
    if (install_pid == 0) {
        linfo ("Starting emerge...");
        fflush (stdout);
        
        execv ("/usr/bin/emerge", args);
        exit(-1);
    }
    
    int install_ret;
    waitpid (install_pid, &install_ret, 0); // Wait until finished
    
    string_free (cmd_full);
    
    return install_ret == 0 ? OK : INTERNAL_ERROR;
}*/

response_t host_install (Host* host, char* arg) {
    String* cmd_full = string_new (128);
    
    char* new_line;
    if ((new_line = strchr (arg, '\n')) != NULL) {
        *new_line = 0;
    }
    
    string_append(cmd_full, "emerge --autounmask-continue --buildpkg ");
    string_append(cmd_full, arg);
    
    
    StringVector* args = string_vector_new ();
    char* temp = strdup ((char*)cmd_full->ptr);
    
    string_vector_split (args, temp, " ");
    
    if (string_vector_get(args, args->n - 1) != NULL) {
        if (args->n + 1 == args->s) {
            vector_allocate(args);
        }
        ((char**)args->ptr)[args->n] = NULL;
        args->n++;
    }
    
    pid_t install_pid = fork ();
    if (install_pid == 0) {
        char root[256];
        host_get_path(host, root);
        if (chdir (root) == -1) {
            lerror ("chdir() failed");
            exit (-1);
        }
        if (chroot (root) == -1) {
            lerror ("chroot() failed");
            exit (-1);
        }
        
        linfo (cmd_full->ptr);
        linfo ("Starting emerge...");
        fflush (stdout);
        
        execv ("/usr/bin/emerge", args->ptr);
        exit(-1);
    }
    
    int install_ret;
    waitpid (install_pid, &install_ret, 0); // Wait until finished
    
    string_vector_free (args);
    string_free (cmd_full);
    
    return install_ret == 0 ? OK : INTERNAL_ERROR;
    
}