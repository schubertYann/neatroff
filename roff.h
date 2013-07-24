/* converting scales */
#define SC_IN		(dev_res)	/* inch in units */
#define SC_PT		(SC_IN / 72)	/* point in units */
#define SC_EM		(n_s * SC_IN / 72)
#define SC_DW		(SC_EM / 3)	/* default width */
#define SC_HT		(n_s * SC_PT)	/* character height */

/* predefined array limits */
#define PATHLEN		1024	/* path length */
#define NFILES		16	/* number of input files */
#define NFONTS		32	/* number of fonts */
#define NLIGS		32	/* number of font ligatures */
#define NKERNS		128	/* number of font pairwise kerning pairs */
#define FNLEN		32	/* font name length */
#define NGLYPHS		512	/* glyphs in fonts */
#define NMLEN		32	/* macro/register/environment/glyph name length */
#define GNLEN		NMLEN	/* glyph name length */
#define RNLEN		NMLEN	/* register/macro name */
#define ILNLEN		1000	/* line limit of input files */
#define LNLEN		4000	/* line buffer length (ren.c/out.c) */
#define NWORDS		256	/* number of words in line buffer */
#define NARGS		9	/* number of macro arguments */
#define NPREV		16	/* environment stack depth */
#define NTRAPS		1024	/* number of traps per page */
#define NIES		128	/* number of nested .ie commands */
#define NTABS		16	/* number of tab stops */
#define NFIELDS		32	/* number of fields */
#define MAXFRAC		100000	/* maximum value of the fractional part */

/* escape sequences */
#define ESC_Q	"bCDhHlLNoSvwxX"	/* \X'ccc' quoted escape sequences */
#define ESC_P	"*fgkmns"		/* \Xc \X(cc \X[ccc] escape sequences */

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))
#define LEN(a)		(sizeof(a) / sizeof((a)[0]))

/* special characters */
extern int c_ec;	/* escape character (\) */
extern int c_cc;	/* basic control character (.) */
extern int c_c2;	/* no-break control character (') */
#define c_ni	4	/* non-interpreted copy mode escape */
#define c_hc	env_hc()/* hyphenation character */

/* number registers */
int num_get(int id, int inc);
void num_set(int id, int val);
void num_inc(int id, int val);
void num_del(int id);
char *num_str(int id);
char *num_getfmt(int id);
void num_setfmt(int id, char *fmt);
int *nreg(int id);
int eval(char *s, int unit);
int eval_up(char **s, int unit);
int eval_re(char *s, int orig, int unit);

/* string registers */
void str_set(int id, char *s);
void str_dset(int id, void *d);
char *str_get(int id);
void *str_dget(int id);
void str_rm(int id);
void str_rn(int src, int dst);

/* saving and restoring registers before and after printing diverted lines */
void odiv_beg(void);
void odiv_end(void);

/* enviroments */
void env_init(void);
void env_done(void);
struct adj *env_adj(void);
char *env_hc(void);
int tab_next(int pos);

/* device related variables */
extern int dev_res;
extern int dev_uwid;
extern int dev_hor;
extern int dev_ver;

struct glyph {
	char name[GNLEN];	/* name of the glyph */
	char id[GNLEN];		/* device-dependent glyph identifier */
	struct font *font;	/* glyph font */
	int wid;		/* character width */
	int type;		/* character type; ascender/descender */
};

struct font {
	char name[FNLEN];
	char fontname[FNLEN];
	struct glyph glyphs[NGLYPHS];
	int nglyphs;
	int spacewid;
	int special;
	char c[NGLYPHS][GNLEN];		/* character names in charset */
	struct glyph *g[NGLYPHS];	/* character glyphs in charset */
	int n;				/* number of characters in charset */
	char lig[NLIGS][GNLEN * 4];	/* font ligatures */
	int nlig;
	int kern[NKERNS];		/* font pairwise kerning */
	char kern_c1[NKERNS][GNLEN];
	char kern_c2[NKERNS][GNLEN];
	int nkern;
};

