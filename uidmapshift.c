#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void usage(char *me)
{
	printf("Usage: %s directory u nsuid hostuid range\n", me);
	printf("       %s directory g nsgid hostgid range\n", me);
	printf("       %s directory b nsid  hostid  range\n", me);
	printf("       Shifts all ids under directory\n");
	printf("       'b' means shift both uids and gids to the same mapping\n");
	exit(1);
}

int convert_uids = 0, convert_gids = 0;

int convert(char *base, int h, int ns, int r)
{
	int ret;
	struct stat mystat;
	uid_t u;
	gid_t g;
	int converted = 0;

	ret = lstat(base, &mystat);
	if (ret)
		return -1;
	u = mystat.st_uid;
	g = mystat.st_gid;

	if (convert_uids && u >= h && u < h+r) {
		ret = chown(base, (u-h) + ns, -1);
		if (ret) {
			fprintf(stderr, "failed to chown %s\n", base);
			/* well, let's keep going */
		}
		converted = 1;
	}
	if (convert_gids && g >= h && g < g+r) {
		ret = chown(base, -1, (g-h) + ns);
		if (ret) {
			fprintf(stderr, "failed to chgrp %s\n", base);
			/* well, let's keep going */
		}
		converted = 1;
	}

	if (converted) {
		fprintf(stderr, "resetting mode on %s\n", base);
		ret = chmod(base, mystat.st_mode);
		if (ret) {
			fprintf(stderr, "failed to reset st_mode on %s\n", base);
			/* well, let's keep going */
		}
	}
	return 0;
}

#define GUESS 256
int recursive_convert(char *base, int h, int ns, int r)
{
	char *pathname;
	struct dirent dirent, *direntp;
	DIR *dir;
	int ret;
	int len;

	dir = opendir(base);
	if (!dir) {
		fprintf(stderr, "failed to open %s\n", base);
		return -1;
	}

	len = strlen(base) + GUESS;
	pathname = malloc(len);
	if (!pathname)
		return -1;

	while (!readdir_r(dir, &dirent, &direntp)) {
		struct stat mystat;
		char *p;
		uid_t u;
		gid_t g;
		int converted;

		if (!direntp)
			break;

		if (!strcmp(direntp->d_name, ".") ||
				!strcmp(direntp->d_name, ".."))
			continue;

		if (strlen(base) + strlen(direntp->d_name)+2 > len) {
			len = strlen(base) + strlen(direntp->d_name) + 2;
			p = realloc(pathname, len);
			if (!p) {
				free(pathname);
				return -1;
			}
			pathname = p;
		}

		ret = snprintf(pathname, len, "%s/%s", base, direntp->d_name);
		if (ret < 0 || ret >= len) {
			free(pathname);
			return -1;
		}
		ret = lstat(pathname, &mystat);
		if (ret)
			continue;
		printf("path %s stat is: isdir %s islnk %s mode %d\n", pathname,
				S_ISDIR(mystat.st_mode) ? "true" : "false",
				S_ISLNK(mystat.st_mode) ? "true" : "false",
				mystat.st_mode);
		if (S_ISDIR(mystat.st_mode) && !S_ISLNK(mystat.st_mode))
			recursive_convert(pathname, h, ns, r);
		u = mystat.st_uid;
		g = mystat.st_gid;
		converted = 0;
		if (convert_uids && u >= h && u < h+r) {
			printf("chowning %s\n", pathname);
			ret = chown(pathname, (u-h) + ns, -1);
			if (ret) {
				fprintf(stderr, "failed to chown %s\n", pathname);
				/* well, let's keep going */
			}
			converted = 1;
		}
		if (convert_gids && g >= h && g < h+r) {
			printf("chgrping %s\n", pathname);
			ret = chown(pathname, -1, (g-h) + ns);
			if (ret) {
				fprintf(stderr, "failed to chgrp %s\n", pathname);
				/* well, let's keep going */
			}
			converted = 1;
		}
		if (converted) {
			fprintf(stderr, "resetting mode on %s\n", pathname);
			ret = chmod(pathname, mystat.st_mode);
			if (ret) {
				fprintf(stderr, "failed to reset st_mode on %s\n", pathname);
				/* well, let's keep going */
			}
		}
	}

	if (pathname)
		free(pathname);
	closedir(dir);
	return 0;
}

int main(int argc, char *argv[])
{
	char *base;
	int huid, nsuid, range;
	int ret;

	if (argc < 6 || strcmp(argv[1], "-h") == 0 ||
			strcmp(argv[1], "--help") == 0)
		usage(argv[0]);

	base = argv[1];
	switch (*argv[2]) {
	case 'u': convert_uids = 1; break;
	case 'g': convert_gids = 1; break;
	case 'b': convert_uids = convert_gids = 1; break;
	default: usage(argv[0]);
	}
	huid = atoi(argv[3]);
	nsuid = atoi(argv[4]);
	range = atoi(argv[5]);

	ret = recursive_convert(base, huid, nsuid, range);
	if (ret)
		return ret;
	return convert(base, huid, nsuid, range);
}
