/* font handling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roff.h"

/* convert wid in device unitwidth size to size sz */
#define DEVWID(sz, wid)		(((wid) * (sz) + (dev_uwid / 2)) / dev_uwid)

/* flags for gpat->flg */
#define GF_PAT		1	/* gsub/gpos pattern glyph */
#define GF_REP		2	/* gsub replacement glyph */
#define GF_CON		4	/* context glyph */
#define GF_GRP		8	/* glyph group */

/* glyph substitution and positioning rules */
struct grule {
	struct gpat {			/* rule description */
		short g;		/* glyph index */
		short flg;		/* pattern flags; GF_* */
		short x, y, xadv, yadv;	/* gpos data */
	} *pats;			/* rule pattern */
	short len;			/* pats[] length */
	short feat;			/* feature owning this rule */
};

struct font {
	char name[FNLEN];
	char fontname[FNLEN];
	int spacewid;
	int special;
	int cs, cs_ps, bd, zoom;	/* for .cs, .bd, .fzoom requests */
	int s1, n1, s2, n2;		/* for .tkf request */
	struct glyph *gl;		/* glyphs present in the font */
	int gl_n, gl_sz;		/* number of glyphs in the font */
	struct dict *gl_dict;		/* mapping from gl[i].id to i */
	struct dict *ch_dict;		/* charset mapping */
	struct dict *ch_map;		/* characters mapped via font_map() */
	/* font features */
	char feat_name[NFEATS][8];	/* feature names */
	int feat_set[NFEATS];		/* feature enabled */
	int feat_n;
	/* glyph substitution and positioning */
	struct grule *gsub;		/* glyph substitution rules */
	int gsub_n, gsub_sz;
	struct grule *gpos;		/* glyph positioning rules */
	int gpos_n, gpos_sz;
	struct iset *gsub0;		/* rules matching a glyph at pos 0 */
	struct iset *gpos0;		/* rules matching a glyph at pos 0 */
	struct iset *ggrp;		/* glyph groups */
};

/* find a glyph by its name */
struct glyph *font_find(struct font *fn, char *name)
{
	int i = dict_get(fn->ch_map, name);
	if (i == -1)
		i = dict_get(fn->ch_dict, name);
	return i >= 0 ? fn->gl + i : NULL;
}

/* find a glyph by its device-dependent identifier */
struct glyph *font_glyph(struct font *fn, char *id)
{
	int i = dict_get(fn->gl_dict, id);
	return i >= 0 ? &fn->gl[i] : NULL;
}

static int font_glyphput(struct font *fn, char *id, char *name, int type)
{
	struct glyph *g;
	if (fn->gl_n == fn->gl_sz) {
		fn->gl_sz = fn->gl_sz + 1024;
		fn->gl = mextend(fn->gl, fn->gl_n, fn->gl_sz, sizeof(fn->gl[0]));
	}
	g = &fn->gl[fn->gl_n];
	snprintf(g->id, sizeof(g->id), "%s", id);
	snprintf(g->name, sizeof(g->name), "%s", name);
	g->type = type;
	g->font = fn;
	dict_put(fn->gl_dict, g->id, fn->gl_n);
	return fn->gl_n++;
}

/* map character name to the given glyph; remove the mapping if id is NULL */
int font_map(struct font *fn, char *name, char *id)
{
	int gidx = -1;
	if (id)
		gidx = font_glyph(fn, id) ? font_glyph(fn, id) - fn->gl : -2;
	dict_put(fn->ch_map, name, gidx);
	return 0;
}

/* return nonzero if character name has been mapped with font_map() */
int font_mapped(struct font *fn, char *name)
{
	return dict_get(fn->ch_map, name) != -1;
}

/* enable/disable ligatures; first bit for liga and the second bit for rlig */
static int font_featlg(struct font *fn, int val)
{
	int ret = 0;
	ret |= font_feat(fn, "liga", val & 1);
	ret |= font_feat(fn, "rlig", val & 2) << 1;
	return ret;
}

/* enable/disable pairwise kerning */
static int font_featkn(struct font *fn, int val)
{
	return font_feat(fn, "kern", val);
}

