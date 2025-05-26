
/* 2025.05.22 by renwang */

#include "bstrext.h"

/* safe string operation, dst_size like sizeof(char[]) */
char* _safe_strcpy(char* dst, size_t dst_size, const char* src)
{
	char* p = dst;
	size_t safe_size;
	ASSERT(dst && dst_size != 0);
	if (src == NULL || *src == '\0' || dst_size == 0) {
		if (dst_size != 0)
			*dst = '\0';
		return dst;
	}
	if (dst_size == (size_t)-1) {
		ASSERT(FALSE);  /* if you specify dst_size = -1, use strcpy */
		while (*p++ == *src ++)
			;
		*p = '\0';
		return dst;
	}

	/* strncpy */
	safe_size = dst_size - 1;
	while (safe_size && (*p++ = *src ++)) {  /* copy string */
		-- safe_size;
	}

	/* pad out with zero */
	*p = '\0';
	return dst;
}
