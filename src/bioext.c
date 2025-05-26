
/* 2025.05.22 by renwang */

#include "bioext.h"
#include "bstrext.h"
#include <sys/stat.h>

int bsm_file_exists(const char* fname)
{
	int is_dir;
	struct stat st;
	if (fname == NULL || *fname == 0)
		return 0;
	if (stat(fname, &st) != 0)
		return 0;
	is_dir = (st.st_mode & _S_IFMT) == _S_IFDIR;
	if (is_dir)
		errno = EISDIR;
	return !is_dir;
}

/* get the directory name of the specified file name */
char* bsm_get_dir(const char* src, char* dst_dir)
{
	ASSERT(dst_dir);
	*dst_dir = 0;
	if (src == NULL || *src == 0)
		return NULL;
	const char* p1 = strrchr(src, '/');
	const char* p2 = strrchr(src, '\\');
	const char* p = max(p1, p2);
	if (p == NULL) {
		dst_dir[0] = '.';
		dst_dir[1] = 0;
		return dst_dir;
	}
	return _safe_strcpy(dst_dir, p - src + 1, src);
}

const char* bsm_get_filename(const char* full_file)
{
	if (full_file == NULL || *full_file == 0)
		return NULL;
	const char* p1 = strrchr(full_file, '/');
	const char* p2 = strrchr(full_file, '\\');
	const char* p = max(p1, p2);
	if (p == NULL)
		return full_file;
	return p + 1;
}

char* bsm_fname_without_ext(const char* full_file, char* of_name)
{
	ASSERT(of_name);
	*of_name = '\0';
	if (full_file == NULL || *full_file == 0)
		return NULL;
	const char* fname = bsm_get_filename(full_file);
	if (fname == NULL)
		return NULL;
	const char *pos = strrchr(fname, '.');
	if (pos == NULL) {
		_safe_strcpy(of_name, MAX_PATH, fname);
		return of_name;
	}
	_safe_strcpy(of_name, pos - fname + 1, fname);
	of_name[pos - fname] = 0;
	return of_name;
}
