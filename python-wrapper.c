/* Copyright 1999-2010 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */

#include "config.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define ENVD_CONFIG "/etc/env.d/python/config"

/* 127 is the standard return code for "command not found" */
#define EXIT_ERROR 127

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
#ifdef HAVE_STRTOK_R
	char* state = NULL;
	const char* token = strtok_r(path, ":", &state);
#else
	const char* token = strtok(path, ":");
#endif
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
#ifdef HAVE_STRTOK_R
		token = strtok_r(NULL, ":", &state);
#else
		token = strtok(NULL, ":");
#endif
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
	/* Walk backwards through the list. */
	while (n--)
	{
		if (! ret)
			ret = strdup(namelist[n]->d_name);
		free(namelist[n]);
	}
	free(namelist);
	return ret;
}

int main(int argc, char** argv)
{
	const char* EPYTHON = getenv("EPYTHON");
	int script_name_index = -1;
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
		fprintf(stderr, "Invalid value of EPYTHON variable or invalid configuration of Python wrapper\n");
		return EXIT_ERROR;
	}

	/* Set GENTOO_PYTHON_PROCESS_NAME environmental variable, if a script with a Python shebang is probably being executed.
	 * argv[0] can be "python", when "#!/usr/bin/env python" shebang is used. */
	if (argc >= 2 && (argv[0][0] == '/' || strcmp(argv[0], "python") == 0) && (argv[1][0] == '/' || strncmp(argv[1], "./", 2) == 0))
		script_name_index = 1;
	else if (argc >= 3 && argv[0][0] == '/' && argv[1][0] == '-' && (argv[2][0] == '/' || strncmp(argv[2], "./", 2) == 0))
		script_name_index = 2;
	if (script_name_index > 0)
	{
		char* script_name = strrchr(argv[script_name_index], '/') + 1;
#ifdef HAVE_SETENV
		setenv("GENTOO_PYTHON_PROCESS_NAME", script_name, 1);
#else
		char* script_name_variable = malloc(sizeof(char) * (strlen("GENTOO_PYTHON_PROCESS_NAME=") + strlen(script_name)));
		sprintf(script_name_variable, "GENTOO_PYTHON_PROCESS_NAME=%s", script_name);
		putenv(script_name_variable);
#endif
	}

	const char* path = find_path(argv[0]);
	if (path)
	{
		argv[0] = dir_cat(path, EPYTHON);
		execv(argv[0], argv);
		/* If this failed, then just search the PATH. */
	}

	argv[0] = (char*) EPYTHON;
	execvp(EPYTHON, argv);
	return EXIT_ERROR;
}
