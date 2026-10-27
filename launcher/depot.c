/*
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 *
 * Formats as implemented by SteamKit2 (DepotManifest, VZipUtil,
 * VZstdUtil, CDNClient): a manifest is a ZIP holding sections
 * "magic, length, protobuf"; chunks are encrypted with the depot key and
 * compressed with LZMA ("VZa"), Zstandard ("VSZa") or deflate (ZIP).
 */
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <lzma.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <zstd.h>

#include "crypto.h"
#include "depot.h"
#include "net.h"
#include "pb.h"

#define MAGIC_PAYLOAD	0x71F617D0
#define MAGIC_METADATA	0x1F4812BE
#define MAGIC_SIGNATURE	0x1B81B817
#define MAGIC_END	0x32C415AB

static int
inflate_raw(const uint8_t *p, size_t n, size_t outlen, struct buf *out)
{
	z_stream z = { 0 };
	int r;

	if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
		return -1;
	out->len = 0;
	buf_put(out, NULL, 0);
	out->p = xrealloc(out->p, outlen + 1);
	out->cap = outlen + 1;
	z.next_in = (uint8_t *)p;
	z.avail_in = n;
	z.next_out = out->p;
	z.avail_out = outlen;
	r = inflate(&z, Z_FINISH);
	out->len = outlen - z.avail_out;
	inflateEnd(&z);
	return r == Z_STREAM_END ? 0 : -1;
}

/* The first file in a ZIP archive, via the central directory. */
static int
unzip_first(const uint8_t *p, size_t n, struct buf *out)
{
	const uint8_t *eocd = NULL, *cd;
	uint32_t cdoff, csize, usize, lho;
	uint16_t method, nlen, xlen;
	size_t i;

	if (n < 22)
		return -1;
	for (i = n - 22; i + 1 > 0 && i + 65557 >= n; i--) {
		if (le32(p + i) == 0x06054b50) {
			eocd = p + i;
			break;
		}
		if (i == 0)
			break;
	}
	if (eocd == NULL)
		return -1;
	cdoff = le32(eocd + 16);
	if (cdoff + 46 > n || le32(p + cdoff) != 0x02014b50)
		return -1;
	cd = p + cdoff;
	method = cd[10] | cd[11] << 8;
	csize = le32(cd + 20);
	usize = le32(cd + 24);
	lho = le32(cd + 42);
	if (lho + 30 > n || le32(p + lho) != 0x04034b50)
		return -1;
	nlen = p[lho + 26] | p[lho + 27] << 8;
	xlen = p[lho + 28] | p[lho + 29] << 8;
	if ((size_t)lho + 30 + nlen + xlen + csize > n)
		return -1;
	p += lho + 30 + nlen + xlen;
	if (method == 0) {
		out->len = 0;
		buf_put(out, p, csize);
		return 0;
	}
	if (method != 8)
		return -1;
	return inflate_raw(p, csize, usize, out);
}

static void
parse_payload(const uint8_t *p, size_t n, struct manifest *m)
{
	struct pbr r, fr;
	struct pbf f, g;

	pbr_init(&r, p, n);
	while (pbr_next(&r, &f) == 1) {
		struct mfile mf = { 0 };
		if (f.field != 1 || f.wt != PB_LEN)
			continue;
		pbr_init(&fr, f.data, f.len);
		while (pbr_next(&fr, &g) == 1) {
			switch (g.field) {
			case 1:
				mf.name = xmalloc(g.len + 1);
				memcpy(mf.name, g.data, g.len);
				mf.name[g.len] = '\0';
				break;
			case 2: mf.size = g.v; break;
			case 3: mf.flags = (uint32_t)g.v; break;
			case 5:
				if (g.len == 20)
					memcpy(mf.sha, g.data, 20);
				break;
			case 6: {
				struct chunk c = { 0 };
				const uint8_t *d;
				size_t dl;
				uint64_t v;
				if (pb_get_bytes(g.data, g.len, 1, &d, &dl) &&
				    dl == 20)
					memcpy(c.sha, d, 20);
				if (pb_get_u64(g.data, g.len, 3, &v))
					c.offset = v;
				if (pb_get_u64(g.data, g.len, 4, &v))
					c.size = (uint32_t)v;
				if (pb_get_u64(g.data, g.len, 5, &v))
					c.csize = (uint32_t)v;
				mf.chunks = xrealloc(mf.chunks,
				    (mf.nchunks + 1) * sizeof(c));
				mf.chunks[mf.nchunks++] = c;
				break;
			}
			case 7:
				mf.link = xmalloc(g.len + 1);
				memcpy(mf.link, g.data, g.len);
				mf.link[g.len] = '\0';
				break;
			}
		}
		if (mf.name == NULL)
			mf.name = xstrdup("");
		m->files = xrealloc(m->files, (m->nfiles + 1) * sizeof(mf));
		m->files[m->nfiles++] = mf;
	}
}