/* output device functions */
int dev_open(char *path);
void dev_close(void);
int dev_mnt(int pos, char *id, char *name);
int dev_pos(char *id);
struct font *dev_font(int pos);
int charwid(int wid, int sz);

/* font-related functions */
struct font *font_open(char *path);
void font_close(struct font *fn);
struct glyph *font_glyph(struct font *fn, char *id);
struct glyph *font_find(struct font *fn, char *name);
int font_lig(struct font *fn, char *c);
int font_kern(struct font *fn, char *c1, char *c2);

/* glyph handling functions */
struct glyph *dev_glyph(char *c, int fn);
struct glyph *dev_glyph_byid(char *id, int fn);
int dev_spacewid(void);

/* different layers of neatroff */
int in_next(void);		/* input layer */
int cp_next(void);		/* copy-mode layer */
int tr_next(void);		/* troff layer */

void in_push(char *s, char **args);
void in_pushnl(char *s, char **args);
void in_so(char *path);		/* .so request */
void in_nx(char *path);		/* .nx request */
void in_ex(void);		/* .nx request */
void in_queue(char *path);	/* .ex request */
char *in_arg(int i);		/* look up argument */
int in_nargs(void);		/* number of arguments */
void in_back(int c);		/* push back input character */
int in_top(void);		/* the first pushed-back character */
char *in_filename(void);	/* current filename */

void cp_blk(int skip);		/* skip or read the next line or block */
void cp_wid(int enable);	/* control inlining \w requests */
#define cp_back		in_back	/* cp.c is stateless */
void tr_first(void);		/* read until the first non-command line */

/* variable length string buffer */
struct sbuf {
	char *s;		/* allocated buffer */
	int sz;			/* buffer size */
	int n;			/* length of the string stored in s */
	int prev_n;		/* n before the last sbuf_append() */
};

void sbuf_init(struct sbuf *sbuf);
void sbuf_done(struct sbuf *sbuf);
char *sbuf_buf(struct sbuf *sbuf);
void sbuf_add(struct sbuf *sbuf, int c);
void sbuf_append(struct sbuf *sbuf, char *s);
void sbuf_printf(struct sbuf *sbuf, char *s, ...);
void sbuf_putnl(struct sbuf *sbuf);
void sbuf_pop(struct sbuf *sbuf);
int sbuf_len(struct sbuf *sbuf);
int sbuf_empty(struct sbuf *sbuf);

/* word buffer */
struct wb {
	struct sbuf sbuf;
	int f, s, m;		/* the last output font and size */
	int r_f, r_s, r_m;	/* current font and size; use n_f and n_s if -1 */
	int part;		/* partial input (\c) */
	int els_neg, els_pos;	/* extra line spacing */
	int h, v;		/* buffer vertical and horizontal positions */
	int ct, sb, st;		/* \w registers */
	char prev_c[GNLEN];	/* previous character added via wb_put() */
	int prev_h;		/* wb->h after wb_put() calls */
	int prev_l;		/* sbuf_len(&wb->sbuf) after wb_put() calls */
};

void wb_init(struct wb *wb);
void wb_done(struct wb *wb);
void wb_hmov(struct wb *wb, int n);
void wb_vmov(struct wb *wb, int n);
void wb_els(struct wb *wb, int els);
void wb_etc(struct wb *wb, char *x);
void wb_put(struct wb *wb, char *c);
int wb_part(struct wb *wb);
void wb_setpart(struct wb *wb);
void wb_drawl(struct wb *wb, int h, int v);
void wb_drawc(struct wb *wb, int r);
void wb_drawe(struct wb *wb, int h, int v);
void wb_drawa(struct wb *wb, int h1, int v1, int h2, int v2);
void wb_drawxbeg(struct wb *wb, int c);
void wb_drawxdot(struct wb *wb, int h, int v);
void wb_drawxend(struct wb *wb);
void wb_cat(struct wb *wb, struct wb *src);
int wb_hyph(struct wb *wb, int w, struct wb *w1, struct wb *w2, int flg);
int wb_wid(struct wb *wb);
int wb_empty(struct wb *wb);
void wb_wconf(struct wb *wb, int *ct, int *st, int *sb);
int wb_lig(struct wb *wb, char *c);
int wb_kern(struct wb *wb, char *c);