/* glyph index in fn->glyphs[] */
static int font_idx(struct font *fn, struct glyph *g)
{
	return g ? g - fn->gl : -1;
}

static int font_gpatmatch(struct font *fn, struct gpat *p, int g)
{
	int *r;
	if (!(p->flg & GF_GRP))
		return p->g == g;
	r = iset_get(fn->ggrp, p->g);
	while (r && *r >= 0)
		if (*r++ == g)
			return 1;
	return 0;
}

static int font_rulematch(struct font *fn, struct grule *rule,
			int *src, int slen, int *dst, int dlen)
{
	int sidx = 0;		/* the index of matched glyphs in src */
	int ncon = 0;		/* number of initial context glyphs */
	struct gpat *pats = rule->pats;
	int j;
	if (!fn->feat_set[rule->feat])
		return 0;
	/* the number of initial context glyphs */
	for (j = 0; j < rule->len && pats[j].flg & GF_CON; j++)
		ncon++;
	if (dlen < ncon)
		return 0;
	/* matching the base pattern */
	for (; j < rule->len; j++) {
		if (pats[j].flg & GF_REP)
			continue;
		if (sidx >= slen || !font_gpatmatch(fn, &pats[j], src[sidx]))
			return 0;
		sidx++;
	}
	/* matching the initial context */
	for (j = 0; j < rule->len && pats[j].flg & GF_CON; j++)
		if (!font_gpatmatch(fn, &pats[j], dst[j - ncon]))
			return 0;
	return 1;
}

static int font_findrule(struct font *fn, int gsub, int pos,
		int *fwd, int fwdlen, int *ctx, int ctxlen)
{
	struct grule *rules = gsub ? fn->gsub : fn->gpos;
	int *r1 = iset_get(gsub ? fn->gsub0 : fn->gpos0, fwd[0]);
	int i = -1;
	while (r1 && r1[++i] >= 0) {
		if (r1[i] >= pos && font_rulematch(fn, &rules[r1[i]], fwd,
							fwdlen, ctx, ctxlen))
			return r1[i];
	}
	return -1;
}

/* perform all possible gpos rules on src */
static void font_performgpos(struct font *fn, int *src, int slen,
		int *x, int *y, int *xadv, int *yadv)
{
	struct grule *gpos = fn->gpos;
	int i, k;
	for (i = 0; i < slen; i++) {
		int r = font_findrule(fn, 0, 0, src + i, slen - i, src + i, i);
		if (r >= 0) {
			struct gpat *pats = gpos[r].pats;
			/* we should accumulate the values... */
			for (k = 0; k < gpos[r].len; k++) {
				x[i + k] = pats[k].x;
				y[i + k] = pats[k].y;
				xadv[i + k] = pats[k].xadv;
				yadv[i + k] = pats[k].yadv;
			}
		}
	}
}

/* find the first gsub rule after pos that matches any glyph in src */
static int font_firstgsub(struct font *fn, int pos, int *src, int slen)
{
	int best = -1;
	int i;
	for (i = 0; i < slen; i++) {
		int r = font_findrule(fn, 1, pos, src + i, slen - i, src + i, i);
		if (r >= 0 && (best < 0 || r < best))
			best = r;
	}
	return best;
}

/* apply the given gsub rule to all matches in src */
static int font_gsubapply(struct font *fn, struct grule *rule,
			int *src, int slen, int *smap)
{
	int dst[WORDLEN];
	int dlen = 0;
	int dmap[WORDLEN];
	int i, j;
	memset(dmap, 0, slen * sizeof(dmap[i]));
	for (i = 0; i < slen; i++) {
		dmap[dlen] = smap[i];
		if (font_rulematch(fn, rule, src + i, slen - i,
					dst + dlen, dlen)) {
			for (j = 0; j < rule->len; j++) {
				if (rule->pats[j].flg & GF_REP)
					dst[dlen++] = rule->pats[j].g;
				if (rule->pats[j].flg & GF_PAT)
					i++;
			}
			i--;
		} else {
			dst[dlen++] = src[i];
		}
	}
	memcpy(src, dst, dlen * sizeof(dst[0]));
	memcpy(smap, dmap, dlen * sizeof(dmap[0]));
	return dlen;
}

