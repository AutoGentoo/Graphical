//
// Created by atuser on 1/5/18.
//

#ifndef AUTOGENTOO_ABS_PACKAGE_H
#define AUTOGENTOO_ABS_PACKAGE_H

typedef struct __aabs_package_t aabs_package_t;

struct __aabs_package_t {
    unsigned long name_hash;
    char *filename;
    char *base;
    char *name;
    char *version;
    char *desc;
    char *url;
    char *packager;
    char *md5sum;
    char *sha256sum;
    char *base64_sig;
    char *arch;
};

#endif //AUTOGENTOO_ABS_PACKAGE_H
