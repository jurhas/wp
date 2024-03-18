#ifndef MPARSER_H_INCLUDED
#define MPARSER_H_INCLUDED

 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
 
#define MP_UTF8 1
#define MP_UNICODE 2

/***************************/
#define MP_CODING MP_UTF8
/**************************/
#define MP_LOAD_PROPERTY 1

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#define MLONGLONGPATTERN "%lld"
#define MULONGLONGPATTERN "%llu"
#define F_PAT_TYPE (double)
#else
#define MLONGLONGPATTERN "%I64d"
#define MULONGLONGPATTERN "%I64u"
#define F_PAT_TYPE (float)
#endif

typedef long long mll;
typedef unsigned long long mull;

typedef wchar_t mu16;

typedef union _mval {
	mll ll;
	mull ull;
	double d;
	char *s;
	char sbuf[sizeof(double) / sizeof(char)];
	mu16 *ws;
	mu16 wsbuf[sizeof(double) / sizeof(mu16)];
	void *v;
} mValue;

typedef void (*mfree_f)(void *v);

typedef struct _mstack
{
	mValue *val;
	size_t sz;
	size_t n;
	mValue io_val;
} mStack;

mStack *new_mStack(size_t start_size);
void destroy_mStack(mStack *stk, mfree_f fv);
size_t mStack_push(mStack *stk);
int mStack_pop(mStack *stk);

typedef struct _mhlist
{
	struct _mhlist *next;
	mValue key;
	mValue value;
} mHList;
typedef struct _mhashtable mHashtable;
typedef size_t (*mhash_f)(mValue *);
typedef int (*mcmp_f)(mValue *a, mValue *b);

typedef enum mhsh_res_value
{
	MHSH_RUN_OUT_OF_MEM = 0,
	MHSH_OK,
	MHSH_ERR_EXISTS,
	MHSH_ERR_NOT_EXISTS

} MHSH_RES_VALUE;
typedef enum mhsh_flags
{
	MHSH_STRDUP_KEY = (1 << 0),
	MHSH_STRDUP_VALUE = (1 << 1),
	MSHS_WCSDUP_KEY = (1 << 2),
	MSHS_WCSDUP_VALUE = (1 << 3),
	MHSH_DESTROY_KEY_ON_POP = (1 << 4),
	MHSH_DESTROY_VALUE_ON_POP = (1 << 5),
	MHSH_FREE_STR_KEY_ON_POP = (1 << 6),
	MHSH_FREE_STR_VALUE_ON_POP = (1 << 7),
	MSHS_FREE_STR_KEY_ON_DESTROY = (1 << 8),
	MSHS_FREE_STR_VALUE_ON_DESTROY = (1 << 9)

} MHSH_FLAGS;
typedef struct _mhashtable
{
	mHList **tbl;
	size_t tblsz;
	mHList *lst;
	size_t lstsz;
	size_t lstn;
	mStack *free_slots;
	mhash_f hsh_f;
	mcmp_f cmp_f;
	mValue i_key;
	mValue i_val;
	mValue *o_key;
	mValue *o_val;
	mValue _private_k; //these two values contains the value when you remove an item, I have to release the list and I need some place
	mValue _private_v; //to store it. The first solution of a static variable inside the pop() function is not good, since this function is shared from all the hashtables. So if more hashtables pops consequently, their output get syncronized...yap, my first need was with a single hashtable,bad idea. If you free() their value and don't want pending addresses just memset(ht->o_key,0,sizeof(mValue)), without access the member, if you are lazy  ht->o_key->v=NULL; and if you want sleep the night comfortablu ht->o_key=NULL;   
	MHSH_RES_VALUE err_n;
	int flags;
	mfree_f fk_f;
	mfree_f fv_f;
} mHashtable;

size_t mhash_f_ull(mValue *);
size_t mhash_f_sbuf(mValue *);
size_t mhash_f_s(mValue *);

int mcmp_ull(mValue *a, mValue *b);
int mcmp_s(mValue *a, mValue *b);
int mcmp_sbuf(mValue *a, mValue *b);
int mcmp_ll(mValue *a, mValue *b);

void simplefree_v(mValue *v);

mHashtable *new_mHashtable(size_t start_size, mhash_f hsh_f, mcmp_f cmp_f);
void destroy_mHashtable(mHashtable *tbl, mfree_f fkey, mfree_f fval);

MHSH_RES_VALUE mhash_insert(mHashtable *htbl);
MHSH_RES_VALUE mhash_get(mHashtable *htbl);
MHSH_RES_VALUE mhash_pop(mHashtable *htbl);

#define mUSTRING_BUF_START_SZ 512

typedef struct _m8string
{
	char *s;
	size_t n;
	size_t sz;
} m8String;