/* perform all possible gsub rules on src */
static int font_performgsub(struct font *fn, int *src, int slen, int *smap)
{
	int i = -1;
	while (++i >= 0) {
		if ((i = font_firstgsub(fn, i, src, slen)) < 0)
			break;
		slen = font_gsubapply(fn, &fn->gsub[i], src, slen, smap);
	}
	return slen;
}

int font_layout(struct font *fn, struct glyph **gsrc, int nsrc, int sz,
		struct glyph **gdst, int *dmap,
		int *x, int *y, int *xadv, int *yadv, int lg, int kn)
{
	int dst[WORDLEN];
	int ndst = nsrc;
	int i;
	int featlg, featkn;
	/* initialising dst */
	for (i = 0; i < nsrc; i++)
		dst[i] = font_idx(fn, gsrc[i]);
	for (i = 0; i < ndst; i++)
		dmap[i] = i;
	memset(x, 0, ndst * sizeof(x[0]));
	memset(y, 0, ndst * sizeof(y[0]));
	memset(xadv, 0, ndst * sizeof(xadv[0]));
	memset(yadv, 0, ndst * sizeof(yadv[0]));
	/* substitution rules */
	if (lg)
		featlg = font_featlg(fn, 3);
	ndst = font_performgsub(fn, dst, ndst, dmap);
	if (lg)
		font_featlg(fn, featlg);
	/* positioning rules */
	if (kn)
		featkn = font_featkn(fn, 1);
	font_performgpos(fn, dst, ndst, x, y, xadv, yadv);
	if (kn)
		font_featkn(fn, featkn);
	for (i = 0; i < ndst; i++)
		gdst[i] = fn->gl + dst[i];
	/* cursive attachment */
	for (i = ndst - 2; i >= 0; i--) {
		if (gdst[i]->ocur && gdst[i + 1]->icur) {
			x[i] = x[i + 1];
			y[i] = y[i + 1];
			x[i] += gdst[i + 1]->icx - gdst[i]->ocx + gdst[i]->wid;
			if (gdst[i]->ocx < gdst[i]->wid / 2)	/* r2l probably */
				x[i] += -gdst[i]->wid - gdst[i + 1]->wid;
			y[i] -= gdst[i + 1]->icy - gdst[i]->ocy;
		}
	}
	return ndst;
}

static int font_readchar(struct font *fn, FILE *fin, int *n, int *gid)
{
	struct glyph *g;
	char tok[ILNLEN];
	char name[ILNLEN];
	char id[ILNLEN];
	int type;
	if (fscanf(fin, "%s %s", name, tok) != 2)
		return 1;
	if (!strcmp("---", name))
		sprintf(name, "c%04d", *n);
	if (strcmp("\"", tok)) {
		if (fscanf(fin, "%d %s", &type, id) != 2)
			return 1;
		*gid = dict_get(fn->gl_dict, id);
		if (*gid < 0) {
			*gid = font_glyphput(fn, id, name, type);
			g = &fn->gl[*gid];
			sscanf(tok, "%hd,%hd,%hd,%hd,%hd", &g->wid,
				&g->llx, &g->lly, &g->urx, &g->ury);
		}
	}
	dict_put(fn->ch_dict, name, *gid);
	(*n)++;
	return 0;
}

static int font_findfeat(struct font *fn, char *feat, int mk)
{
	int i;
	for (i = 0; i < fn->feat_n; i++)
		if (!strcmp(feat, fn->feat_name[i]))
			return i;
	if (mk) {
		snprintf(fn->feat_name[fn->feat_n],
			sizeof(fn->feat_name[fn->feat_n]), "%s", feat);
	}
	return mk ? fn->feat_n++ : -1;
}

static struct gpat *font_gpat(struct font *fn, int len)
{
	struct gpat *pats = xmalloc(len * sizeof(pats[0]));
	memset(pats, 0, len * sizeof(pats[0]));
	return pats;
}

