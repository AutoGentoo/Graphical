//
// Created by atuser on 4/28/19.
//

#ifndef AUTOGENTOO_MANIFEST_H
#define AUTOGENTOO_MANIFEST_H

#include <stdio.h>
#include "constants.h"

typedef enum {
	MANIFEST_IGNORE,
	MANIFEST_DATA,
	MANIFEST_MANIFEST
} manifest_t;

struct __ManifestHash {
	char* type;
	char* hash;
	ManifestHash* next;
};


struct __Manifest {
	char* full_path;
	char* filename;
	char* parent_dir;
	manifest_t type;
	size_t len;
	
	Manifest* parsed;
	Manifest* next;
};

struct __manifest_type_link_t {
	manifest_t type;
	char* type_str;
};

Manifest* manifest_metadata_parse_fp(FILE* fp, char* dir_path);
Manifest* manifest_metadata_parse(char* path);
void manifest_metadata_deep(Manifest* mans);
void manifest_free(Manifest* ptr);

#endif //AUTOGENTOO_MANIFEST_H
