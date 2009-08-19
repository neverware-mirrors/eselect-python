/* Copyright 1999-2009 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"

#define ENVD_CONFIG "/etc/env.d/python/config"

/* 127 is the standard return code for "command not found" */
#define EXIT_ERROR 127

const char program_description[] = "Gentoo Python wrapper program";

char* dir_cat(const char* dir, const char* file)
{
	size_t dir_len = strlen(dir);
	char* abs = malloc(dir_len + strlen(file) + 2);
	strcpy(abs, dir);
	abs[dir_len] = '/';
	abs[dir_len + 1] = 0;
	return strcat(abs, file);
}

const char* find_path(const char* exe)
{
	const char* last_slash = strrchr(exe, '/');
	if (last_slash)
	{
#ifdef HAVE_STRNDUP
		return strndup(exe, last_slash - exe);
#else
		size_t len = last_slash - exe;
		char* ret = malloc(sizeof(char) * (len + 1));
		memcpy(ret, exe, len);
		ret[len] = '\0';
		return(ret);
#endif
	}
	const char* PATH = getenv("PATH");
	if (! PATH)
	{
		/* If PATH is unset, then it defaults to ":/bin:/usr/bin", per
		 * execvp(3).
		 */
		PATH = ":/bin:/usr/bin";
	}
	char* path = strdup(PATH);
	char* state = NULL;
	const char* token = strtok_r(path, ":", &state);
	while (token)
	{
		/* If an element of PATH is empty ("::"), then it is "." */
		if (! *token)
		{
			token = ".";
		}
		struct stat sbuf;
		char* str = dir_cat(token, exe);
		if (stat(str, &sbuf) == 0 && (S_ISREG(sbuf.st_mode) || S_ISLNK(sbuf.st_mode)))
		{
			return token;
		}
		token = strtok_r(NULL, ":", &state);
	}
	return NULL;
}

/* True if a valid file name, and not "python" */
int valid_interpreter(const char* name)
{
	if (! name || ! *name || (strcmp(name, "python") == 0))
	{
		return 0;
	}
	return 1;
}

int get_version(const char* name)
{
	/* Only find files beginning with "python" - this is a fallback,
	 * so we only want CPython
	 */
	if (! valid_interpreter(name) || strncmp(name, "python", 6) != 0)
		return -1;
	int pos = 6;
	int major = 0;
	int minor = 0;
	if (name[pos] < '0' || name[pos] > '9')
		return -1;
	do
	{
		major = major * 10 + name[pos] - '0';
		if (! name[++pos])
			return -1;
	}
	while (name[pos] >= '0' && name[pos] <= '9');
	if (name[pos++] != '.')
		return -1;
	if (name[pos] < '0' || name[pos] > '9')
		return -1;
	do
	{
		minor = minor * 10 + name[pos] - '0';
		if (! name[++pos])
			return (major << 8) | minor;
	}
	while (name[pos] >= '0' && name[pos] <= '9');
	return -1;
}

int filter_python(const struct dirent* file)
{
	return get_version(file->d_name) != -1;
}

/* This implements a version sort, such that the following order applies:
 *    <invalid file names> (should never be seen)
 *    python2.6
 *    python2.9
 *    python2.10
 *    python3.0
 *    python3.1
 *    python3.2
 *    python9.1
 *    python9.9
 *    python9.10
 *    python10.1
 *    python10.9
 *    python10.10
 */
int sort_python(const struct dirent**f1, const struct dirent** f2)
{
	int ver1 = get_version((*f1)->d_name);
	int ver2 = get_version((*f2)->d_name);
	return ver1 - ver2;
}

const char* find_latest(const char* exe)
{
	const char* path = find_path(exe);
	if (! path || ! *path)
	{
		path = "/usr/bin";
	}
	struct dirent** namelist;
	int n = scandir(path, &namelist, filter_python, sort_python);
	const char* ret = NULL;
	if (n < 0)
	{
		return NULL;
	}
	/* walk backwards through the list */
	while (n--)
	{
		if (! ret)
			ret = strdup(namelist[n]->d_name);
		free(namelist[n]);
	}
	free(namelist);
	return ret;
}

int main(__attribute__((unused)) int argc, char** argv)
{
	if (strlen(program_description) == 0)
		abort();

	const char* EPYTHON = getenv("EPYTHON");
	if (! valid_interpreter(EPYTHON))
	{
		FILE* f = fopen(ENVD_CONFIG, "r");
		if (f)
		{
			struct stat st;
			fstat(fileno(f), &st);
			size_t size = st.st_size;
			char* cont = malloc(size + 1);
			cont = fgets(cont, size + 1, f);
			fclose(f);
			size_t len = strlen(cont);
			if (len && cont[len - 1] == '\n')
				cont[len - 1] = 0;
			EPYTHON = cont;
		}
	}

	if (! valid_interpreter(EPYTHON))
		EPYTHON = find_latest(argv[0]);

	if (! EPYTHON)
		return EXIT_ERROR;

	if (strchr(EPYTHON, '/'))
	{
		execv(EPYTHON, argv);
		return EXIT_ERROR;
	}

	const char* path = find_path(argv[0]);
	if (*path)
	{
		execv(dir_cat(path, EPYTHON), argv);
		return EXIT_ERROR;
	}

	execvp(EPYTHON, argv);
	return EXIT_ERROR;
}