static struct grule *font_gsub(struct font *fn, char *feat, int len)
{
	struct grule *rule;
	struct gpat *pats = font_gpat(fn, len);
	if (fn->gsub_n  == fn->gsub_sz) {
		fn->gsub_sz = fn->gsub_sz + 1024;
		fn->gsub = mextend(fn->gsub, fn->gsub_n, fn->gsub_sz,
				sizeof(fn->gsub[0]));
	}
	rule = &fn->gsub[fn->gsub_n++];
	rule->pats = pats;
	rule->len = len;
	rule->feat = font_findfeat(fn, feat, 1);
	return rule;
}

static struct grule *font_gpos(struct font *fn, char *feat, int len)
{
	struct grule *rule;
	struct gpat *pats = font_gpat(fn, len);
	if (fn->gpos_n == fn->gpos_sz) {
		fn->gpos_sz = fn->gpos_sz + 1024;
		fn->gpos = mextend(fn->gpos, fn->gpos_n, fn->gpos_sz,
				sizeof(fn->gpos[0]));
	}
	rule = &fn->gpos[fn->gpos_n++];
	rule->pats = pats;
	rule->len = len;
	rule->feat = font_findfeat(fn, feat, 1);
	return rule;
}

static int font_readgpat(struct font *fn, struct gpat *p, char *s)
{
	if (s[0] == '@') {
		p->g = atoi(s + 1);
		if (iset_len(fn->ggrp, p->g) == 1)
			p->g = iset_get(fn->ggrp, p->g)[0];
		else
			p->flg |= GF_GRP;
	} else {
		p->g = font_idx(fn, font_glyph(fn, s));
	}
	return p->g < 0;
}

static int font_readgsub(struct font *fn, FILE *fin)
{
	char tok[128];
	struct grule *rule;
	int i, n;
	if (fscanf(fin, "%s %d", tok, &n) != 2)
		return 1;
	rule = font_gsub(fn, tok, n);
	for (i = 0; i < n; i++) {
		if (fscanf(fin, "%s", tok) != 1)
			return 1;
		if (tok[0] == '-')
			rule->pats[i].flg = GF_PAT;
		if (tok[0] == '=')
			rule->pats[i].flg = GF_CON;
		if (tok[0] == '+')
			rule->pats[i].flg = GF_REP;
		if (!tok[0] || font_readgpat(fn, &rule->pats[i], tok + 1))
			return 0;
	}
	return 0;
}

static int font_readgpos(struct font *fn, FILE *fin)
{
	char tok[128];
	char *col;
	struct grule *rule;
	int i, n;
	if (fscanf(fin, "%s %d", tok, &n) != 2)
		return 1;
	rule = font_gpos(fn, tok, n);
	for (i = 0; i < n; i++) {
		if (fscanf(fin, "%s", tok) != 1)
			return 1;
		col = strchr(tok, ':');
		if (col)
			*col = '\0';
		rule->pats[i].flg = GF_PAT;
		if (!tok[0] || font_readgpat(fn, &rule->pats[i], tok))
			return 0;
		if (col)
			sscanf(col + 1, "%hd%hd%hd%hd",
				&rule->pats[i].x, &rule->pats[i].y,
				&rule->pats[i].xadv, &rule->pats[i].yadv);
	}
	return 0;
}

static int font_readggrp(struct font *fn, FILE *fin)
{
	char tok[ILNLEN];
	int id, n, i, g;
	if (fscanf(fin, "%d %d", &id, &n) != 2)
		return 1;
	for (i = 0; i < n; i++) {
		if (fscanf(fin, "%s", tok) != 1)
			return 1;
		g = font_idx(fn, font_glyph(fn, tok));
		if (g >= 0)
			iset_put(fn->ggrp, id, g);
	}
	return 0;
}

static int font_readgcur(struct font *fn, FILE *fin)
{
	char feat[8];
	char name[GNLEN];
	struct glyph *g;
	char ix[16], iy[16], ox[16], oy[16];
	if (fscanf(fin, "%s %s %s %s %s %s", feat, name, ix, iy, ox, oy) != 6)
		return 1;
	if (!(g = font_glyph(fn, name)))
		return 0;
	g->icur = !!strcmp("-", ix);
	g->ocur = !!strcmp("-", ox);
	g->icx = atoi(ix);
	g->icy = atoi(iy);
	g->ocx = atoi(ox);
	g->ocy = atoi(oy);
	return 0;
}

