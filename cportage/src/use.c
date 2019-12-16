//
// Created by atuser on 5/5/19.
//

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include "use.h"
#include "package.h"
#include "directory.h"
#include "portage.h"
#include <stdlib.h>
#include "language/share.h"
#include <ctype.h>
#include "resolve.h"

use_t use_set(UseFlag* head, char* use_search, use_t new_val, use_priority_t priority) {
	UseFlag* target = use_get(head, use_search);
	if (!target)
		return -1;
	
	if (target->priority > priority && target->status != new_val) {
		portage_die("Explicit flag %s overriden by profile forced flag", use_search);
	}
	
	use_t out = target->status;
	target->status = new_val;
	target->priority = priority;
	
	return out;
}

UseFlag* use_get(UseFlag* head, char* useflag) {
	for (UseFlag* current = head; current; current = current->next)
		if (strcmp(current->name, useflag) == 0)
			return current;
	
	return NULL;
}

RequiredUse* use_build_required_use(char* target, use_t option) {
	RequiredUse* out = malloc(sizeof(RequiredUse));
	
	if (target)
		out->target = strdup(target);
	else
		out->target = NULL;
	out->option = option;
	out->depend = NULL;
	out->next = NULL;
	
	return out;
}

static char* use_select_prefix[] = {
		"", //None
		"!",
		"",
		"",
		"",
		""
};

static char* use_select_suffix[] = {
		"", //None
		"?",
		"?",
		"",
		"",
		""
};

char* use_expr_str(RequiredUse* expr) {
	char* out;
	if (expr->depend) {
		StringVector* ls = string_vector_new();
		size_t size = 0;
		for (RequiredUse* deps = expr->depend; deps; deps = deps->next) {
			char* to_free = use_expr_str(deps);
			string_vector_add(ls, to_free);
			size += strlen(to_free) + 1;
			free(to_free);
		}
		
		char* depend_str = malloc(size + 1);
		depend_str[0] = 0;
		
		for (int i = 0; i < ls->n; i++) {
			strcat(depend_str, string_vector_get(ls, i));
			strcat(depend_str, " ");
		}
		
		string_vector_free(ls);
		
		asprintf(&out, "%s%s%s ( %s )",
		         use_select_prefix[expr->option],
		         expr->target,
		         use_select_suffix[expr->option],
		         depend_str
		);
		free(depend_str);
	}
	else
		asprintf(&out, "%s%s", use_select_prefix[expr->option], expr->target);
	return out;
}

int use_check_expr(ResolvedEbuild* ebuild, RequiredUse* expr) {
	if (expr->option == USE_ENABLE || expr->option == USE_DISABLE) {
		UseFlag* u = use_get(ebuild->useflags, expr->target);
		use_t status = USE_NONE;
		if (u)
			status = u->status;
		
		if (expr->depend) {
			if (status == expr->option)
				return use_check_expr(ebuild, expr->depend);
			return 1;
		}
		else
			return expr->option == status;
	}
	
	int num_true = 0;
	for (RequiredUse* c = expr->depend; c; c = c->next) {
		if (use_check_expr(ebuild, c))
			num_true++;
	}
	
	if (expr->option == USE_LEAST_ONE)
		return num_true >= 1;
	if (expr->option == USE_MOST_ONE)
		return num_true <= 1;
	if (expr->option == USE_EXACT_ONE)
		return num_true == 1;
	
	return 0;
}

int ebuild_check_required_use(ResolvedEbuild* ebuild) {
	if (!ebuild->ebuild->required_use)
		return 1;
	for (RequiredUse* expr = ebuild->ebuild->required_use; expr; expr = expr->next) {
		if (use_check_expr(ebuild, expr) == 0) {
			char* expr_fail = use_expr_str(expr);
			plog_warn("Required use not met for ebuild %s", ebuild->ebuild->ebuild_key);
			plog_warn(expr_fail);
			free(expr_fail);
			return 0;
		}
	}
	
	return 1;
}

UseReason* use_reason_new(AtomFlag* flag, SelectedBy* selected_by) {
	UseReason* reason = malloc(sizeof(UseReason));
	reason->flag = flag;
	reason->selected_by = selected_by;
	reason->next = NULL;
	
	return reason;
}

UseFlag* use_new(char* name, use_t status, use_priority_t priority) {
	UseFlag* out = malloc(sizeof(UseFlag));
	
	if (*name == '+') {
		status = USE_ENABLE;
		name++;
	}
	else if (*name == '-') {
		status = USE_DISABLE;
		name++;
	}
	
	out->next = NULL;
	out->name = strdup(name);
	out->status = status;
	out->priority = priority;
	out->reason = NULL;
	
	return out;
}