m8String *new_m8String();
void destroy_m8String(m8String *);
char *m8s_realloc(m8String *, size_t);
char *m8s_concat(m8String *, const char *, size_t len);
char *m8s_concatc(m8String *, char);
char *m8s_concatU16c(m8String *s, mu16 c);
char *m8s_concati(m8String *, mll);
char *m8s_concatwcs(m8String *, const mu16 *);
char *m8s_strdup(m8String *);
char *m8s_replace(m8String *, char *from, char *to);
m8String *m8s_clone(m8String *);

char *m8s_rtrim(m8String *);
// be carfeul that ltrim can trigger an O(n^2) if you need an extensive use of ltrim consider to
// put a mobile start string ;
char *m8s_ltrim(m8String *);
#define m8s_trim(a) (m8s_rtrim((a)), m8s_ltrim((a)))
#define m8s_concats(a, b) (m8s_concat((a), (b), strlen(b)))
#define m8s_concat8s(dest, src) (m8s_concat((dest), (src)->s, (src)->n))
#define m8s_concatcm(a, b)                                                                                             \
	((b) == 0					  ? (a)->s                                                                             \
	 : ((a)->n + 1 + 1 < (a)->sz) ? ((a)->s[(a)->n++] = (b), (a)->s[(a)->n] = '\0', (a)->s)                            \
								  : m8s_concatc(a, b))
#define m8s_reset(a) ((a)->n = 0, *((a)->s) = '\0', (a)->s = (a)->s)

int read_utf8_seq(const char *stream, char ans[5]);
size_t strlen_mb(const char *s);
char *transliterate_diac(const char *utf8seq, char ans[5]);

typedef struct _mu16string
{
	mu16 *s;
	size_t n;
	size_t sz;
} mU16String;

mU16String *new_mU16String();
void destroy_mU16String(mU16String *);
mu16 *mU16s_realloc(mU16String *, size_t);
mu16 *mU16s_concat(mU16String *, const mu16 *, size_t);
mu16 *mU16s_concatc(mU16String *, mu16 c);
#define mU16s_concats(a, b) (mU16s_concat((a), (b), wcslen(b)))
#define mU16s_concatU16s(a, b) (mU16s_concat((a), (b)->s, (b)->n))
#define mU16s_concatcm(a, b)                                                                                           \
	((b) == 0				  ? (a)->s                                                                                 \
	 : ((a)->n + 2 < (a)->sz) ? ((a)->s[(a)->n++] = (b), (a)->s[(a)->n] = '\0', (a)->s)                                \
							  : mU16s_concatc(a, b))
#define mU16s_reset(a) ((a)->n = 0, *((a)->s) = '\0', (a)->s = (a)->s)
char *mU16s_to_m8s(mU16String *, m8String *);
mu16 *m8s_to_mU16s(m8String *, mU16String *);

#ifdef DHASH_H_INCLUDED
#define malloc(a) dmalloc(a, __FILE__, __LINE__)
#define calloc(a, b) dcalloc(a, b, __FILE__, __LINE__)
#define realloc(a, b) drealloc(a, b, __FILE__, __LINE__)
#define free(a) dfree(a)
#define strdup(a) dstrdup(a, __FILE__, __LINE__)
#define wcsdup(a) dwcsdup(a, __FILE__, __LINE__)
#endif

#if MP_CODING == MP_UTF8

typedef m8String mUString;
typedef char muchar;
typedef char mschar;
#define mUs_concat(a, b, c) m8s_concat(a, b, c)
#define mUs_concatc(a, b) m8s_concatc(a, b)
#define mUs_concatcm(a, b) m8s_concatcm(a, b)
#define mUs_concats(a, b) m8s_concats(a, b)
#define mUs_concat_U16c(a, b) m8s_concatU16c(a, b)
#define mUs_concatCurCodingS(a, b) m8s_concats(a, b)
#define mUs_realloc(a, b) m8s_realloc(a, b)
#define mUs_reset(a) m8s_reset(a)
#define ustrcmp(a, b) strcmp(a, b)
#define ustrncmp(a, b, c) strncmp(a, b, c)
#define ustrdup(a) strdup(a)
#define ustrlen(a) strlen(a)
#define ustrstr(str, substr) strstr(str, substr)
#define new_mUString() new_m8String()
#define destroy_mUString(a) destroy_m8String(a)
#define MP_STRINGPAT "s"
#define MP_EMPTY_STR ""

#elif MP_CODING == MP_UNICODE

typedef mU16String mUString;
typedef mu16 muchar;
typedef wchar_t mschar;
#define mUs_concat(a, b, c) mU16s_concat(a, b, c)
#define mUs_concatc(a, b) mU16s_concatc(a, b)
#define mUs_concatcm(a, b) mU16s_concatcm(a, b)
#define mUs_concats(a, b) mU16s_concats(a, b)
#define mUs_concat_U16c(a, b) mU16s_concatcm(a, b)
#define mUs_concatCurCodingS(a, b) m8s_concatwcs(a, b)
#define mUs_realloc(a, b) mU16s_realloc(a, b)
#define mUs_reset(a) mU16s_reset(a)
#define ustrcmp(a, b) wcscmp(a, b)
#define ustrncmp(a, b) wcsncmp(a, b, c)
#define ustrdup(a) wcsdup(a)
#define ustrlen(a) wcslen(a)

