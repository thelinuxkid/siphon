#include "../include/siphon/http.h"
#include "../include/siphon/alloc.h"
#include "../include/siphon/error.h"
#include "../include/siphon/fmt.h"
#include "mu.h"

#include <stdlib.h>
#include <ctype.h>

typedef struct {
	union {
		struct {
			char method[8];
			char uri[32];
			uint8_t version;
		} request;
		struct {
			uint8_t version;
			uint16_t status;
			char reason[32];
		} response;
	} as;
	struct {
		char name[32];
		char value[64];
	} fields[16];
	size_t field_count;
	char body[256];
} Message;

static bool
parse (SpHttp *p, Message *msg, const uint8_t *in, size_t inlen, ssize_t speed)
{
	memset (msg, 0, sizeof *msg);

	const uint8_t *buf = in;
	size_t len, trim = 0;
	size_t body = 0;
	ssize_t rc;
	bool ok = true;

	if (speed > 0) {
		len = speed;
	}
	else {
		len = inlen;
	}

	while (body > 0 || !sp_http_is_done (p)) {
		mu_assert_uint_ge (len, trim);
		if (len < trim) {
			ok = false;
			goto out;
		}

		if (body > 0) {
			rc = len - trim;
			if (body < (size_t)rc) {
				rc = body;
			}
			strncat (msg->body, (char *)buf, rc);
			body -= rc;
		}
		else {
			rc = sp_http_next (p, buf, len - trim);

			// normally rc could equal 0 if a full scan couldn't be completed
			mu_assert_int_ge (rc, 0);
			if (rc < 0) {
				char err[256];
				sp_error_string (rc, err, sizeof err);
				fprintf (stderr, "Parsing Failed:\n\terror=\"%s\"\n\tinput=", err);
				sp_fmt_str (stderr, buf, len - trim, true);
				fprintf  (stderr, "\n");
				ok = false;
				goto out;
			}

			if (p->type == SP_HTTP_REQUEST) {
				strncat (msg->as.request.method,
						(char *)buf + p->as.request.method.off,
						p->as.request.method.len);
				strncat (msg->as.request.uri,
						(char *)buf + p->as.request.uri.off,
						p->as.request.uri.len);
				msg->as.request.version = p->as.request.version;
			}
			else if (p->type == SP_HTTP_RESPONSE) {
				msg->as.response.version = p->as.response.version;
				msg->as.response.status = p->as.response.status;
				strncat (msg->as.response.reason,
						(char *)buf + p->as.response.reason.off,
						p->as.response.reason.len);
			}
			else if (p->type == SP_HTTP_FIELD) {
				strncat (msg->fields[msg->field_count].name,
						(char *)buf + p->as.field.name.off,
						p->as.field.name.len);
				strncat (msg->fields[msg->field_count].value,
						(char *)buf + p->as.field.value.off,
						p->as.field.value.len);
				msg->field_count++;
			}
			else if (p->type == SP_HTTP_BODY_START) {
				if (!p->as.body_start.chunked) {
					body = p->as.body_start.content_length;
				}
			}
			else if (p->type == SP_HTTP_BODY_CHUNK) {
				body = p->as.body_chunk.length;
			}
		}

		// trim the buffer
		buf += rc;
		trim += rc;

		if (speed > 0) {
			len += speed;
			if (len > inlen) {
				len = inlen;
			}
		}
	}

out:
	return ok;
}