/* hyphenation flags */
#define HY_MASK		0x0f	/* enable hyphenation */
#define HY_LAST		0x02	/* do not hyphenate last lines */
#define HY_FINAL	0x04	/* do not hyphenate the final character */
#define HY_FIRSTTWO	0x08	/* do not hyphenate the first two characters */
#define HY_ANY		0x10	/* break at any possible position */

void hyphenate(char *hyphs, char *word);

/* adjustment */
#define AD_L		0
#define AD_B		1
#define AD_C		3
#define AD_R		5

struct adj *adj_alloc(void);
void adj_free(struct adj *adj);
int adj_fill(struct adj *adj, int ad_b, int fill, int hyph, struct sbuf *dst,
		int *ll, int *in, int *ti, int *els_neg, int *els_pos);
int adj_full(struct adj *adj, int fill);
int adj_empty(struct adj *adj, int fill);
int adj_wid(struct adj *adj);
void adj_swid(struct adj *adj, int swid);
void adj_ll(struct adj *adj, int ll);
void adj_in(struct adj *adj, int in);
void adj_ti(struct adj *adj, int ti);
void adj_wb(struct adj *adj, struct wb *wb);
void adj_nl(struct adj *adj);
void adj_sp(struct adj *adj);
void adj_nonl(struct adj *adj);

/* rendering */
int render(void);				/* the main loop */
void ren_char(struct wb *wb, int (*next)(void), void (*back)(int));
int ren_wid(int (*next)(void), void (*back)(int));
void ren_tl(int (*next)(void), void (*back)(int));
void ren_hline(struct wb *wb, char *arg);	/* horizontal line */
void ren_vline(struct wb *wb, char *arg);	/* vertical line */
void ren_bracket(struct wb *wb, char *arg);	/* \b */
void ren_over(struct wb *wb, char *arg);	/* \o */
void ren_draw(struct wb *wb, char *arg);	/* \D */

/* out.c */
void out_line(char *s);				/* output rendered line */
int out_readc(char **s, char *d);		/* read request or glyph */
void out(char *s, ...);				/* output troff cmd */

/* troff commands */
void tr_ab(char **args);
void tr_bp(char **args);
void tr_br(char **args);
void tr_ce(char **args);
void tr_ch(char **args);
void tr_di(char **args);
void tr_divbeg(char **args);
void tr_divend(char **args);
void tr_dt(char **args);
void tr_em(char **args);
void tr_ev(char **args);
void tr_fc(char **args);
void tr_fi(char **args);
void tr_fp(char **args);
void tr_ft(char **args);
void tr_hw(char **args);
void tr_in(char **args);
void tr_ll(char **args);
void tr_mk(char **args);
void tr_ne(char **args);
void tr_nf(char **args);
void tr_ns(char **args);
void tr_os(char **args);
void tr_pn(char **args);
void tr_ps(char **args);
void tr_rs(char **args);
void tr_rt(char **args);
void tr_sp(char **args);
void tr_sv(char **args);
void tr_ta(char **args);
void tr_ti(char **args);
void tr_wh(char **args);
void tr_eject(char **args);

void tr_init(void);

/* helpers */
void errmsg(char *msg, ...);
int utf8len(int c);
int utf8read(char **s, char *d);
void schar_read(char *d, int (*next)(void));
int schar_jump(char *d, int (*next)(void), void (*back)(int));

