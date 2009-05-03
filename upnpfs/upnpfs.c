/**
 * Copyright (c) 2009 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the fuse-ext2
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "upnpfs.h"

struct options_s opts;
struct private_s priv;

static const char *HOME = "http://sourceforge.net/projects/upnpavd/";

#if __FreeBSD__ == 10
static char def_opts[] = "allow_other,local,";
#else
static char def_opts[] = "";
#endif

static const char *usage_msg =
"\n"
"%s %s %d - FUSE UPNPFS Driver\n"
"\n"
"Copyright (C) 2009 Alper Akcan <alper.akcan@gmail.com>\n"
"\n"
"Usage:    %s <path to share> <mount point> [-o option[,...]]\n"
"\n"
"Options:  ro, force, allow_others\n"
"          Please see details in the manual.\n"
"\n"
"Example:  upnpfs /mnt/upnpfs\n"
"\n"
"%s\n"
"\n";

static int strappend (char **dest, const char *append)
{
	char *p;
	size_t size;

	if (!dest) {
		return -1;
	}
	if (!append) {
		return 0;
	}

	size = strlen(append) + 1;
	if (*dest) {
		size += strlen(*dest);
	}

	p = realloc(*dest, size);
    	if (!p) {
    		debugfs("Memory realloction failed");
		return -1;
	}

	if (*dest) {
		strcat(p, append);
	} else {
		strcpy(p, append);
	}
	*dest = p;

	return 0;
}

static void usage (void)
{
	printf(usage_msg, PACKAGE, VERSION, fuse_version(), PACKAGE, HOME);
}

static int parse_options (int argc, char *argv[])
{
	int c;

	static const char *sopt = "-o:i:hv";
	static const struct option lopt[] = {
		{ "options",	 required_argument,	NULL, 'o' },
		{ "help",	 no_argument,		NULL, 'h' },
		{ "interface",	 no_argument,		NULL, 'i' },
		{ "verbose",	 no_argument,		NULL, 'v' },
		{ NULL,		 0,			NULL,  0  }
	};

#if 0
	printf("arguments;\n");
	for (c = 0; c < argc; c++) {
		printf("%d: %s\n", c, argv[c]);
	}
	printf("done\n");
#endif

	opterr = 0; /* We'll handle the errors, thank you. */

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
			case 1:	/* A non-option argument */
				if (!opts.mntpoint) {
					opts.mntpoint = optarg;
				} else {
					debugfs("You must specify exactly one mount point");
					return -1;
				}
				break;
			case 'o':
				if (opts.options)
					if (strappend(&opts.options, ","))
						return -1;
				if (strappend(&opts.options, optarg))
					return -1;
				break;
			case 'h':
				usage();
				exit(9);
			case 'v':
				/*
				 * We must handle the 'verbose' option even if
				 * we don't use it because mount(8) passes it.
				 */
				break;
			case 'i':
				opts.interface = optarg;
				break;
			default:
				debugfs("Unknown option '%s'", argv[optind - 1]);
				return -1;
		}
	}

	if (!opts.mntpoint) {
		debugfs("No mount point is specified");
		return -1;
	}
	if (!opts.interface) {
		debugfs("No interface is specified");
		return -1;
	}

	return 0;
}

static char * parse_mount_options (const char *orig_opts)
{
	char *options, *s, *opt, *val, *ret;

	ret = malloc(strlen(def_opts) + strlen(orig_opts) + 256 + PATH_MAX);
	if (!ret) {
		return NULL;
	}

	*ret = 0;
	options = strdup(orig_opts);
	if (!options) {
		debugfs("strdup failed");
		return NULL;
	}

	s = options;
	while (s && *s && (val = strsep(&s, ","))) {
		opt = strsep(&val, "=");
		if (!strcmp(opt, "debug")) { /* enable debug */
			if (val) {
				debugfs("'debug' option should not have value");
				goto err_exit;
			}
			opts.debug = 1;
			strcat(ret, "debug,");
		} else if (!strcmp(opt, "silent")) { /* keep silent */
			if (val) {
				debugfs("'silent' option should not have value");
				goto err_exit;
			}
			opts.silent = 1;
		} else { /* Probably FUSE option. */
			strcat(ret, opt);
			if (val) {
				strcat(ret, "=");
				strcat(ret, val);
			}
			strcat(ret, ",");
		}
	}

	strcat(ret, def_opts);
	strcat(ret, "ro,");
	strcat(ret, "fsname=");
	strcat(ret, "upnpfs");
#if __FreeBSD__ == 10
	strcat(ret, ",fstypename=");
	strcat(ret, "upnpfs");
	strcat(ret, ",volname=");
	strcat(ret, "upnpfs");
#endif
exit:
	free(options);
	return ret;
err_exit:
	free(ret);
	ret = NULL;
	goto exit;
}

static const struct fuse_operations upnpfs_ops = {
	.getattr        = op_getattr,
	.readlink       = NULL,
	.mknod          = NULL,
	.mkdir          = NULL,
	.unlink         = NULL,
	.rmdir          = NULL,
	.symlink        = NULL,
	.rename         = NULL,
	.link           = NULL,
	.chmod          = NULL,
	.chown          = NULL,
	.truncate       = NULL,
	.open           = op_open,
	.read           = op_read,
	.write          = NULL,
	.statfs         = NULL,
	.flush          = NULL,
	.release	= op_release,
	.fsync          = NULL,
	.setxattr       = NULL,
	.getxattr       = NULL,
	.listxattr      = NULL,
	.removexattr    = NULL,
	.opendir        = op_opendir,
	.readdir        = op_readdir,
	.releasedir     = NULL,
	.fsyncdir       = NULL,
	.init		= op_init,
	.destroy	= op_destroy,
	.access         = op_access,
	.create         = NULL,
	.ftruncate      = NULL,
	.fgetattr       = NULL,
	.lock           = NULL,
	.utimens        = NULL,
	.bmap           = NULL,
};

int main (int argc, char *argv[])
{
	int err = 0;
	char *parsed_options = NULL;
	struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);

	memset(&opts, 0, sizeof(opts));
	memset(&priv, 0, sizeof(priv));

	if (parse_options(argc, argv)) {
		usage();
		return -1;
	}

	parsed_options = parse_mount_options(opts.options ? opts.options : "");
	if (!parsed_options) {
		err = -2;
		goto err_out;
	}

	debugfs("opts.mntpoint: %s", opts.mntpoint);
	debugfs("opts.interface: %s", opts.interface);
	debugfs("opts.options: %s", opts.options);
	debugfs("parsed_options: %s", parsed_options);

	if (fuse_opt_add_arg(&fargs, PACKAGE) == -1 ||
	    fuse_opt_add_arg(&fargs, "-s") == -1 ||
	    fuse_opt_add_arg(&fargs, "-o") == -1 ||
	    fuse_opt_add_arg(&fargs, parsed_options) == -1 ||
	    fuse_opt_add_arg(&fargs, opts.mntpoint) == -1) {
		debugfs("Failed to set FUSE options");
		fuse_opt_free_args(&fargs);
		err = -5;
		goto err_out;
	}

	debugfs("mounting read-only");

	fuse_main(fargs.argc, fargs.argv, &upnpfs_ops, NULL);

err_out:
	fuse_opt_free_args(&fargs);
	free(parsed_options);
	free(opts.options);
	return err;
}
