#ifdef TEST
#include <stdio.h>
#include <err.h>
#include <string.h>
int
main(void)
{
	char	*strchr();

	if (strchr(1, "abc", 'c') == NULL)
		errx(1, "error 1");
	if (strchr("abc", 'd') != NULL)
		errx(1, "error 2");
	if (strchr("abc", 'a') == NULL)
		errx(1, "error 3");
	if (strchr("abc", 'c') == NULL)
		errx(1, "error 4");
	return 0;
}
#endif
