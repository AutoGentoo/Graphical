//
// Created by atuser on 10/21/17.
//

#include <portage/repository.h>
#include <string.h>
#include <stdlib.h>
#include <tools/string.h>
#include <tools/log.h>
#include <portage/directory.h>

char* sync_types[] = {
        "cvs",
        "git",
        "rsync",
        "svn",
        "webrsync"
};

RepoConfig* repo_config_new () {
    RepoConfig* repo_conf = malloc(sizeof(RepoConfig));

    repo_conf->config = vector_new(sizeof(Conf*), REMOVE | UNORDERED);
    repo_conf->repositories = vector_new(sizeof(Repository*), REMOVE | UNORDERED);
    repo_conf->eclass_overrides = NULL;
    repo_conf->force = NULL;
    
    return repo_conf;
}

void repo_config_read (RepoConfig* repo_conf, char* filepath) {
    Conf* conf = conf_new(filepath);
    vector_add(repo_conf->config, &conf);
    if (repo_conf->eclass_overrides != NULL) {
        string_vector_free(repo_conf->eclass_overrides);
    }
    if (repo_conf->force != NULL) {
        string_vector_free(repo_conf->force);
    }
    repo_conf->eclass_overrides = conf_get_vector(conf, "DEFAULT", "eclass-overrides");
    repo_conf->force = conf_get_vector(conf, "DEFAULT", "force");

    int i;
    for (i = 0; i != conf->sections->n; i++) {
        ConfSection* current_section = *(ConfSection**)vector_get(conf->sections, i);
        if (strcmp(current_section->name, "DEFAULT") == 0) {
            continue;
        }
        Repository* temp = parse_repository(current_section);
        vector_add(repo_conf->repositories, &temp);
    }
}

Repository* parse_repository (ConfSection* section) {
    Repository* repo = malloc (sizeof(Repository));
    repo->categories = vector_new(sizeof(Category*), REMOVE | UNORDERED);
    strcpy(repo->name, section->name);
    printf ("repo: %s\n", repo->name);
    fflush(stdout);

    repo->eclass_overrides = conf_get_vector(section->parent, section->name, "eclass-overrides");
    repo->force = conf_get_vector(section->parent, section->name, "force");
    conf_get_convert(section->parent, (char*)repo->location, section->name, "location");
    conf_get_convert(section->parent, (char*)repo->sync_cvs_repo, section->name, "sync-cvs-repo");
    conf_get_convert(section->parent, (char*)repo->sync_uri, section->name, "sync-uri");
    char sync_type_buff[32];
    int has = conf_get_convert(section->parent, (char*)sync_type_buff, section->name, "sync-type");
    if (has) {
        if ((repo->sync_type = (repo_t)string_find(sync_types, sync_type_buff, 5)) == -1) {
            lerror("error in file %s", section->parent->path);
            lerror("sync type '%s' is invalid", sync_type_buff);
            exit(1);
        }
    }

    char auto_sync_buff[16];
    if (conf_get_convert(section->parent, (char*)auto_sync_buff, section->name, "auto-sync")) {
        if (strcmp ("yes", auto_sync_buff) == 0) {
            repo->auto_sync = true;
        }
        else if(strcmp ("no", auto_sync_buff) == 0) {
            repo->auto_sync = false;
        }
        else {
            lwarning("invalid token: '%s' (%s)", auto_sync_buff, section->parent->path);
        }
    }

    char priority_buff[8];
    if (conf_get_convert(section->parent, (char*)priority_buff, section->name, "priority")) {
        repo->priority = (int)strtol(priority_buff, NULL, 10);
    }
    
    
    char cat_dir[256];
    sprintf (cat_dir, "/%s", repo->location);
    fix_path (cat_dir);
    StringVector* dirs = get_directories (cat_dir);
    
    int i;
    for (i = 0; i != dirs->n; i++) {
        printf ("%d\n", i);
        Category* c = category_new(repo, string_vector_get(dirs, i));
        vector_add(repo->categories, &c);
    }
    
    string_vector_free(dirs);
    
    return repo;
}

void repo_config_free(RepoConfig* ptr) {
    int i;
    for (i=0; i != ptr->config->n; i++) {
        conf_free (*(Conf**)vector_get(ptr->config, i));
    }
    for (i=0; i != ptr->repositories->n; i++) {
        repository_free (*(Repository**)vector_get(ptr->repositories, i));
    }

    string_vector_free(ptr->force);
    string_vector_free(ptr->eclass_overrides);
    free (ptr);
}

void repository_free (Repository* ptr) {
    if (ptr->force != NULL) {
        string_vector_free(ptr->force);
    }
    if (ptr->eclass_overrides != NULL) {
        string_vector_free(ptr->eclass_overrides);
    }
    free(ptr);
}