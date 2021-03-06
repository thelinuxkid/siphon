#ifndef SIPHON_RANGE_H
#define SIPHON_RANGE_H

typedef struct {
	uint8_t off, len;
} SpRange8;

typedef struct {
	uint16_t off, len;
} SpRange16;

#define SP_RANGE_EQ_MEM(r, rbuf, buf, blen) \
	((r).len == (blen) && \
	 memcmp ((rbuf)+(r).off, (buf), (blen)) == 0)

#define SP_RANGE_PREFIX_MEM(r, rbuf, buf, blen) \
	((r).len >= (blen) && \
	 memcmp ((rbuf)+(r).off, (buf), (blen)) == 0)

#define SP_RANGE_SUFFIX_MEM(r, rbuf, buf, blen) \
	((r).len >= (blen) && \
	 memcmp ((rbuf)+(r).off+(r).len-(blen), (buf), (blen)) == 0)

#define SP_RANGE_EQ_STR(r, rbuf, str) \
	SP_RANGE_EQ_MEM(r, rbuf, str, strlen (str))

#define SP_RANGE_PREFIX_STR(r, rbuf, str) \
	SP_RANGE_PREFIX_MEM(r, rbuf, str, strlen (str))

#define SP_RANGE_SUFFIX_STR(r, rbuf, str) \
	SP_RANGE_SUFFIX_MEM(r, rbuf, str, strlen (str))

#define SP_RANGE_EQ(a, abuf, b, bbuf) \
	SP_RANGE_EQ_MEM(a, abuf, (bbuf)+(b).off, (b).len)

#define SP_RANGE_PREFIX(a, abuf, b, bbuf) \
	SP_RANGE_PREFIX_MEM(a, abuf, (bbuf)+(b).off, (b).len)

#define SP_RANGE_SUFFIX(a, abuf, b, bbuf) \
	SP_RANGE_SUFFIX_MEM(a, abuf, (bbuf)+(b).off, (b).len)

#endif