/* Decrypt and normalise a file name; reject anything leaving the dir. */
static int
fix_name(struct mfile *f, const uint8_t key[32], int encrypted)
{
	char *s, *seg;
	size_t i;

	if (encrypted) {
		struct buf raw = { 0 }, dec = { 0 };
		if (b64_decode(f->name, &raw) == -1 ||
		    steam_decrypt(key, raw.p, raw.len, &dec) == -1) {
			buf_free(&raw);
			buf_free(&dec);
			return -1;
		}
		free(f->name);
		f->name = xstrdup(buf_str(&dec));
		buf_free(&raw);
		buf_free(&dec);
	}
	for (s = f->name; *s; s++)
		if (*s == '\\')
			*s = '/';
	while (f->name[0] == '/')
		memmove(f->name, f->name + 1, strlen(f->name));
	i = strlen(f->name);
	while (i > 0 && f->name[i - 1] == '/')
		f->name[--i] = '\0';
	s = xstrdup(f->name);
	for (seg = strtok(s, "/"); seg; seg = strtok(NULL, "/"))
		if (strcmp(seg, "..") == 0) {
			free(s);
			return -1;
		}
	free(s);
	return 0;
}

int
manifest_fetch(char **servers, uint32_t depot, uint64_t gid, uint64_t code,
    const uint8_t key[32], struct manifest *m)
{
	struct http_resp r;
	struct buf z = { 0 };
	const uint8_t *p, *end;
	int encrypted = 0, ok = 0, s;
	size_t i;

	memset(m, 0, sizeof(*m));
	m->depot = depot;
	m->gid = gid;
	for (s = 0; servers[s] && !ok; s++) {
		char *url = code ?
		    xasprintf("%s/depot/%u/manifest/%llu/5/%llu", servers[s],
		    depot, (unsigned long long)gid, (unsigned long long)code) :
		    xasprintf("%s/depot/%u/manifest/%llu/5", servers[s], depot,
		    (unsigned long long)gid);
		debug("GET %s", url);
		if (http_request(NULL, "GET", url, NULL, NULL, 0, &r) == 0 &&
		    r.status == 200)
			ok = 1;
		else {
			debug("manifest from %s: HTTP %d", servers[s], r.status);
			http_resp_free(&r);
		}
		free(url);
	}
	if (!ok) {
		warnx_("cannot download the manifest of depot %u", depot);
		return -1;
	}
	if (unzip_first(r.body.p, r.body.len, &z) == -1) {
		http_resp_free(&r);
		warnx_("bad manifest archive for depot %u", depot);
		return -1;
	}
	http_resp_free(&r);
	for (p = z.p, end = z.p + z.len; end - p >= 8;) {
		uint32_t magic = le32(p), len = le32(p + 4);
		p += 8;
		if (magic == MAGIC_END || len > (size_t)(end - p))
			break;
		if (magic == MAGIC_PAYLOAD)
			parse_payload(p, len, m);
		else if (magic == MAGIC_METADATA) {
			uint64_t v;
			if (pb_get_u64(p, len, 4, &v))
				encrypted = v != 0;
		}
		p += len;
	}
	buf_free(&z);
	for (i = 0; i < m->nfiles; i++) {
		if (fix_name(&m->files[i], key, encrypted) == -1) {
			warnx_("bad file name in depot %u", depot);
			manifest_free(m);
			return -1;
		}
		if (!(m->files[i].flags & DEPOT_FLAG_DIRECTORY))
			m->total += m->files[i].size;
	}
	return 0;
}

void
manifest_free(struct manifest *m)
{
	size_t i;

	for (i = 0; i < m->nfiles; i++) {
		free(m->files[i].name);
		free(m->files[i].link);
		free(m->files[i].chunks);
	}
	free(m->files);
	memset(m, 0, sizeof(*m));
}

static int
lzma_unpack(const uint8_t *p, size_t n, size_t outlen, struct buf *out)
{
	lzma_filter filters[2];
	lzma_stream s = LZMA_STREAM_INIT;
	lzma_ret r;

	/* "VZa", uint32, 5 property bytes, data, crc32, size, "zv" */
	if (n < 7 + 5 + 10)
		return -1;
	filters[0].id = LZMA_FILTER_LZMA1;
	filters[0].options = NULL;
	filters[1].id = LZMA_VLI_UNKNOWN;
	if (lzma_properties_decode(&filters[0], NULL, p + 7, 5) != LZMA_OK)
		return -1;
	r = lzma_raw_decoder(&s, filters);
	free(filters[0].options);
	if (r != LZMA_OK)
		return -1;
	out->len = 0;
	buf_put(out, NULL, 0);
	out->p = xrealloc(out->p, outlen + 1);
	out->cap = outlen + 1;
	s.next_in = p + 12;
	s.avail_in = n - 12 - 10;
	s.next_out = out->p;
	s.avail_out = outlen;
	r = lzma_code(&s, LZMA_RUN);
	if (r == LZMA_OK && s.avail_out > 0)
		r = lzma_code(&s, LZMA_FINISH);
	out->len = outlen - s.avail_out;
	lzma_end(&s);
	return (r == LZMA_OK || r == LZMA_STREAM_END) && out->len == outlen ?
	    0 : -1;
}