static void
test_request (ssize_t speed)
{
	SpHttp p;
	sp_http_init_request (&p, false);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, request, sizeof request - 1, speed));

	mu_assert_str_eq ("GET", msg.as.request.method);
	mu_assert_str_eq ("/some/path", msg.as.request.uri);
	mu_assert_uint_eq (1, msg.as.request.version);
	mu_assert_uint_eq (7, msg.field_count);
	mu_assert_str_eq ("Empty", msg.fields[0].name);
	mu_assert_str_eq ("", msg.fields[0].value);
	mu_assert_str_eq ("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq ("", msg.fields[1].value);
	mu_assert_str_eq ("Space", msg.fields[2].name);
	mu_assert_str_eq ("value", msg.fields[2].value);
	mu_assert_str_eq ("No-Space", msg.fields[3].name);
	mu_assert_str_eq ("value", msg.fields[3].value);
	mu_assert_str_eq ("Spaces", msg.fields[4].name);
	mu_assert_str_eq ("value with spaces", msg.fields[4].value);
	mu_assert_str_eq ("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq ("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq ("Content-Length", msg.fields[6].name);
	mu_assert_str_eq ("12", msg.fields[6].value);
	mu_assert_str_eq ("Hello World!", msg.body);

	sp_http_final (&p);
}

static void
test_request_capture (ssize_t speed)
{
	SpHttp p;
	sp_http_init_request (&p, true);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		"Test: value 1\r\n"
		"TEST: value 2\r\n"
		"test: value 3\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, request, sizeof request - 1, speed));

	mu_assert_str_eq ("GET", msg.as.request.method);
	mu_assert_str_eq ("/some/path", msg.as.request.uri);
	mu_assert_uint_eq (1, msg.as.request.version);
	mu_assert_uint_eq (0, msg.field_count);
	mu_assert_str_eq ("Hello World!", msg.body);

	struct iovec iov = { NULL, 0 };
	const SpHttpEntry *e = sp_http_map_get (p.headers, "TeSt", 4);
	mu_fassert_ptr_ne (e, NULL);

	sp_http_entry_name (e, &iov);
	mu_assert_str_eq ("Test", iov.iov_base);

	mu_fassert_uint_eq (3, sp_http_entry_count (e));

	mu_fassert (sp_http_entry_value (e, 0, &iov));
	mu_assert_str_eq ("value 1", iov.iov_base);
	mu_fassert (sp_http_entry_value (e, 1, &iov));
	mu_assert_str_eq ("value 2", iov.iov_base);
	mu_fassert (sp_http_entry_value (e, 2, &iov));
	mu_assert_str_eq ("value 3", iov.iov_base);

	char buf[1024];

	mu_assert_uint_eq (sp_http_map_encode_size (p.headers), 185);
	mu_assert_uint_eq (sp_http_map_scatter_count (p.headers), 40);
	memset (buf, 0, sizeof buf);
	sp_http_map_encode (p.headers, buf);
	mu_assert_uint_eq (strlen (buf), 185);

	sp_http_map_del (p.headers, "test", 4);

	mu_assert_uint_eq (sp_http_map_encode_size (p.headers), 140);
	mu_assert_uint_eq (sp_http_map_scatter_count (p.headers), 28);
	memset (buf, 0, sizeof buf);
	sp_http_map_encode (p.headers, buf);
	mu_assert_uint_eq (strlen (buf), 140);

	sp_http_final (&p);
}

static void
test_chunked_request (ssize_t speed)
{
	SpHttp p;
	sp_http_init_request (&p, false);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, request, sizeof request - 1, speed));

	mu_assert_str_eq ("GET", msg.as.request.method);
	mu_assert_str_eq ("/some/path", msg.as.request.uri);
	mu_assert_uint_eq (1, msg.as.request.version);
	mu_assert_uint_eq (8, msg.field_count);
	mu_assert_str_eq ("Empty", msg.fields[0].name);
	mu_assert_str_eq ("", msg.fields[0].value);
	mu_assert_str_eq ("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq ("", msg.fields[1].value);
	mu_assert_str_eq ("Space", msg.fields[2].name);
	mu_assert_str_eq ("value", msg.fields[2].value);
	mu_assert_str_eq ("No-Space", msg.fields[3].name);
	mu_assert_str_eq ("value", msg.fields[3].value);
	mu_assert_str_eq ("Spaces", msg.fields[4].name);
	mu_assert_str_eq ("value with spaces", msg.fields[4].value);
	mu_assert_str_eq ("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq ("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq ("Transfer-Encoding", msg.fields[6].name);
	mu_assert_str_eq ("chunked", msg.fields[6].value);
	mu_assert_str_eq ("Trailer", msg.fields[7].name);
	mu_assert_str_eq ("trailer value", msg.fields[7].value);
	mu_assert_str_eq ("Hello World!", msg.body);
}

static void
test_chunked_request_capture (ssize_t speed)
{
	SpHttp p;
	sp_http_init_request (&p, true);

	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Test: value 1\r\n"
		"TEST: value 2\r\n"
		"test: value 3\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, request, sizeof request - 1, speed));

	mu_assert_str_eq ("GET", msg.as.request.method);
	mu_assert_str_eq ("/some/path", msg.as.request.uri);
	mu_assert_uint_eq (1, msg.as.request.version);
	mu_assert_uint_eq (0, msg.field_count);
	mu_assert_str_eq ("Hello World!", msg.body);

	struct iovec iov = { NULL, 0 };
	const SpHttpEntry *e = sp_http_map_get (p.headers, "TeSt", 4);
	mu_fassert_ptr_ne (e, NULL);

	sp_http_entry_name (e, &iov);
	mu_assert_str_eq ("Test", iov.iov_base);

	mu_fassert_uint_eq (3, sp_http_entry_count (e));

	mu_fassert (sp_http_entry_value (e, 0, &iov));
	mu_assert_str_eq ("value 1", iov.iov_base);
	mu_fassert (sp_http_entry_value (e, 1, &iov));
	mu_assert_str_eq ("value 2", iov.iov_base);
	mu_fassert (sp_http_entry_value (e, 2, &iov));
	mu_assert_str_eq ("value 3", iov.iov_base);

	char buf[1024];

	mu_assert_uint_eq (sp_http_map_encode_size (p.headers), 217);
	mu_assert_uint_eq (sp_http_map_scatter_count (p.headers), 44);
	memset (buf, 0, sizeof buf);
	sp_http_map_encode (p.headers, buf);
	mu_assert_uint_eq (strlen (buf), 217);

	sp_http_map_del (p.headers, "test", 4);

	mu_assert_uint_eq (sp_http_map_encode_size (p.headers), 172);
	mu_assert_uint_eq (sp_http_map_scatter_count (p.headers), 32);
	memset (buf, 0, sizeof buf);
	sp_http_map_encode (p.headers, buf);
	mu_assert_uint_eq (strlen (buf), 172);

	sp_http_final (&p);
}

static void
test_response (ssize_t speed)
{
	SpHttp p;
	sp_http_init_response (&p, false);

	static const uint8_t response[] = 
		"HTTP/1.1 200 OK\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Content-Length: 12\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"Hello World!"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, response, sizeof response - 1, speed));

	mu_assert_uint_eq (1, msg.as.response.version);
	mu_assert_uint_eq (200, msg.as.response.status);
	mu_assert_str_eq ("OK", msg.as.response.reason);
	mu_assert_uint_eq (7, msg.field_count);
	mu_assert_str_eq ("Empty", msg.fields[0].name);
	mu_assert_str_eq ("", msg.fields[0].value);
	mu_assert_str_eq ("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq ("", msg.fields[1].value);
	mu_assert_str_eq ("Space", msg.fields[2].name);
	mu_assert_str_eq ("value", msg.fields[2].value);
	mu_assert_str_eq ("No-Space", msg.fields[3].name);
	mu_assert_str_eq ("value", msg.fields[3].value);
	mu_assert_str_eq ("Spaces", msg.fields[4].name);
	mu_assert_str_eq ("value with spaces", msg.fields[4].value);
	mu_assert_str_eq ("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq ("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq ("Content-Length", msg.fields[6].name);
	mu_assert_str_eq ("12", msg.fields[6].value);
	mu_assert_str_eq ("Hello World!", msg.body);

	sp_http_final (&p);
}

static void
test_chunked_response (ssize_t speed)
{
	SpHttp p;
	sp_http_init_response (&p, false);

	static const uint8_t response[] = 
		"HTTP/1.1 200 OK\r\n"
		"Empty:\r\n"
		"Empty-Space: \r\n"
		"Space: value\r\n"
		"No-Space:value\r\n"
		"Spaces: value with spaces\r\n"
		"Pre-Spaces:           value with prefix spaces\r\n"
		"Transfer-Encoding: chunked\r\n"
		//"Newlines: stuff\r\n with\r\n newlines\r\n"
		//"String: stuff\r\n \"with\r\n\\\"strings\\\" and things\r\n\"\r\n"
		"\r\n"
		"5\r\n"
		"Hello\r\n"
		"7\r\n"
		" World!\r\n"
		"0\r\n"
		"Trailer: trailer value\r\n"
		"\r\n"
		;

	Message msg;
	mu_fassert (parse (&p, &msg, response, sizeof response - 1, speed));

	mu_assert_uint_eq (1, msg.as.response.version);
	mu_assert_uint_eq (200, msg.as.response.status);
	mu_assert_str_eq ("OK", msg.as.response.reason);
	mu_assert_uint_eq (8, msg.field_count);
	mu_assert_str_eq ("Empty", msg.fields[0].name);
	mu_assert_str_eq ("", msg.fields[0].value);
	mu_assert_str_eq ("Empty-Space", msg.fields[1].name);
	mu_assert_str_eq ("", msg.fields[1].value);
	mu_assert_str_eq ("Space", msg.fields[2].name);
	mu_assert_str_eq ("value", msg.fields[2].value);
	mu_assert_str_eq ("No-Space", msg.fields[3].name);
	mu_assert_str_eq ("value", msg.fields[3].value);
	mu_assert_str_eq ("Spaces", msg.fields[4].name);
	mu_assert_str_eq ("value with spaces", msg.fields[4].value);
	mu_assert_str_eq ("Pre-Spaces", msg.fields[5].name);
	mu_assert_str_eq ("value with prefix spaces", msg.fields[5].value);
	mu_assert_str_eq ("Transfer-Encoding", msg.fields[6].name);
	mu_assert_str_eq ("chunked", msg.fields[6].value);
	mu_assert_str_eq ("Trailer", msg.fields[7].name);
	mu_assert_str_eq ("trailer value", msg.fields[7].value);
	mu_assert_str_eq ("Hello World!", msg.body);

	sp_http_final (&p);
}

static void
test_invalid_header (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Header\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, SP_HTTP_ESYNTAX);
}

static void
test_limit_method_size (void)
{
	static const uint8_t request[] = 
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX /some/path HTTP/1.1\r\n"
		"\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_assert_int_eq (rc, 54);
}

static void
test_exceed_method_size (void)
{
	static const uint8_t request[] = 
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX /some/path HTTP/1.1\r\n"
		"\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_assert_int_eq (rc, SP_HTTP_ESIZE);
}

static void
test_limit_name_size (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX: value\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, 265);
}

static void
test_exceed_name_size (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX: value\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, SP_HTTP_ESIZE);
}

