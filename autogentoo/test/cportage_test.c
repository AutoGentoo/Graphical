//
// Created by tumbar on 12/3/20.
//

#include <stdio.h>
#include <cportage/atom.h>
#include <cportage/language.h>
#include <cportage/module.h>
#include <setjmp.h>
#include <cmocka.h>
#include <ebuild.h>
#include <dirent.h>

struct atom_verify_proto {
    const char* category;
    const char* name;
    const char* full_version;
    atom_version_t version_select;
};

struct use_verify_proto {
    const char* use_name;
    use_operator_t operator;
};

static Dependency* dependency_verify_atom(Dependency* current, U32 count, struct atom_verify_proto verify[])
{
    for (U32 i = 0; i < count; i++)
    {
        assert_non_null(current->atom);
        assert_string_equal(current->atom->category, verify[i].category);
        assert_string_equal(current->atom->name, verify[i].name);

        if (verify[i].full_version)
        {
            assert_non_null(current->atom->version);
            assert_string_equal(current->atom->version->full_version, verify[i].full_version);
        }
        else
        {
            assert_null(current->atom->version);
        }

        assert_int_equal(current->atom->range, verify[i].version_select);
        current = current->next;
    }

    return current;
}

static Dependency* dependency_verify_use(Dependency* current, U32 count, struct use_verify_proto verify[])
{
    for (U32 i = 0; i < count; i++)
    {
        assert_null(current->atom);
        assert_int_equal(current->use_operator, verify[i].operator);
        if (current->use_condition)
            assert_string_equal(lut_get_key(global_portage->global_flags, current->use_condition), verify[i].use_name);
        current = current->next;
    }

    return current;
}

#define CTEST(name) static void name(void** state)

CTEST(test_atom_full)
{
    Atom* atom = atom_parse("!!>=cat2/pkg3-2.2.34-r6:3/32::repository"
                            "[use_enable, -use_disable,"
                            "use_equal=, !use_opposite=,"
                            "!disable_if_off?, enable_if_on?]");
    assert_non_null(atom);
    assert_string_equal(atom->category, "cat2");
    assert_string_equal(atom->name, "pkg3");
    assert_string_equal(atom->version->full_version, "2.2.34-r6");
    assert_int_equal(atom->version->revision, 6);
    assert_string_equal(atom->repository, "repository");

    AtomFlag* current_flag = atom->useflags;

    assert_string_equal(current_flag->name, "use_enable");
    assert_int_equal(current_flag->option, ATOM_USE_ENABLE);
    current_flag = current_flag->next;

    assert_string_equal(current_flag->name, "use_disable");
    assert_int_equal(current_flag->option, ATOM_USE_DISABLE);
    current_flag = current_flag->next;

    assert_string_equal(current_flag->name, "use_equal");
    assert_int_equal(current_flag->option, ATOM_USE_EQUAL);
    current_flag = current_flag->next;

    assert_string_equal(current_flag->name, "use_opposite");
    assert_int_equal(current_flag->option, ATOM_USE_OPPOSITE);
    current_flag = current_flag->next;

    assert_string_equal(current_flag->name, "disable_if_off");
    assert_int_equal(current_flag->option, ATOM_USE_DISABLE_IF_OFF);
    current_flag = current_flag->next;

    assert_string_equal(current_flag->name, "enable_if_on");
    assert_int_equal(current_flag->option, ATOM_USE_ENABLE_IF_ON);
    current_flag = current_flag->next;

    assert_null(current_flag);
    assert_string_equal(atom->slot, "3");
    assert_string_equal(atom->sub_slot, "32");

    assert_int_equal(atom->range, ATOM_VERSION_GE);
    assert_int_equal(atom->blocks, ATOM_BLOCK_HARD);

    Py_DECREF(atom);
}

CTEST(test_depend)
{
    Dependency* deps = depend_parse("use1? ( >=cat2/pkg3-2.2.34 cat1/pkg2-2.2.34 )");
    assert_non_null(deps);

    assert_string_equal(lut_get_key(global_portage->global_flags, deps->use_condition), "use1");
    assert_string_equal(deps->children->atom->name, "pkg3");
    assert_string_equal(deps->children->next->atom->name, "pkg2");

    Py_DECREF(deps);
}

CTEST(test_ebuild_init)
{
    Ebuild* self = (Ebuild*) PyEbuild_new(&PyEbuildType, NULL, NULL);
    assert_non_null(self);
    assert_int_equal(ebuild_init(self, "data/test-repo", "sys-devel", "gcc-9.3.0-r1"), 0);
    assert_int_equal(ebuild_metadata_init(self), 0);

    assert_non_null(self->pdepend);
    assert_non_null(self->rdepend);
    assert_non_null(self->bdepend);
    assert_non_null(self->depend);

    struct atom_verify_proto verify_atom_1[] = {
            {"app-portage", "elt-patches", "20170815", ATOM_VERSION_GE},
            {"sys-devel", "bison", "1.875", ATOM_VERSION_GE},
            {"sys-devel", "flex", "2.5.4", ATOM_VERSION_GE},
    };
    struct atom_verify_proto verify_atom_2[] = {
            {"sys-devel", "gettext", NULL, ATOM_VERSION_ALL},
    };
    struct atom_verify_proto verify_atom_3[] = {
            {"dev-util", "dejagnu", "1.4.4", ATOM_VERSION_GE},
            {"sys-devel", "autogen", "5.5.4", ATOM_VERSION_GE},
    };

    struct use_verify_proto verify_use_1[] = {
            {"nls", USE_OP_ENABLE}
    };
    struct use_verify_proto verify_use_2[] = {
            {"test", USE_OP_ENABLE}
    };

    Dependency* current = self->bdepend;
    current = dependency_verify_atom(current, 3, verify_atom_1);
    assert_null(dependency_verify_atom(current->children, 1, verify_atom_2));
    current = dependency_verify_use(current, 1, verify_use_1);
    assert_null(dependency_verify_atom(current->children, 2, verify_atom_3));
    assert_null(dependency_verify_use(current, 1, verify_use_2));

    assert_int_equal(Py_REFCNT(self), 1);
    Py_DECREF(self);
}