static int
zstd_unpack(const uint8_t *p, size_t n, size_t outlen, struct buf *out)
{
	size_t fsz, r;

	/* "VSZa", uint32 crc, zstd frame, footer */
	if (n < 8)
		return -1;
	fsz = ZSTD_findFrameCompressedSize(p + 8, n - 8);
	if (ZSTD_isError(fsz))
		return -1;
	out->len = 0;
	buf_put(out, NULL, 0);
	out->p = xrealloc(out->p, outlen + 1);
	out->cap = outlen + 1;
	r = ZSTD_decompress(out->p, outlen, p + 8, fsz);
	if (ZSTD_isError(r) || r != outlen)
		return -1;
	out->len = r;
	return 0;
}

static int
chunk_unpack(const uint8_t *key, const uint8_t *in, size_t n,
    const struct chunk *c, struct buf *out)
{
	struct buf dec = { 0 };
	uint8_t h[20];
	int r = -1;

	if (steam_decrypt(key, in, n, &dec) == -1)
		goto out;
	if (dec.len >= 3 && memcmp(dec.p, "VZa", 3) == 0)
		r = lzma_unpack(dec.p, dec.len, c->size, out);
	else if (dec.len >= 4 && memcmp(dec.p, "VSZa", 4) == 0)
		r = zstd_unpack(dec.p, dec.len, c->size, out);
	else if (dec.len >= 4 && le32(dec.p) == 0x04034b50)
		r = unzip_first(dec.p, dec.len, out);
	else if (dec.len == c->size) {
		out->len = 0;
		buf_put(out, dec.p, dec.len);
		r = 0;
	}
	if (r == 0) {
		sha1(out->p, out->len, h);
		if (memcmp(h, c->sha, 20) != 0)
			r = -1;
	}
out:
	buf_free(&dec);
	return r;
}

struct job {
	const struct mfile	*file;
	const struct chunk	*chunk;
	char			*path;
};

struct pool {
	char			**servers;
	const uint8_t		*key;
	struct job		*jobs;
	size_t			 njobs, next;
	uint64_t		 done_bytes, total_bytes;
	uint32_t		 depot;
	int			 failed, nworkers;
	pthread_mutex_t		 mu;
	double			 start, last_print;
};

static void
progress(struct pool *p, int final)
{
	double t = now_sec(), el = t - p->start;

	if (!final && t - p->last_print < 0.5)
		return;
	p->last_print = t;
	fprintf(stderr, "\r  %6.1f / %.1f MB  %5.1f MB/s   ",
	    p->done_bytes / 1048576.0, p->total_bytes / 1048576.0,
	    el > 0 ? p->done_bytes / 1048576.0 / el : 0);
	if (final)
		fputc('\n', stderr);
}

static void *
worker(void *arg)
{
	struct pool *p = arg;
	struct conn c = { .fd = -1 };
	struct buf out = { 0 };
	char hex[41];
	int server, ns = 0;

	while (p->servers[ns])
		ns++;
	pthread_mutex_lock(&p->mu);
	server = p->nworkers++ % ns;	/* spread workers over the servers */
	pthread_mutex_unlock(&p->mu);

	for (;;) {
		struct job *j;
		int tries, ok = 0, fd;

		pthread_mutex_lock(&p->mu);
		if (p->next >= p->njobs || p->failed) {
			pthread_mutex_unlock(&p->mu);
			break;
		}
		j = &p->jobs[p->next++];
		pthread_mutex_unlock(&p->mu);

		hex_encode(j->chunk->sha, 20, hex);
		for (tries = 0; tries < 6 && !ok; tries++) {
			struct http_resp r;
			char *url = xasprintf("%s/depot/%u/chunk/%s",
			    p->servers[(server + tries) % ns], p->depot, hex);
			if (http_request(&c, "GET", url, NULL, NULL, 0, &r) == 0 &&
			    r.status == 200 &&
			    chunk_unpack(p->key, r.body.p, r.body.len, j->chunk,
			    &out) == 0)
				ok = 1;
			else {
				debug("chunk %s from %s failed (HTTP %d)", hex,
				    url, r.status);
				conn_close(&c);
				if (tries)
					sleep(tries);
			}
			http_resp_free(&r);
			free(url);
		}
		if (!ok) {
			warnx_("\ncannot download chunk %s of %s", hex,
			    j->file->name);
			pthread_mutex_lock(&p->mu);
			p->failed = 1;
			pthread_mutex_unlock(&p->mu);
			break;
		}
		if ((fd = open(j->path, O_WRONLY)) == -1 ||
		    pwrite(fd, out.p, out.len, j->chunk->offset) !=
		    (ssize_t)out.len) {
			warnx_("\nwrite %s: %s", j->path, strerror(errno));
			pthread_mutex_lock(&p->mu);
			p->failed = 1;
			pthread_mutex_unlock(&p->mu);
			if (fd != -1)
				close(fd);
			break;
		}
		close(fd);
		pthread_mutex_lock(&p->mu);
		p->done_bytes += j->chunk->size;
		progress(p, 0);
		pthread_mutex_unlock(&p->mu);
	}
	conn_close(&c);
	buf_free(&out);
	return NULL;
}