UseFlag* use_iuse_parse(Emerge* em, char* metadata) {
	UseFlag* current = NULL;
	UseFlag* next = NULL;
	
	for (char* token = strtok(metadata, " "); token; token = strtok(NULL, " ")) {
		current = use_new(token, USE_DISABLE, PRIORITY_NORMAL);
		
		/* Set profile use (maybe?) */
		/* Set globals */
		use_t* ptr = map_get(em->profile->use, current->name);
		if (ptr)
			current->status = *ptr;
		
		current->next = next;
		next = current;
	}
	
	return current;
}

void use_free(UseFlag* ptr) {
	UseFlag* next = NULL;
	UseFlag* curr = ptr;
	while (curr) {
		next = curr->next;
		free(curr->name);
		free(curr);
		curr = next;
	}
}

void requireduse_free(RequiredUse* ptr) {
	if (!ptr)
		return;
	
	if (ptr->depend)
		requireduse_free(ptr->depend);
	
	if (ptr->next)
		requireduse_free(ptr->next);
	
	free(ptr->target);
	free(ptr);
}

char* strlwr(char* str) {
	unsigned char* p = (unsigned char*)str;
	while (*p) {
		*p = tolower(*p);
		p++;
	}
	
	return str;
}

void useflag_parse(FILE* fp, Vector* useflags, keyword_t keyword_required, use_priority_t priority) {
	char* line = NULL;
	size_t n = 0;
	size_t read_size = 0;
	
	while ((read_size = getline(&line, &n, fp)) != -1) {
		if (line[0] == '#' || line[0] == '\n' || line[0] == 0)
			continue;
		line[read_size - 1] = 0;
		char* atom_splt = strchr(line, ' ');
		*atom_splt = 0;
		char* atom = strdup(line);
		
		PackageUse* temp = malloc(sizeof(PackageUse));
		temp->atom = atom_parse(atom);
		temp->keyword_required = keyword_required;
		temp->flags = NULL;
		free(atom);
		
		for (char* curr_flag = strtok(atom_splt + 1, " "); curr_flag; curr_flag = strtok(NULL, " ")) {
			size_t len = strlen(curr_flag);
			if (curr_flag[len - 1] == ':') { // USE_EXPAND
				char* use_expand_flag = strtok(NULL, " ");
				if (!use_expand_flag)
					portage_die("Expected USE_EXPAND flag for %s (package.use)", curr_flag);
				
				UseFlag* new_flag = use_new(use_expand_flag, USE_ENABLE, priority);
				curr_flag[len - 1] = '_';
				curr_flag = strlwr(curr_flag);
				
				char* new_name = NULL;
				asprintf(&new_name, "%s%s", curr_flag, use_expand_flag);
				free(new_flag->name);
				new_flag->name = new_name;
				
				new_flag->next = temp->flags;
				temp->flags = new_flag;
				
				free(curr_flag);
				continue;
			}
			
			UseFlag* new_flag = use_new(curr_flag, USE_ENABLE, priority);
			new_flag->next = temp->flags;
			temp->flags = new_flag;
		}
		
		vector_add(useflags, temp);
	}
	
	free(line);
}

void emerge_parse_useflags(Emerge* emerge) {
	char path[256];
	sprintf(path, "%setc/portage/package.use", emerge->root);
	
	FPNode* files = open_directory(path);
	
	/* USE THE PROFILE VECTOR WHEN READY */
	
	for (FPNode* current = files; current; current = current->next) {
		if (current->type == FP_NODE_DIR)
			continue;
		
		FILE* fp = fopen(current->path, "r");
		if (!fp) {
			plog_error("Failed to open %s", current->path);
			fpnode_free(current);
			return;
		}
		
		useflag_parse(fp, emerge->profile->package_use, KEYWORD_UNSTABLE, PRIORITY_NORMAL);
		fclose(fp);
	}
	
	fpnode_free(files);
	emerge_apply_package_use(emerge);
}

void emerge_apply_package_use(Emerge* emerge) {
	for (int i = 0; i < emerge->profile->package_use->n; i++) {
		PackageUse* current = vector_get(emerge->profile->package_use, i);
		
		for (Repository* repo = emerge->repos; repo; repo = repo->next) {
			if (current->atom->repo_selected == ATOM_REPO_DEFINED
			    && strcmp(current->atom->repository, repo->name) != 0)
				continue;
			
			Package* target = map_get(repo->packages, current->atom->key);
			
			if (!target)
				continue;
			
			for (Ebuild* current_ebuild = target->ebuilds; current_ebuild; current_ebuild = current_ebuild->older) {
				package_metadata_init(current_ebuild);
				if (ebuild_match_atom(current_ebuild, current->atom)) {
					for (UseFlag* current_flag = current->flags; current_flag; current_flag = current_flag->next) {
						if (current_ebuild->keywords[emerge->target_arch] >= current->keyword_required)
							use_set(current_ebuild->use, current_flag->name, current_flag->status, current_flag->priority);
					}
				}
			}
		}
	}
}