CTEST(test_parse_all_metadata)
{
    /* Parse the entire repository */
    /* We can just free each ebuild immediately */

    struct dirent* dp;
    struct dirent* dp2;
    struct dirent* dp3;
    DIR* dfd;
    DIR* dfd2;
    DIR* dfd3;

    char* repository_path = "/var/db/repos/gentoo/";
    assert_non_null(dfd = opendir(repository_path));

    char path1[PATH_MAX];
    char path2[PATH_MAX];

    while ((dp = readdir(dfd)) != NULL)
    {
        struct stat stbuf;
        sprintf(path1, "%s/%s", repository_path, dp->d_name);
        assert_int_not_equal(stat(path1, &stbuf), -1);

        if (*dp->d_name == '.')
            continue;

        if (S_ISDIR(stbuf.st_mode))
        {
            /* Skip directories without '-' */
            if (!strchr(dp->d_name, '-'))
                continue;

            const char* category = dp->d_name;
            assert_non_null(dfd2 = opendir(path1));

            while ((dp2 = readdir(dfd2)) != NULL)
            {
                if (*dp2->d_name == '.')
                    continue;
                sprintf(path2, "%s/%s/%s", repository_path, category, dp2->d_name);
                assert_int_not_equal(stat(path2, &stbuf), -1);
                if (!S_ISDIR(stbuf.st_mode))
                    continue;

                assert_non_null(dfd3 = opendir(path2));

                while ((dp3 = readdir(dfd3)) != NULL)
                {
                    if (*dp3->d_name == '.')
                        continue;
                    char* name_and_value = dp3->d_name;
                    char* match = strstr(name_and_value, ".ebuild");
                    if (!match)
                        continue;
                    *match = 0;

                    Ebuild* self = (Ebuild*) PyEbuild_new(&PyEbuildType, NULL, NULL);
                    assert_int_equal(ebuild_init(self, repository_path, category, name_and_value), 0);
                    assert_int_equal(ebuild_metadata_init(self), 0);
                    Py_DECREF(self);
                }

                closedir(dfd3);
            }

            closedir(dfd2);
        }
    }

    closedir(dfd);
}

CTEST(test_parse_invalid)
{
    assert_null(atom_parse("package-name-not-atom"));
    //assert_null(cmdline_parse("33"));
    assert_null(required_use_parse(" ?? "));
    assert_null(depend_parse("use? "));
}

CTEST(test_package_key)
{
    PyObject* args = PyTuple_New(1);
    PyObject* pkg_key = PyUnicode_FromString("sys-devel/gcc");
    Py_INCREF(pkg_key);
    PyTuple_SetItem(args, 0, pkg_key);
    PyObject* pkg = PyObject_CallObject((PyObject*) &PyPackageType, args);

    assert_non_null(pkg);

    assert_int_equal(Py_REFCNT(pkg), 1);
    PyPortage_add_package(global_portage, &pkg, 1);
    assert_int_equal(Py_REFCNT(pkg), 2);

    PyObject* pkg_id = PyLong_FromUnsignedLongLong(((Package*)pkg)->package_id);
    PyObject* pkg_2 = PyPortage_get_package(global_portage, &pkg_id, 1);
    assert_int_equal(Py_REFCNT(pkg), 3);

    assert_ptr_equal(pkg, pkg_2);

    Py_DECREF(pkg_id);
    Py_DECREF(args);
    Py_DECREF(pkg_key);
    Py_DECREF(pkg_2);
    Py_DECREF(pkg);

    // Still held by the LUT
    assert_int_equal(Py_REFCNT(pkg), 1);
}

const static struct CMUnitTest cportage_tests[] = {
        cmocka_unit_test(test_depend),
        cmocka_unit_test(test_atom_full),
        cmocka_unit_test(test_ebuild_init),
        //cmocka_unit_test(test_parse_all_metadata),
        cmocka_unit_test(test_parse_invalid),
        cmocka_unit_test(test_package_key),
};

int main(void)
{
    Portage* p;
    PyObject* module;

    Py_Initialize();
    module = PyInit_autogentoo_cportage();
    assert_non_null(module);

    p = (Portage*) PyPortage_new(&PyPortageType, NULL, NULL);
    PyPortage_init(p, NULL, NULL);
    PyCportage_Init(NULL, (PyObject* const*) &p, 1);

    int ret = cmocka_run_group_tests(cportage_tests, NULL, NULL);

    Py_DECREF(p);
    Py_DECREF(module);

    assert_int_equal(Py_FinalizeEx(), 0);
    return ret;
}