/* Is the chunk already on disk with the right contents? */
static int
chunk_present(int fd, const struct chunk *c, struct buf *tmp)
{
	uint8_t h[20];
	ssize_t n;

	if (fd == -1)
		return 0;
	tmp->len = 0;
	buf_put(tmp, NULL, 0);
	if (tmp->cap < c->size + 1) {
		tmp->p = xrealloc(tmp->p, c->size + 1);
		tmp->cap = c->size + 1;
	}
	n = pread(fd, tmp->p, c->size, c->offset);
	if (n != (ssize_t)c->size)
		return 0;
	sha1(tmp->p, c->size, h);
	return memcmp(h, c->sha, 20) == 0;
}

static int
make_parent(const char *path)
{
	char *d = xstrdup(path), *s = strrchr(d, '/');
	int r = 0;

	if (s) {
		*s = '\0';
		r = mkdir_p(d, 0755);
	}
	free(d);
	return r;
}

int
depot_download(char **servers, const struct manifest *m,
    const uint8_t key[32], const char *dir, int threads)
{
	struct pool p = { 0 };
	struct buf tmp = { 0 };
	pthread_t *tid;
	size_t i, k;
	uint64_t have = 0;
	int t;

	p.depot = m->depot;
	p.servers = servers;
	p.key = key;
	pthread_mutex_init(&p.mu, NULL);
	fprintf(stderr, "  checking files...\r");
	for (i = 0; i < m->nfiles; i++) {
		const struct mfile *f = &m->files[i];
		char *path = xasprintf("%s/%s", dir, f->name);
		int fd;

		if (f->flags & DEPOT_FLAG_DIRECTORY) {
			mkdir_p(path, 0755);
			free(path);
			continue;
		}
		make_parent(path);
		if (f->link && *f->link && (f->flags & DEPOT_FLAG_SYMLINK)) {
			unlink(path);
			if (symlink(f->link, path) == -1)
				warnx_("symlink %s: %s", path, strerror(errno));
			free(path);
			continue;
		}
		fd = open(path, O_RDWR | O_CREAT, (f->flags &
		    DEPOT_FLAG_EXECUTABLE) ? 0755 : 0644);
		if (fd == -1) {
			warnx_("%s: %s", path, strerror(errno));
			free(path);
			return -1;
		}
		for (k = 0; k < f->nchunks; k++) {
			if (chunk_present(fd, &f->chunks[k], &tmp)) {
				have += f->chunks[k].size;
				continue;
			}
			p.jobs = xrealloc(p.jobs, (p.njobs + 1) * sizeof(*p.jobs));
			p.jobs[p.njobs].file = f;
			p.jobs[p.njobs].chunk = &f->chunks[k];
			p.jobs[p.njobs].path = xstrdup(path);
			p.total_bytes += f->chunks[k].size;
			p.njobs++;
		}
		if (ftruncate(fd, f->size) == -1)
			warnx_("%s: %s", path, strerror(errno));
		close(fd);
		free(path);
	}
	buf_free(&tmp);
	fprintf(stderr, "  %zu files, %.1f MB, %.1f MB already present\n",
	    m->nfiles, m->total / 1048576.0, have / 1048576.0);
	if (p.njobs == 0)
		return 0;
	p.start = p.last_print = now_sec();
	if (threads < 1)
		threads = 1;
	tid = xcalloc(threads, sizeof(*tid));
	for (t = 0; t < threads; t++)
		pthread_create(&tid[t], NULL, worker, &p);
	for (t = 0; t < threads; t++)
		pthread_join(tid[t], NULL);
	free(tid);
	progress(&p, 1);
	for (i = 0; i < p.njobs; i++)
		free(p.jobs[i].path);
	free(p.jobs);
	pthread_mutex_destroy(&p.mu);
	return p.failed ? -1 : 0;
}
