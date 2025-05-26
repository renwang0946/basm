
/* 2025.05.22 by renwang
 * BoAsm main entry */

#include "bctx.h"

/* main entry */
int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Please input the asm source file\n");
		return EXIT_FAILURE;
	}
	bctx ctx;
	bctx_init(&ctx);
	for (int i = 1; i < argc; ++i) {
		const char* argp = argv[i];
		ASSERT(argp);
		if (argp[0] != '-') {
			if (ctx.src_file == NULL) {
				ctx.src_file = _strdup(argp);
				if (ctx.src_file == NULL) {
					ASSERT(FALSE);
					bctx_drop(&ctx);
					return 100;
				}
				continue;
			}
			bctx_print_err(&ctx, "unrecognized option '%s'\n", argp);
			bctx_drop(&ctx);
			return EXIT_FAILURE;
		}
		switch (argp[1]) {
		case 'o':
		case 'O':
			if (ctx.dst_file) {
				bctx_print_err(&ctx, "destination file '%s' specified again", ctx.dst_file);
				bctx_drop(&ctx);
				return EXIT_FAILURE;
			}
			ctx.dst_file = _strdup(argp);
			break;
		default:
			ASSERT(FALSE);
			bctx_print_err(&ctx, "unrecognized option '-%c'\n", argp[0]);
			bctx_drop(&ctx);
			return EXIT_FAILURE;
		}
	}
	int ret = bctx_start(&ctx);
	bctx_drop(&ctx);
	return ret;
}