#define new_mUString() new_mU16String()
#define destroy_mUString(a) destroy_mU16String(a)
#define ustrstr(str, substr) wcsstr(str, substr)
#define MP_STRINGPAT "ls"
#define MP_EMPTY_STR L""

#else
#error Only UTF8 and UNICODE are available
#endif

typedef enum _sytype
{
	mSyNonterminal = 0,
	mSyTerminal = 1,
	mSyNoise = 2,
	mSyEOF = 3,
	mSyGroupStart = 4,
	mSyGroupEnd = 5,
	mSyDecremented = 6,
	mSyError = 7

} mSyType;
typedef struct _symbol
{
	const muchar *Name;
	mSyType Type;
} mSymbol;

typedef struct _rule
{
	short NonTerminal;
	short *symbol;
	int nsymbol;
} mRule;

typedef struct _edge
{
	short CharSetIndex;
	short TargetIndex;
} mEdge;
typedef struct _dfa_state
{
	char Accept;
	short AcceptIndex;
	mEdge *edge;
	short nedge;
} mDfa;
typedef enum _action_enum
{
	ActionShift = 1,
	ActionReduce = 2,
	ActionGoto = 3,
	ActionAccept = 4
} MP_ACTION;
typedef struct _action
{
	short SymbolIndex;
	MP_ACTION Action;
	short Target;
} mAction;
typedef struct _lalr_state
{
	mAction *action;
	short naction;
	mHashtable *h;
} mLalr;
typedef struct _group
{
	muchar *name;
	short cont_ix;
	short start_ix;
	short end_ix;
	short advance_mode;
	short end_mode;
	short nnest;
	short *nest;
} mGroup;
typedef struct _token
{
	short id;
	const muchar *lexeme;
} mToken;

typedef struct _mtree
{
	mSymbol symbol;
	mToken token;
	short state;
	short rule;
	struct _mtree **chs;
	short nchs;

} mTree;

typedef struct _mtree_data
{
	mStack *mem_chunks;
	mTree *cur_buf;
	size_t sz;
	size_t n;
} mTreeData;
typedef struct _mchset
{
	muchar *chset;
	mHashtable *h;
} mCharSet;

typedef enum _mp_ERRS
{
	MP_RUN_OUT_OF_MEMORY = 0,
	MP_OK,
	MP_CANNOT_OPEN_FILE,
	MP_ERR_READING_FILE,
	MP_NOT_VALID_GRAMMAR,
	MP_NOT_VALID_UTF8_SEQ,
	MP_NOT_VALID_CHAR,
	MP_NOT_VALID_END,
	MP_UNKNOWN_ERR
} MP_ERRS;

typedef struct _mgram
{
	short init_dfa;
	short init_lalr;
	char case_sensitive;
	short start_symbol;
	mCharSet *chset;
	short ncharset;
	mDfa *dfa;
	short ndfa;
	mSymbol *sym;
	short nsym;
	mRule *rule;
	short nrule;
	mLalr *lalr;
	short nlalr;
	mGroup *grp;
	short ngrp;
#if MP_LOAD_PROPERTY
	muchar **prop_name;
	muchar **prop_value;
	size_t n_props;
#endif
} mGram;

typedef struct _minput
{
	const muchar *exp;
	size_t exp_len;
	const muchar *cur;
	char MEOF;
	int row_n;
	const muchar *start_row_pos;
#if MP_CODING == MP_UTF8
	char out[5];
	int l_seq;
#elif MP_CODING == MP_UNICODE
	mu16 out;
#endif

} mInput;

typedef struct _mparser
{
	mGram *grm;
	mInput in;
	char reduction;
	short reduce_rule;
	short symbol;
	short lalr_state;
	mTreeData tree_data;
	mStack *tree_stack;
	mToken *tokens;
	size_t ntokens;
	size_t sztoks;
	mUString *lex;

	mTree *out_tree;
	MP_ERRS err_n;
#if MP_CODING == MP_UTF8
	char *err_msg;
#elif MP_CODING == MP_UNICODE
	wchar_t *err_msg;
#endif // MP_CODING
} mParser;

mParser *new_mParserF(const char *file_name, MP_ERRS *err_n);
mParser *new_mParserA(const char *src, size_t len, MP_ERRS *err_n);
mParser *new_mParser();
void destroy_mParser(mParser *mp);

void mpReset(mParser *mp);

#if MP_CODING == MP_UTF8
MP_ERRS mpExec(mParser *mp, const char *exp);
MP_ERRS mpExecF(mParser *mp, const char *file_name);
#elif MP_CODING == MP_UNICODE
MP_ERRS mpExec(mParser *mp, const wchar_t *exp);
#endif

#endif // MPARSER_H_INCLUDED
