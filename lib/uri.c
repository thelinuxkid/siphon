#include "siphon/uri.h"
#include "siphon/path.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

ssize_t
sp_uri_copy (
		const SpUri *u, const char *buf,
		SpUri *out, char *outbuf, size_t len)
{
	assert (u != NULL);
	assert (buf != NULL);
	assert (out != NULL);
	assert (outbuf != NULL);

	uint16_t ulen = sp_uri_length (u);
	if (ulen > len) {
		errno = ENAMETOOLONG;
		return -1;
	}

	memcpy (outbuf, buf, ulen);
	if (len > ulen) {
		outbuf[ulen] = '\0';
	}
	memcpy (out, u, sizeof *u);
	return ulen;
}

#include <stdio.h>

ssize_t
sp_uri_join (
		const SpUri *a, const char *abuf,
		const SpUri *b, const char *bbuf,
		SpUri *out, char *outbuf, size_t len)
{
	if (sp_uri_eq (a, abuf, b, bbuf) || sp_uri_length (a) == 0) {
		return sp_uri_copy (b, bbuf, out, outbuf, len);
	}
	if (b == NULL || sp_uri_length (b) == 0) {
		return sp_uri_copy (a, abuf, out, outbuf, len);
	}

	char *p = outbuf;

	// find the first segment of the join URI
	SpUriSegment seg = sp_uri_find_segment (b, SP_URI_SEGMENT_FIRST, true);
	SpUriSegment end;
	SpRange16 rng;

	// get base URI up to first segment of join URI
	end = sp_uri_sub (a, SP_URI_SEGMENT_FIRST, seg-1, &rng);
	if (end >= 0 && rng.len > 0) {
		memcpy (p, abuf + rng.off, rng.len);
		p += rng.len;
		*p = '\0';
	}

	// if the first segment of the join URI is a path, join the paths
	if (seg == SP_URI_PATH) {
		rng = a->seg[SP_URI_PATH];
		sp_path_pop (abuf, &rng, 1);

		uint16_t plen = sp_path_join (
			p, len - (p - outbuf),
			abuf + rng.off, rng.len,
			bbuf + b->seg[SP_URI_PATH].off, b->seg[SP_URI_PATH].len,
			SP_PATH_URI);

		if (plen == 0) {
			if (errno != 0) {
				return -1;
			}
		}
		else {
			plen = sp_path_clean (p, plen, SP_PATH_URI);
			p += plen;
		}
		seg++;
	}

	// add any remaining segments from the join URI
	if (end > SP_URI_SCHEME && seg < SP_URI_PATH) {
		if (end < SP_URI_HOST) {
			*p = '@';
			p++;
		}
		end = sp_uri_range (b, seg, b->last, &rng);
	}
	else {
		end = sp_uri_sub (b, seg, b->last, &rng);
	}
	if (end >= 0 && rng.len > 0) {
		memcpy (p, bbuf + rng.off, rng.len);
		p += rng.len;
		*p = '\0';
	}

	return sp_uri_parse (out, outbuf, p - outbuf);
}

bool
sp_uri_eq (const SpUri *a, const char *abuf, const SpUri *b, const char *bbuf)
{
	assert (a != NULL);
	assert (abuf != NULL);
	assert (b != NULL);
	assert (bbuf != NULL);

	if (a == b) return true;
	if (a == NULL
		|| b == NULL
		|| a->first != b->first
		|| a->last != b->last
		|| a->host != b->host
		|| a->port != b->port) {
		return false;
	}
	uint16_t alen = sp_uri_length (a);
	uint16_t blen = sp_uri_length (b);
	return alen == blen && memcmp (abuf, bbuf, alen) == 0;
}

uint16_t
sp_uri_length (const SpUri *u)
{
	assert (u != NULL);

	return u->seg[u->last].off + u->seg[u->last].len;
}