static void
test_limit_value_size (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, 1031);
}

static void
test_exceed_value_size (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, SP_HTTP_ESIZE);
}

static void
test_increase_value_size (void)
{
	static const uint8_t request[] = 
		"GET /some/path HTTP/1.1\r\n"
		"Name:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
		"\r\n"
		;

	SpHttp p;
	ssize_t rc;

	sp_http_init_request (&p, false);
	p.max_value = 2048;
	rc = sp_http_next (&p, request, sizeof request - 1);
	mu_fassert_int_eq (rc, 25);
	mu_assert_int_eq (p.type, SP_HTTP_REQUEST);
	rc = sp_http_next (&p, request + rc, sizeof request - 1 - rc);
	mu_assert_int_eq (rc, 1032);
}

static void
test_cc_max_stale (void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "max-stale";
	const char buf2[] = "max-stale=123";

	n = sp_cache_control_parse (&cc, buf1, (sizeof buf1) - 1);
	mu_assert_int_eq (n, (sizeof buf1) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE, SP_CACHE_MAX_STALE);
	mu_assert_int_ne (cc.type & SP_CACHE_MAX_STALE_TIME, SP_CACHE_MAX_STALE_TIME);

	n = sp_cache_control_parse (&cc, buf2, (sizeof buf2) - 1);
	mu_assert_int_eq (n, (sizeof buf2) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE, SP_CACHE_MAX_STALE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE_TIME, SP_CACHE_MAX_STALE_TIME);
	mu_assert_int_eq (cc.max_stale, 123);
}