/* internal commands */
#define TR_DIVBEG	"\07<"	/* diversion begins */
#define TR_DIVEND	"\07>"	/* diversion ends */
#define TR_EJECT	"\07P"	/* page eject */

/* mapping register, macro and environment names to numbers */
#define NREGS		(1 << 16)
#define NREGS2		(NREGS * 2)
#define REG(c1, c2)	((c1) * 256 + (c2))

int map(char *s);
char *map_name(int id);

/* colors */
#define CLR_R(c)		(((c) >> 16) & 0xff)
#define CLR_G(c)		(((c) >> 8) & 0xff)
#define CLR_B(c)		((c) & 0xff)
#define CLR_RGB(r, g, b)	(((r) << 16) | ((g) << 8) | (b))

char *clr_str(int c);
int clr_get(char *s);

/* builtin number registers; n_X for .X register */
#define n_a		(*nreg(REG('.', 'a')))
#define n_cp		(*nreg(REG('.', 'C')))
#define n_d		(*nreg(REG('.', 'd')))
#define n_f		(*nreg(REG('.', 'f')))
#define n_h		(*nreg(REG('.', 'h')))
#define n_i		(*nreg(REG('.', 'i')))
#define n_j		(*nreg(REG('.', 'j')))
#define n_l		(*nreg(REG('.', 'l')))
#define n_L		(*nreg(REG('.', 'L')))
#define n_n		(*nreg(REG('.', 'n')))
#define n_m		(*nreg(REG('.', 'm')))
#define n_o		(*nreg(REG('.', 'o')))
#define n_p		(*nreg(REG('.', 'p')))
#define n_s		(*nreg(REG('.', 's')))
#define n_u		(*nreg(REG('.', 'u')))
#define n_v		(*nreg(REG('.', 'v')))
#define n_ct		(*nreg(REG('c', 't')))
#define n_dl		(*nreg(REG('d', 'l')))
#define n_dn		(*nreg(REG('d', 'n')))
#define n_nl		(*nreg(REG('n', 'l')))
#define n_sb		(*nreg(REG('s', 'b')))
#define n_st		(*nreg(REG('s', 't')))
#define n_pg		(*nreg(REG('%', '\0')))	/* % */
#define n_lb		(*nreg(REG(0, 'b')))	/* input line beg */
#define n_ce		(*nreg(REG(0, 'c')))	/* .ce remaining */
#define n_f0		(*nreg(REG(0, 'f')))	/* last .f */
#define n_lg		(*nreg(REG(0, 'g')))	/* .lg mode */
#define n_hy		(*nreg(REG(0, 'h')))	/* .hy mode */
#define n_i0		(*nreg(REG(0, 'i')))	/* last .i */
#define n_kn		(*nreg(REG(0, 'k')))	/* .kn mode */
#define n_l0		(*nreg(REG(0, 'l')))	/* last .l */
#define n_L0		(*nreg(REG(0, 'L')))	/* last .L */
#define n_m0		(*nreg(REG(0, 'm')))	/* last .m */
#define n_mk		(*nreg(REG(0, 'M')))	/* .mk internal register */
#define n_na		(*nreg(REG(0, 'n')))	/* .na mode */
#define n_ns		(*nreg(REG(0, 'N')))	/* .ns mode */
#define n_o0		(*nreg(REG(0, 'o')))	/* last .o */
#define n_s0		(*nreg(REG(0, 's')))	/* last .s */
#define n_sv		(*nreg(REG(0, 'S')))	/* .sv value */
#define n_lt		(*nreg(REG(0, 't')))	/* .lt value */
#define n_t0		(*nreg(REG(0, 'T')))	/* previous .lt value */
#define n_v0		(*nreg(REG(0, 'v')))	/* last .v */

/* functions for implementing read-only registers */
int f_nexttrap(void);	/* .t */
int f_divreg(void);	/* .z */
int f_hpos(void);	/* .k */
