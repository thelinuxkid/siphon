#include "../include/siphon/http.h"
#include "../include/siphon/error.h"
#include "../include/siphon/alloc.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include <ctype.h>

static char buffer[1024*1024];

static void
print_string (FILE *out, const void *val, size_t len)
{
	for (const uint8_t *p = val, *pe = p + len; p < pe; p++) {
		if (isprint (*p)) {
			fputc (*p, out);
		}
		else if (*p >= '\a' && *p <= '\r') {
			static const char tab[] = "abtnvfr";
			fprintf (out, "\\%c", tab[*p - '\a']);
		}
		else {
			fprintf (out, "\\x%02X", *p);
		}
	}
	fputc ('\n', out);
}

static ssize_t
read_body (SpHttp *p, char *buf, size_t len)
{
	if (!p->as.body_start.chunked) {
		if (len < p->as.body_start.content_length) {
			return SP_HTTP_ESYNTAX;
		}
		print_string (stdout, buf, p->as.body_start.content_length);
		return p->as.body_start.content_length;
	}

	char *cur = buf;
	char *end = buf + len;
	do {
		ssize_t rc = sp_http_next (p, cur, len);
		if (rc < 0) return rc;
		cur += rc;
		len -= rc;
		if (p->type == SP_HTTP_BODY_CHUNK) {
			if (len < p->as.body_chunk.length) {
				return SP_HTTP_ESYNTAX;
			}
			print_string (stdout, cur, p->as.body_chunk.length);
			cur += p->as.body_chunk.length;
			len -= p->as.body_chunk.length;
		}
		else {
			break;
		}
	} while (cur < end);
	return cur - buf;
}

static char *
readin (const char *path, size_t *outlen)
{
	FILE *in = stdin;
	if (path != NULL) {
		in = fopen (path, "r");
		if (in == NULL) {
			err (EXIT_FAILURE, "fopen");
		}
	}

	size_t len = fread (buffer, 1, sizeof buffer, in);
	if (len == 0) {
		err (EXIT_FAILURE, "fread");
	}

	fclose (in);

	char *copy = sp_malloc (len);
	if (copy == NULL) {
		err (EXIT_FAILURE, "sp_malloc");
	}

	memcpy (copy, buffer, len);
	*outlen = len;
	return copy;
}

int
main (int argc, char **argv)
{
	size_t len;
	char *buf = readin (argc > 1 ? argv[1] : NULL, &len);
	printf ("readin: %zu\n", len);
	char *cur = buf;
	char *end = buf + len;
	ssize_t rc = 0;
	size_t freelen = len;

	SpHttp p;
	sp_http_init_request (&p, true);

	while (!sp_http_is_done (&p) && cur < end) {
		rc = sp_http_next (&p, cur, len);
		if (rc < 0) break;

		if (rc > 0) {
			if (p.type == SP_HTTP_BODY_START) {
				sp_http_map_print (p.headers, stdout);
			}
			sp_http_print (&p, cur, stdout);
			cur += rc;
			len -= rc;
			if (p.type == SP_HTTP_BODY_START) {
				rc = read_body (&p, cur, len);
				if (rc < 0) break;
				cur += rc;
				len -= rc;
			}
		}
	}

	if (rc < 0) {
		fprintf (stderr, "http error: %s\n", sp_strerror (rc));
	}
	sp_http_final (&p);
	sp_free (buf, freelen);
	return sp_alloc_summary () && rc >= 0 ? 0 : 1;
}