static int font_readkern(struct font *fn, FILE *fin)
{
	char c1[ILNLEN], c2[ILNLEN];
	struct grule *rule;
	int val;
	if (fscanf(fin, "%s %s %d", c1, c2, &val) != 3)
		return 1;
	rule = font_gpos(fn, "kern", 2);
	rule->pats[0].g = font_idx(fn, font_glyph(fn, c1));
	rule->pats[1].g = font_idx(fn, font_glyph(fn, c2));
	rule->pats[0].xadv = val;
	rule->pats[0].flg = GF_PAT;
	rule->pats[1].flg = GF_PAT;
	return 0;
}

static void font_lig(struct font *fn, char *lig)
{
	char c[GNLEN];
	int g[WORDLEN];
	struct grule *rule;
	char *s = lig;
	int j, n = 0;
	while (utf8read(&s, c) > 0)
		g[n++] = font_idx(fn, font_find(fn, c));
	rule = font_gsub(fn, "liga", n + 1);
	for (j = 0; j < n; j++) {
		rule->pats[j].g = g[j];
		rule->pats[j].flg = GF_PAT;
	}
	rule->pats[n].g = font_idx(fn, font_find(fn, lig));
	rule->pats[n].flg = GF_REP;
}

static void skipline(FILE* filp)
{
	int c;
	do {
		c = getc(filp);
	} while (c != '\n' && c != EOF);
}

static struct gpat *font_rulefirstpat(struct font *fn, struct grule *rule)
{
	int i;
	for (i = 0; i < rule->len; i++)
		if (!(rule->pats[i].flg & (GF_REP | GF_CON)))
			return &rule->pats[i];
	return NULL;
}

static void font_isetinsert(struct font *fn, struct iset *iset, int rule, struct gpat *p)
{
	if (p->flg & GF_GRP) {
		int *r = iset_get(fn->ggrp, p->g);
		while (r && *r >= 0)
			iset_put(iset, *r++, rule);
	} else {
		if (p->g >= 0)
			iset_put(iset, p->g, rule);
	}
}

struct font *font_open(char *path)
{
	struct font *fn;
	int ch_g = -1;		/* last glyph in the charset */
	int ch_n = 0;			/* number of glyphs in the charset */
	char tok[ILNLEN];
	FILE *fin;
	char ligs[512][GNLEN];
	int ligs_n = 0;
	int i;
	fin = fopen(path, "r");
	if (!fin)
		return NULL;
	fn = xmalloc(sizeof(*fn));
	if (!fn) {
		fclose(fin);
		return NULL;
	}
	memset(fn, 0, sizeof(*fn));
	fn->gl_dict = dict_make(-1, 1, 0);
	fn->ch_dict = dict_make(-1, 1, 0);
	fn->ch_map = dict_make(-1, 1, 0);
	fn->ggrp = iset_make();
	while (fscanf(fin, "%s", tok) == 1) {
		if (!strcmp("char", tok)) {
			font_readchar(fn, fin, &ch_n, &ch_g);
		} else if (!strcmp("kern", tok)) {
			font_readkern(fn, fin);
		} else if (!strcmp("ligatures", tok)) {
			while (fscanf(fin, "%s", ligs[ligs_n]) == 1) {
				if (!strcmp("0", ligs[ligs_n]))
					break;
				if (ligs_n < LEN(ligs))
					ligs_n++;
			}
		} else if (!strcmp("gsub", tok)) {
			font_readgsub(fn, fin);
		} else if (!strcmp("gpos", tok)) {
			font_readgpos(fn, fin);
		} else if (!strcmp("ggrp", tok)) {
			font_readggrp(fn, fin);
		} else if (!strcmp("gcur", tok)) {
			font_readgcur(fn, fin);
		} else if (!strcmp("spacewidth", tok)) {
			fscanf(fin, "%d", &fn->spacewid);
		} else if (!strcmp("special", tok)) {
			fn->special = 1;
		} else if (!strcmp("name", tok)) {
			fscanf(fin, "%s", fn->name);
		} else if (!strcmp("fontname", tok)) {
			fscanf(fin, "%s", fn->fontname);
		} else if (!strcmp("charset", tok)) {
			while (!font_readchar(fn, fin, &ch_n, &ch_g))
				;
			break;
		}
		skipline(fin);
	}
	for (i = 0; i < ligs_n; i++)
		font_lig(fn, ligs[i]);
	fclose(fin);
	fn->gsub0 = iset_make();
	fn->gpos0 = iset_make();
	for (i = 0; i < fn->gsub_n; i++)
		font_isetinsert(fn, fn->gsub0, i,
			font_rulefirstpat(fn, &fn->gsub[i]));
	for (i = 0; i < fn->gpos_n; i++)
		font_isetinsert(fn, fn->gpos0, i,
			font_rulefirstpat(fn, &fn->gpos[i]));
	return fn;
}

