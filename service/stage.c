#define _GNU_SOURCE

#include <stdio.h>
#include <stage.h>
#include <host.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <download.h>
#include <command.h>
#include <unistd.h>

/*
 * Arguments (Everything after chost is optional)
 * [id] [portage-arch] [cflags] [chost] [extras]
 */

HostTemplate host_templates[] = {
    /*
    {"alpha", "alpha", "-mieee -pipe -O2 -mcpu=ev4", "alpha-unknown-linux-gnu", {"/space/catalyst/portage", PORTDIR}, 1},
    {"amd64", "amd64", "-O2 -pipe", "x86_64-pc-linux-gnu", {"CPU_FLAGS_X86=\"mmx sse sse2\"", OTHER}, 1},
    */
    {"amd64-systemd", "amd64", "-O2 -pipe", "x86_64-pc-linux-gnu", {"CPU_FLAGS_X86=\"mmx sse sse2\"", OTHER}, 1},
    /*  
    {"armv4tl", "arm"},
    {"armv5tel", "arm"},
    {"armv6j", "arm"},
    {"armv6j_hardfp", "arm"},
    {"armv7a", "arm"},
    {"armv7a_hardfp", "arm", "-O2 -pipe -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard", "armv7a-hardfloat-linux-gnueabi"},
    {"arm64", "arm", "-O2 -pipe", "aarch64-unknown-linux-gnu"},
    {"hppa", "hppa", "-O2 -pipe -march=1.1", "hppa1.1-unknown-linux-gnu", "CXXFLAGS=\"-O2 -pipe\"", CXXFLAGS},
    */
};

HostTemplate* host_template_init(Server* parent, HostTemplate t) {
    HostTemplate* out = malloc(sizeof(HostTemplate));
    out->parent = parent;
    
    out->id = strdup(t.id);
    out->arch = strdup(t.arch);
    out->cflags = strdup(t.cflags);
    out->chost = strdup(t.chost);
    
    if ((out->extra_c = t.extra_c != 0)) {
        int i;
        for (i = 0; i != t.extra_c; i++) {
            out->extras[i].make_extra = strdup(t.extras[i].make_extra);
            out->extras[i].select = t.extras[i].select;
        }
    }
    
    out->new_id = host_id_new();
    sprintf (out->dest_dir, "%s/stage-%s", parent->location, out->new_id);
    
    return out;
}

void host_template_stage(HostTemplate* t) {
    char distfile_dir[256];
    sprintf (distfile_dir, "%s/distfiles", t->parent->location);
    
    if (!opendir(distfile_dir)) {
        mkdir(t->dest_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    
    char distfile_meta_url[256];
    sprintf (distfile_meta_url, "http://distfiles.gentoo.org/%s/autobuilds/latest-stage3-%s.txt", t->arch, t->id);
    
    if (download(distfile_meta_url, "temp_dest", NO_PROGRESS).code != 200) {
        lerror("Could not download metadata for stage3!");
        return;
    }
    
    FILE* fp_temp = fopen("temp_dest", "r");
    
    char* line;
    size_t len;
    char* stage3_dest = NULL;
    ssize_t read = 0;
    while ((read = getline(&line, &len, fp_temp)) != -1) {
        if (line[0] == '#')
            continue;
        // The first non-comment line will be our target stage3
        
        line[strlen(line) - 1] = '\0'; // Remove the newline
        char* s = strtok(line, " ");
        asprintf(&stage3_dest, "http://distfiles.gentoo.org/%s/autobuilds/%s", t->arch, s);
        break;
    }
    fclose(fp_temp);
    remove("temp_dest");
    
    char* fname;
    asprintf(&fname, "%s/distfiles/stage3-%s.tar.bz2", t->parent->location, t->id);
    
    if (access(fname, F_OK) == -1) {
        linfo("Downloading stage3 from %s", stage3_dest);
        download(stage3_dest, fname, SHOW_PROGRESS);
    }
    
    int ext_ret;
    command("tar", "extract to", NULL, &ext_ret, fname, t->dest_dir);
}

Host* host_template_handoff(HostTemplate* src) {
    Host* out = host_new (src->parent, src->id);
    
    {
        char* t_profile_l;
        char profile_dest[256];
        
        asprintf (&t_profile_l, "%s/etc/portage/make.profile", src->dest_dir);
        ssize_t profile_len = readlink(t_profile_l, profile_dest, sizeof(profile_dest) - 1);
        profile_dest[(int)profile_len] = 0; // Readlink does not null terminal
        free (t_profile_l);
        
        char* t_profile_split = strstr (profile_dest, "profiles/");
        free (out->profile);
        out->profile = strdup (t_profile_split + strlen("profiles/"));
    }
    
    strcpy (out->hostname, "default");
    
    {
        strcpy (out->cflags, src->cflags);
    }
    
    return out;
}