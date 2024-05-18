/*
 * Make sure that scandir function works OK.
 *
 * Copyright (C) 2006-2012 Toni Ronkko
 * This file is part of dirent.  Dirent may be freely distributed
 * under the MIT license.  For all details and documentation, see
 * https://github.com/tronkko/dirent
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#   include <direct.h>
#else
#   include <unistd.h>
#endif
#include <sys/stat.h>
#include <dirent.h>

#undef NDEBUG
#include <assert.h>


/* Filter and sort functions */
static int only_readme (const struct dirent *entry);
static int no_directories (const struct dirent *entry);
static int reverse_alpha (const struct dirent **a, const struct dirent **b);


int
main(
    int argc, char *argv[])
{
    struct dirent **files;
    int i;
    int n;

    (void) argc;
    (void) argv;

    /* Basic scan with simple filter function */
    {
        /* Read directory entries */
        n = scandir ("tests/3", &files, only_readme, alphasort);
        assert (n == 1);

        /* Make sure that the filter works */
        assert (strcmp (files[0]->d_name, "README.txt") == 0);

        /* Release file names */
        for (i = 0; i < n; i++) {
            free (files[i]);
        }
        free (files);
    }

    /* Basic scan with default sorting function */
    {
        /* Read directory entries in alphabetic order */
        n = scandir ("tests/3", &files, NULL, alphasort);
        assert (n == 13);

        /* Make sure that we got all the file names in the proper order */
        assert (strcmp (files[0]->d_name, ".") == 0);
        assert (strcmp (files[1]->d_name, "..") == 0);
        assert (strcmp (files[2]->d_name, "3zero.dat") == 0);
        assert (strcmp (files[3]->d_name, "666.dat") == 0);
        assert (strcmp (files[4]->d_name, "Qwerty-my-aunt.dat") == 0);
        assert (strcmp (files[5]->d_name, "README.txt") == 0);
        assert (strcmp (files[6]->d_name, "aaa.dat") == 0);
        assert (strcmp (files[7]->d_name, "dirent.dat") == 0);
        assert (strcmp (files[8]->d_name, "empty.dat") == 0);
        assert (strcmp (files[9]->d_name, "sane-1.12.0.dat") == 0);
        assert (strcmp (files[10]->d_name, "sane-1.2.3.dat") == 0);
        assert (strcmp (files[11]->d_name, "sane-1.2.4.dat") == 0);
        assert (strcmp (files[12]->d_name, "zebra.dat") == 0);

        /* Release file names */
        for (i = 0; i < n; i++) {
            free (files[i]);
        }
        free (files);
    }

    /* Custom filter AND sort function */
    {
        /* Read directory entries in alphabetic order */
        n = scandir ("tests/3", &files, no_directories, reverse_alpha);
        assert (n == 11);

        /* Make sure that we got all the FILE names in the REVERSE order */
        assert (strcmp (files[0]->d_name, "zebra.dat") == 0);
        assert (strcmp (files[1]->d_name, "sane-1.2.4.dat") == 0);
        assert (strcmp (files[2]->d_name, "sane-1.2.3.dat") == 0);
        assert (strcmp (files[3]->d_name, "sane-1.12.0.dat") == 0);
        assert (strcmp (files[4]->d_name, "empty.dat") == 0);
        assert (strcmp (files[5]->d_name, "dirent.dat") == 0);
        assert (strcmp (files[6]->d_name, "aaa.dat") == 0);
        assert (strcmp (files[7]->d_name, "README.txt") == 0);
        assert (strcmp (files[8]->d_name, "Qwerty-my-aunt.dat") == 0);
        assert (strcmp (files[9]->d_name, "666.dat") == 0);
        assert (strcmp (files[10]->d_name, "3zero.dat") == 0);

        /* Release file names */
        for (i = 0; i < n; i++) {
            free (files[i]);
        }
        free (files);
    }

    printf ("OK\n");
    return EXIT_SUCCESS;
}

/* Only pass README.txt file */
static int
only_readme (const struct dirent *entry)
{
    int pass;

    if (strcmp (entry->d_name, "README.txt") == 0) {
        pass = 1;
    } else {
        pass = 0;
    }

    return pass;
}

/* Filter out directories */
static int
no_directories (const struct dirent *entry)
{
    int pass;

    if (entry->d_type != DT_DIR) {
        pass = 1;
    } else {
        pass = 0;
    }

    return pass;
}

/* Sort in reverse direction */
static int
reverse_alpha(
    const struct dirent **a, const struct dirent **b)
{
    return strcoll ((*b)->d_name, (*a)->d_name);
}


