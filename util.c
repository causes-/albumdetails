#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void eprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *emalloc(size_t size) {
	void *p;

	p = malloc(size);
	if (!p)
		eprintf("Out of memory\n");
	return p;
}

void *ecalloc(size_t nmemb, size_t size) {
	void *p;

	p = calloc(nmemb, size);
	if (!p)
		eprintf("Out of memory\n");
	return p;
}

void *erealloc(void *p, size_t size) {
	void *r;

	r = realloc(p, size);
	if (!r)
		eprintf("Out of memory\n");
	return r;
}

void *estrdup(void *p) {
	void *r;

	r = strdup(p);
	if (!r)
		eprintf("out of memory\n");
	return r;
}

size_t strlcpy(char *dest, const char *src, size_t size) {
	size_t len;

	len = strlen(src);

	if (size) {
		if (len >= size)
			size -= 1;
		else
			size = len;
		strncpy(dest, src, size);
		dest[size] = '\0';
	}

	return size;
}