void font_close(struct font *fn)
{
	int i;
	for (i = 0; i < fn->gsub_n; i++)
		free(fn->gsub[i].pats);
	for (i = 0; i < fn->gpos_n; i++)
		free(fn->gpos[i].pats);
	dict_free(fn->gl_dict);
	dict_free(fn->ch_dict);
	dict_free(fn->ch_map);
	iset_free(fn->gsub0);
	iset_free(fn->gpos0);
	iset_free(fn->ggrp);
	free(fn->gsub);
	free(fn->gpos);
	free(fn->gl);
	free(fn);
}

int font_special(struct font *fn)
{
	return fn->special;
}

/* return width w for the given font and size */
int font_wid(struct font *fn, int sz, int w)
{
	sz = font_zoom(fn, sz);
	return w >= 0 ? DEVWID(sz, w) : -DEVWID(sz, -w);
}

/* return track kerning width for the given size */
static int font_twid(struct font *fn, int sz)
{
	if (fn->s1 >= 0 && sz <= fn->s1)
		return fn->n1 * SC_PT;
	if (fn->s2 >= 0 && sz >= fn->s2)
		return fn->n2 * SC_PT;
	if (sz > fn->s1 && sz < fn->s2)
		return ((sz - fn->s1) * fn->n1 + (fn->s2 - sz) * fn->n2) *
				(long) SC_PT / (fn->s2 - fn->s1);
	return 0;
}

/* glyph width, where cfn is the current font and fn is glyph's font */
int font_gwid(struct font *fn, struct font *cfn, int sz, int w)
{
	struct font *xfn = cfn ? cfn : fn;
	if (xfn->cs)
		return xfn->cs * (font_zoom(fn, xfn->cs_ps ? xfn->cs_ps : sz)
					* SC_IN / 72) / 36;
	return font_wid(fn, sz, w) + (cfn ? font_twid(fn, sz) : 0) +
		(font_getbd(xfn) ? font_getbd(xfn) - 1 : 0);
}

/* space width for the give word space or sentence space */
int font_swid(struct font *fn, int sz, int ss)
{
	return font_gwid(fn, NULL, sz, (fn->spacewid * ss + 6) / 12);
}

int font_getcs(struct font *fn)
{
	return fn->cs;
}

void font_setcs(struct font *fn, int cs, int ps)
{
	fn->cs = cs;
	fn->cs_ps = ps;
}

int font_getbd(struct font *fn)
{
	return fn->bd;
}

void font_setbd(struct font *fn, int bd)
{
	fn->bd = bd;
}

void font_track(struct font *fn, int s1, int n1, int s2, int n2)
{
	fn->s1 = s1;
	fn->n1 = n1;
	fn->s2 = s2;
	fn->n2 = n2;
}

int font_zoom(struct font *fn, int sz)
{
	return fn->zoom ? (sz * fn->zoom + 500) / 1000 : sz;
}

void font_setzoom(struct font *fn, int zoom)
{
	fn->zoom = zoom;
}

/* enable/disable font features; returns the previous value */
int font_feat(struct font *fn, char *name, int val)
{
	int idx = font_findfeat(fn, name, 0);
	int old = idx >= 0 ? fn->feat_set[idx] : 0;
	if (idx >= 0)
		fn->feat_set[idx] = val != 0;
	return old;
}