SpUriSegment
sp_uri_sub (const SpUri *u, SpUriSegment start, SpUriSegment end, SpRange16 *out)
{
	assert (u != NULL);
	assert (out != NULL);

	if (sp_uri_adjust_range (u, &start, &end, true) < 0) {
		return SP_URI_NONE;
	}

	SpRange16 r = {
		u->seg[start].off,
		u->seg[end].off - u->seg[start].off + u->seg[end].len
	};
	switch (start) {
		case SP_URI_USER:
		case SP_URI_PASSWORD:
			// fallthrough
		case SP_URI_HOST:
			r.off -= 2;
			r.len += 2;
			break;
		case SP_URI_QUERY:
		case SP_URI_FRAGMENT:
			if (u->seg[start].len > 0) {
				r.off--;
				r.len++;
			}
			break;
		default:
			break;
	}
	if (end == SP_URI_SCHEME) {
		r.len++;
	}
	memcpy (out, &r, sizeof r);
	return end;
}

SpUriSegment
sp_uri_range (const SpUri *u, SpUriSegment start, SpUriSegment end, SpRange16 *out)
{
	assert (u != NULL);
	assert (out != NULL);

	if (sp_uri_adjust_range (u, &start, &end, false) < 0) {
		return SP_URI_NONE;
	}
	out->off = u->seg[start].off;
	out->len = u->seg[end].off - u->seg[start].off + u->seg[end].len;
	return end;
}

int
sp_uri_adjust_range (const SpUri *u, SpUriSegment *start, SpUriSegment *end, bool valid)
{
	assert (u != NULL);

	SpUriSegment s = *start, e = *end;

	// verify that the expected range is reasonable
	if (s > e || s < SP_URI_SEGMENT_FIRST || e > SP_URI_SEGMENT_LAST) {
		errno = EINVAL;
		return -1;
	}

	// a valid netloc-relative URL must start with '//'
	if (valid && s > SP_URI_USER && s <= SP_URI_PORT) {
		s = SP_URI_USER;
	}

	for (; s < e && u->seg[s].len == 0; s++) {}
	for (; e > s && u->seg[e].len == 0; e--) {}

#if 0
	if (valid && s != SP_URI_PATH && s == e && u->seg[s].len == 0) {
		errno = ERANGE;
		return -1;
	}
#endif

	*start = s;
	*end = e;
	return 0;
}

SpUriSegment
sp_uri_find_segment (const SpUri *u, SpUriSegment start, bool nonempty)
{
	assert (u != NULL);
	assert (start >= SP_URI_SEGMENT_FIRST && start <= SP_URI_SEGMENT_LAST);

	if (start < SP_URI_SEGMENT_FIRST || start > SP_URI_SEGMENT_LAST) {
		return SP_URI_NONE;
	}
	if (!nonempty && start == SP_URI_PATH) {
		return SP_URI_PATH;
	}
	if (start < SP_URI_SEGMENT_FIRST || start > SP_URI_SEGMENT_LAST) {
		return SP_URI_NONE;
	}

	unsigned x = (unsigned)start, y;
	SpUriSegment def;
	if (!nonempty && start < SP_URI_PATH) {
		y = SP_URI_PATH;
		def = SP_URI_PATH;
	}
	else {
		y = u->last;
		def = SP_URI_NONE;
	}

	for (; x <= y; x++) {
		if (u->seg[x].len > 0) {
			return (SpUriSegment)x;
		}
	}
	return def;
}

SpUriSegment
sp_uri_rfind_segment (const SpUri *u, SpUriSegment start, bool nonempty)
{
	assert (u != NULL);

	if (!nonempty && start == SP_URI_PATH) {
		return SP_URI_PATH;
	}
	if (start < SP_URI_SEGMENT_FIRST || start > SP_URI_SEGMENT_LAST) {
		return SP_URI_NONE;
	}

	unsigned x = (unsigned)start, y;
	SpUriSegment def;
	if (!nonempty && start > SP_URI_PATH) {
		y = SP_URI_PATH;
		def = SP_URI_PATH;
	}
	else {
		y = u->first;
		def = SP_URI_NONE;
	}

	for (; x >= y; x--) {
		if (u->seg[x].len > 0) {
			return (SpUriSegment)x;
		}
	}
	return def;
}

bool
sp_uri_has_segment (const SpUri *u, SpUriSegment seg)
{
	assert (u != NULL);

	return (seg == SP_URI_PATH ||
			(SP_URI_SEGMENT_VALID (seg) && u->seg[seg].len));
}

bool
sp_uri_is_absolute (const SpUri *u)
{
	return (u &&
			sp_uri_has_segment (u, SP_URI_SCHEME) &&
			sp_uri_has_segment (u, SP_URI_HOST));
}