static void
test_cc_private (void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "private";
	const char buf2[] = "private=\"string\"";
	const char buf3[] = "private=token"; // non-standard
	char val[16];

	n = sp_cache_control_parse (&cc, buf1, (sizeof buf1) - 1);
	mu_assert_int_eq (n, (sizeof buf1) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_PRIVATE, SP_CACHE_PRIVATE);
	mu_assert_uint_eq (cc.private.len, 0);

	n = sp_cache_control_parse (&cc, buf2, (sizeof buf2) - 1);
	mu_assert_int_eq (n, (sizeof buf2) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_PRIVATE, SP_CACHE_PRIVATE);
	mu_assert_uint_eq (cc.private.len, 6);
	memcpy (val, buf2+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq (val, "string");

	n = sp_cache_control_parse (&cc, buf3, (sizeof buf3) - 1);
	mu_assert_int_eq (n, (sizeof buf3) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf3 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_PRIVATE, SP_CACHE_PRIVATE);
	mu_assert_uint_eq (cc.private.len, 5);
	memcpy (val, buf3+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq (val, "token");
}

static void
test_cc_no_cache (void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf1[] = "no-cache";
	const char buf2[] = "no-cache=\"string\"";
	const char buf3[] = "no-cache=token"; // non-standard
	char val[16];

	n = sp_cache_control_parse (&cc, buf1, (sizeof buf1) - 1);
	mu_assert_int_eq (n, (sizeof buf1) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf1 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);
	mu_assert_uint_eq (cc.no_cache.len, 0);

	n = sp_cache_control_parse (&cc, buf2, (sizeof buf2) - 1);
	mu_assert_int_eq (n, (sizeof buf2) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf2 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);
	mu_assert_uint_eq (cc.no_cache.len, 6);
	memcpy (val, buf2+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq (val, "string");

	n = sp_cache_control_parse (&cc, buf3, (sizeof buf3) - 1);
	mu_assert_int_eq (n, (sizeof buf3) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf3 + (-1 - n));
		return;
	}
	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);
	mu_assert_uint_eq (cc.no_cache.len, 5);
	memcpy (val, buf3+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq (val, "token");
}

static void
test_cc_group (void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf[] = "max-age=0, no-cache, no-store";
	n = sp_cache_control_parse (&cc, buf, (sizeof buf) - 1);
	mu_assert_int_eq (n, (sizeof buf) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq (cc.type & SP_CACHE_MAX_AGE, SP_CACHE_MAX_AGE);
	mu_assert_int_eq (cc.max_age, 0);

	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);

	mu_assert_int_eq (cc.type & SP_CACHE_NO_STORE, SP_CACHE_NO_STORE);

	mu_assert_int_eq (cc.type, SP_CACHE_MAX_AGE | SP_CACHE_NO_CACHE | SP_CACHE_NO_STORE);
}

static void
test_cc_group_semi (void)
{
	SpCacheControl cc;
	ssize_t n;

	const char buf[] = "max-age=0; no-cache; no-store";
	n = sp_cache_control_parse (&cc, buf, (sizeof buf) - 1);
	mu_assert_int_eq (n, (sizeof buf) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq (cc.type & SP_CACHE_MAX_AGE, SP_CACHE_MAX_AGE);
	mu_assert_int_eq (cc.max_age, 0);

	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);

	mu_assert_int_eq (cc.type & SP_CACHE_NO_STORE, SP_CACHE_NO_STORE);

	mu_assert_int_eq (cc.type, SP_CACHE_MAX_AGE | SP_CACHE_NO_CACHE | SP_CACHE_NO_STORE);
}

static void
test_cc_all (void)
{
	const char buf[] =
		"public,"
		"private,"
		"private=\"stuff\","
		"no-cache,"
		"no-cache=\"thing\","
		"no-store,"
		"max-age=12,"
		"s-maxage=34,"
		"max-stale,"
		"max-stale=56,"
		"min-fresh=78,"
		"no-transform,"
		"only-if-cached,"
		"must-revalidate,"
		"proxy-revalidate,"
		"ext1,"
		"ext2=something,"
		"ext3=\"other\""
		;

	SpCacheControl cc;
	ssize_t n = sp_cache_control_parse (&cc, buf, (sizeof buf) - 1);
	mu_assert_int_eq (n, (sizeof buf) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq (cc.type & SP_CACHE_PUBLIC, SP_CACHE_PUBLIC);
	mu_assert_int_eq (cc.type & SP_CACHE_PRIVATE, SP_CACHE_PRIVATE);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_STORE, SP_CACHE_NO_STORE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_AGE, SP_CACHE_MAX_AGE);
	mu_assert_int_eq (cc.type & SP_CACHE_S_MAXAGE, SP_CACHE_S_MAXAGE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE, SP_CACHE_MAX_STALE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE_TIME, SP_CACHE_MAX_STALE_TIME);
	mu_assert_int_eq (cc.type & SP_CACHE_MIN_FRESH, SP_CACHE_MIN_FRESH);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_TRANSFORM, SP_CACHE_NO_TRANSFORM);
	mu_assert_int_eq (cc.type & SP_CACHE_ONLY_IF_CACHED, SP_CACHE_ONLY_IF_CACHED);
	mu_assert_int_eq (cc.type & SP_CACHE_MUST_REVALIDATE, SP_CACHE_MUST_REVALIDATE);
	mu_assert_int_eq (cc.type & SP_CACHE_PROXY_REVALIDATE, SP_CACHE_PROXY_REVALIDATE);

	mu_assert_int_eq (cc.max_age, 12);
	mu_assert_int_eq (cc.s_maxage, 34);
	mu_assert_int_eq (cc.max_stale, 56);
	mu_assert_int_eq (cc.min_fresh, 78);

	char val[64];

	memcpy (val, buf+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq (val, "stuff");

	memcpy (val, buf+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq (val, "thing");
}

static void
test_cc_all_semi (void)
{
	const char buf[] =
		"public;"
		"private;"
		"private=\"stuff\";"
		"no-cache;"
		"no-cache=\"thing\","
		"no-store;"
		"max-age=12;"
		"s-maxage=34;"
		"max-stale;"
		"max-stale=56;"
		"min-fresh=78;"
		"no-transform;"
		"only-if-cached;"
		"must-revalidate;"
		"proxy-revalidate;"
		"ext1;"
		"ext2=something;"
		"ext3=\"other\""
		;

	SpCacheControl cc;
	ssize_t n = sp_cache_control_parse (&cc, buf, (sizeof buf) - 1);
	mu_assert_int_eq (n, (sizeof buf) - 1);
	if (n < 0) {
		printf ("ERROR: %s\n", buf + (-1 - n));
		return;
	}

	mu_assert_int_eq (cc.type & SP_CACHE_PUBLIC, SP_CACHE_PUBLIC);
	mu_assert_int_eq (cc.type & SP_CACHE_PRIVATE, SP_CACHE_PRIVATE);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_CACHE, SP_CACHE_NO_CACHE);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_STORE, SP_CACHE_NO_STORE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_AGE, SP_CACHE_MAX_AGE);
	mu_assert_int_eq (cc.type & SP_CACHE_S_MAXAGE, SP_CACHE_S_MAXAGE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE, SP_CACHE_MAX_STALE);
	mu_assert_int_eq (cc.type & SP_CACHE_MAX_STALE_TIME, SP_CACHE_MAX_STALE_TIME);
	mu_assert_int_eq (cc.type & SP_CACHE_MIN_FRESH, SP_CACHE_MIN_FRESH);
	mu_assert_int_eq (cc.type & SP_CACHE_NO_TRANSFORM, SP_CACHE_NO_TRANSFORM);
	mu_assert_int_eq (cc.type & SP_CACHE_ONLY_IF_CACHED, SP_CACHE_ONLY_IF_CACHED);
	mu_assert_int_eq (cc.type & SP_CACHE_MUST_REVALIDATE, SP_CACHE_MUST_REVALIDATE);
	mu_assert_int_eq (cc.type & SP_CACHE_PROXY_REVALIDATE, SP_CACHE_PROXY_REVALIDATE);

	mu_assert_int_eq (cc.max_age, 12);
	mu_assert_int_eq (cc.s_maxage, 34);
	mu_assert_int_eq (cc.max_stale, 56);
	mu_assert_int_eq (cc.min_fresh, 78);

	char val[64];

	memcpy (val, buf+cc.private.off, cc.private.len);
	val[cc.private.len] = '\0';
	mu_assert_str_eq (val, "stuff");

	memcpy (val, buf+cc.no_cache.off, cc.no_cache.len);
	val[cc.no_cache.len] = '\0';
	mu_assert_str_eq (val, "thing");
}


int
main (void)
{
	mu_init ("http");

	/**
	 * Parse the input at varying "speeds". The speed is the number
	 * of bytes to emulate reading at each pass of the parser.
	 * 0 indicates that all bytes should be available at the start
	 * of the parser.
	 */
	for (ssize_t i = 0; i <= 250; i++) {
		test_request (i);
		test_chunked_request (i);
		test_request_capture (i);
		test_chunked_request_capture (i);
		test_response (i);
		test_chunked_response (i);
	}

	test_invalid_header ();

	test_limit_method_size ();
	test_exceed_method_size ();
	test_limit_name_size ();
	test_exceed_name_size ();
	test_limit_value_size ();
	test_exceed_value_size ();
	test_increase_value_size ();

	test_cc_max_stale ();
	test_cc_private ();
	test_cc_no_cache ();
	test_cc_group ();
	test_cc_group_semi ();
	test_cc_all ();
	test_cc_all_semi ();

	mu_assert (sp_alloc_summary ());
}

