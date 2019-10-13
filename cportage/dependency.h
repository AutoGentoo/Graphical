//
// Created by atuser on 5/19/19.
//

#ifndef AUTOGENTOO_DEPENDENCY_H
#define AUTOGENTOO_DEPENDENCY_H

#include <autogentoo/hacksaw/vector.h>
#include <autogentoo/hacksaw/hacksaw.h>
#include "constants.h"
#include "atom.h"

typedef enum {
	PORTAGE_NEW = 1 << 0, //!< New package
	PORTAGE_SLOT = 1 << 1, //!< Install side by side
	PORTAGE_REMOVE = 1 << 2, //!< Remove package 'old'
	PORTAGE_UPDATE = 1 << 3, //!< Install a new package and remove the old one
	PORTAGE_DOWNGRADE = 1 << 4,
	PORTAGE_REPLACE = 1 << 5,
	PORTAGE_USE_FLAG = 1 << 6,
	PORTAGE_BLOCK = 1 << 7
} dependency_t;

struct __SelectedEbuild {
	/* Next and depends not relevant */
	SelectedEbuild* parent_ebuild;
	Vector* use_change;
	
	Dependency* selected_by;
	InstalledEbuild* installed;
	Ebuild* ebuild;
	
	dependency_t action;
	
	UseFlag* useflags;
	UseFlag* explicit_flags;
};

/**
 * Resolve a list of potential ebuilds for depend
 * Generate the following structure:
 *
 *           |----- pkg1: [ebuilds] ---|
 * Dep tree  |----- pkg2: [ebuilds]    |----- Merge and find newest -> layer 2
 *           |----- pkg1: [ebuilds] ---|
 * @param parent the emerge environment
 * @param depend expression to look at
 */
Vector* pd_layer_resolve(Emerge* parent, Dependency* depend);

/**
 * Do the actually dependency tree with useflag checks etc...
 * @param parent build environment
 * @param depend selecting depend expression (linked by portage_depend_next)
 * @param target ebuild resolved in layer 1
 */
void pd_layer_2(Emerge* parent, Dependency* depend, Ebuild* target);

/* Helpers */
Package* atom_resolve_package(Emerge* emerge, P_Atom* atom);
int pd_slot_cmp(char* slot_1, char* sub_slot_1, char* slot_2, char* sub_slot_2);
SelectedEbuild* package_resolve_ebuild(Package* pkg, P_Atom* atom);
void selected_ebuild_free(SelectedEbuild* se);

SelectedEbuild* pd_check_selected(Vector* selected, SelectedEbuild* check);
void selected_ebuild_print(Emerge *em, SelectedEbuild *se);

#endif //AUTOGENTOO_DEPENDENCY_H
