#line 2 "mparser.c"
/// This program is distribuited under the licence GPL-3.0
/// I have not so much requirement, just alert me if you find
/// some bugs: spanu_andrea(at)yahoo.it
/// ah I am a man, I am italian do not send me nudes please, unless you are a girl of course.

#include "mparser.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

#define MP_SWITCH_TO_HASHTABLE_THRESHOLD 5
#define MP_TREE_MEM_CHUNK_SZ 100
#define MP_TOKEN_BUF_SZ 32

static muchar _err_buf[1024];
//clang-format off
#define MP_RAISE_ERR(a, numb, msg, retval)		\
	do						\
	{						\
		(a)->err_n = numb;			\
		(a)->err_msg = (mschar *)(msg);		\
		return retval;				\
	} while ((a)->err_n & 0x3B7)
//clang-format on
#if MP_CODING == MP_UTF8
char *_str[] = {
	"ERR:Run Out Of Memory",
	"ERR:Not valid UTF8 sequence. Probably wrong coding or input damaged",
	"ERR:Unexpected char:\"%.10s\"  at line:%d col:%d",
	"ERR:Unexpected Token:\"%.10s\"  at line:%d col:%d",
	"ERR:Unexpected end of the expression. String not closed, bracket not closed or missing operator",
	"ERR:Unknown error n:%d",
	"\n",
	"NewLine",
};
#elif MP_CODING == MP_UNICODE
wchar_t *_str[] = {
	L"ERR:Run Out Of Memory",
	L"ERR:Not valid UTF8 sequence. Probably wrong coding or input damaged",
	L"ERR:Unexpected char:\"%.10ls\"  at line:%d col:%d",
	L"ERR:Unexpected Token:\"%.10ls\"  at line:%d col:%d",
	L"ERR:Unexpected end of the expression. String not closed, bracket not closed or missing operator",
	L"ERR:Unknown error n:%d",
	L"\n",
	L"NewLine",
};
#endif // MP_CODING
#define MP_ERR_MSG_ROOM _str[0]
#define MP_ERR_MSG_UTF8_SEQ_NOT_VALID _str[1]
#define MP_ERR_MSG_UNEXPECTED_CHAR _str[2]
#define MP_ERR_MSG_UNEXPECTED_TK _str[3]
#define MP_ERR_MSG_UNEXPECTED_END _str[4]
#define MP_ERR_MSG_UNKNOWN_ERR _str[5]
#define MP_STR_N _str[6]
#define MP_STR_NewLine _str[7]

static int mparser_parse(mParser *mp);

void simplefree_v(mValue *v)
{
	if (v && v->v)
		free(v->v);
}

mStack *new_mStack(size_t start_size)
{
	mStack *res = malloc(sizeof(mStack));
	if (!res)
		goto ERR1;
	res->sz = start_size < 10 ? 10 : start_size;
	res->n = 0;
	res->val = calloc(res->sz, sizeof(mValue));
	if (!res->val)
		goto ERR2;
	return res;
ERR2:
	free(res);
ERR1:
	return NULL;
}
void destroy_mStack(mStack *stk, mfree_f fv)
{
	if (fv)
		while (stk->n--)
			(*fv)(stk->val[stk->n].v);
	free(stk->val);
	free(stk);
}
size_t mStack_push(mStack *stk)
{
	if (stk->n >= stk->sz)
	{
		size_t tmp_sz = stk->sz << 1;
		mValue *tmp = realloc(stk->val, sizeof(mValue) * tmp_sz);
		if (!tmp)
			return 0;
		assert(stk->n < tmp_sz);
		memset(tmp + stk->n, 0, (tmp_sz - stk->n) * sizeof(mValue));
		stk->sz = tmp_sz;
		stk->val = tmp;
	}
	stk->val[stk->n] = stk->io_val;
	return ++stk->n;
}
int mStack_pop(mStack *stk)
{

	if (stk->n == 0)
		return 0;
	stk->io_val = stk->val[--stk->n];
	memset(stk->val + stk->n, 0, sizeof(mValue));
	return 1;
}
size_t mhash_f_ull(mValue *v)
{
	return v->ull;
}
size_t mhash_f_sbuf(mValue *v)
{
	unsigned char *cur = (unsigned char *)v->sbuf;
	int i = sizeof(v->sbuf) - 1;
	size_t hash = 5381;
	int c;
	while ((c = *cur++) && i--)
		hash = ((hash << 5) + hash) + c;

	return hash;
}
size_t mhash_f_s(mValue *v)
{

	unsigned char *cur = (unsigned char *)v->s;
	size_t hash = 5381;
	int c;
	while ((c = *cur++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}
mHashtable *new_mHashtable(size_t start_size, mhash_f hsh_f, mcmp_f cmp_f)
{
	assert(hsh_f && cmp_f);
	mHashtable *res = malloc(sizeof(mHashtable));
	if (!res)
		goto ERR1;
	start_size = (start_size < 16 ? 16 : start_size + (4 - start_size % 4));
	res->tblsz = 1 + (start_size >> 2);
	res->tbl = calloc(res->tblsz, sizeof(mHList *));
	if (!res->tbl)
		goto ERR2;
	res->lstn = 0;
	res->lstsz = start_size;
	res->lst = calloc(res->lstsz, sizeof(mHList));
	if (!res->lst)
		goto ERR3;
	if (!(res->free_slots = new_mStack(10)))
		goto ERR4;
	res->hsh_f = hsh_f;
	res->cmp_f = cmp_f;
	res->fk_f = NULL;
	res->fv_f = NULL;
	res->flags = 0;
	return res;
ERR4:
	free(res->lst);
ERR3:
	free(res->tbl);
ERR2:
	free(res);
ERR1:
	return NULL;
}
void destroy_mHashtable(mHashtable *tbl, mfree_f fkey, mfree_f fval)
{

	if ((tbl->flags & (MSHS_FREE_STR_KEY_ON_DESTROY | MSHS_FREE_STR_VALUE_ON_DESTROY)) || fkey || fval)
		while (tbl->lstn--)
		{
			mValue *v = &tbl->lst[tbl->lstn].key;
			if ((tbl->flags & MSHS_FREE_STR_KEY_ON_DESTROY) && v->s)
				free(v->s);
			else if (fkey && v->v)
				(*fkey)(v->v);

			v = &tbl->lst[tbl->lstn].value;
			if ((tbl->flags & MSHS_FREE_STR_VALUE_ON_DESTROY) && v->s)
				free(v->s);
			else if (fval && v->v)
				(*fval)(v->v);
		}
	free(tbl->lst);
	free(tbl->tbl);
	destroy_mStack(tbl->free_slots, NULL);
	free(tbl);
}

int mcmp_ull(mValue *a, mValue *b)
{
	if (a->ull == b->ull)
		return 0;
	else if (a->ull > b->ull)
		return 1;
	return -1;
}
int mcmp_s(mValue *a, mValue *b)
{
	return strcmp(a->s, b->s);
}
int mcmp_sbuf(mValue *a, mValue *b)
{
	return strncmp(a->sbuf, b->sbuf, sizeof(a->sbuf) - 1);
}
int mcmp_ll(mValue *a, mValue *b)
{
	if (a->ll == b->ll)
		return 0;
	else if (a->ll > b->ll)
		return 1;
	return 1;
}
static MHSH_RES_VALUE mhash_realloc(mHashtable *htbl)
{
	mHList **tbl_tmp, *list_buf_tmp;
	size_t new_tbl_sz, new_lst_sz = (htbl->lstsz << 2), i, c_hsh;
	new_tbl_sz = 1 + (new_lst_sz >> 2);

	list_buf_tmp = realloc(htbl->lst, sizeof(mHList) * new_lst_sz);

	if (!list_buf_tmp)
		return htbl->err_n = MHSH_RUN_OUT_OF_MEM;

	memset(list_buf_tmp + htbl->lstn, 0, (new_lst_sz - htbl->lstn) * sizeof(mHList));
	htbl->lst = list_buf_tmp;
	tbl_tmp = calloc(new_tbl_sz, sizeof(mHList *));
	if (!tbl_tmp)
		return htbl->err_n = MHSH_RUN_OUT_OF_MEM;

	htbl->tblsz = new_tbl_sz;
	for (i = 0; i < htbl->lstn; i++)
	{

		c_hsh = (*htbl->hsh_f)(&htbl->lst[i].key) % htbl->tblsz;
		htbl->lst[i].next = tbl_tmp[c_hsh];
		tbl_tmp[c_hsh] = htbl->lst + i;
	}
	htbl->lstsz = new_lst_sz;
	free(htbl->tbl);
	htbl->tbl = tbl_tmp;
	return htbl->err_n = MHSH_OK;
}

MHSH_RES_VALUE mhash_insert(mHashtable *htbl)
{
	size_t n_list, c_hsh;
	MHSH_RES_VALUE res;
	mHList *cur;
	htbl->o_key = NULL;
	htbl->o_val = NULL;
	c_hsh = (*htbl->hsh_f)(&htbl->i_key) % htbl->tblsz;
	for (cur = htbl->tbl[c_hsh]; cur; cur = cur->next)
		if ((*htbl->cmp_f)(&cur->key, &htbl->i_key) == 0)
		{
			htbl->o_key = &cur->key;
			htbl->o_val = &cur->value;
			res = htbl->err_n = MHSH_ERR_EXISTS;
			return res;
		}
	if (mStack_pop(htbl->free_slots))
	{
		n_list = htbl->free_slots->io_val.ull;
	}
	else
	{
		if (htbl->lstn >= htbl->lstsz)
		{
			res = mhash_realloc(htbl);
			if (res == MHSH_RUN_OUT_OF_MEM)
				return res;
			c_hsh = (*htbl->hsh_f)(&htbl->i_key) % htbl->tblsz;
		}
		n_list = htbl->lstn++;
	}

	cur = htbl->lst + n_list;
	cur->next = htbl->tbl[c_hsh];
	htbl->tbl[c_hsh] = cur;
	if (htbl->flags & MHSH_STRDUP_KEY)
	{
		if (!(cur->key.s = strdup(htbl->i_key.s)))
			return htbl->err_n = MHSH_RUN_OUT_OF_MEM;
	}
	else if (htbl->flags & MSHS_WCSDUP_KEY)
	{
		if (!(cur->key.ws = wcsdup(htbl->i_key.ws)))
			return htbl->err_n = MHSH_RUN_OUT_OF_MEM;
	}
	else
		cur->key = htbl->i_key;

	if (htbl->flags & MHSH_STRDUP_VALUE)
	{
		if (!(cur->value.s = strdup(htbl->i_val.s)))
			return htbl->err_n = MHSH_RUN_OUT_OF_MEM;
	}
	else if (htbl->flags & MSHS_WCSDUP_VALUE)
	{
		if (!(cur->key.ws = wcsdup(htbl->i_val.ws)))
			return htbl->err_n = MHSH_RUN_OUT_OF_MEM;
	}
	else
		cur->value = htbl->i_val;

	htbl->o_key = &cur->key;
	htbl->o_val = &cur->value;
	res = htbl->err_n = MHSH_OK;

	return res;
}
MHSH_RES_VALUE mhash_get(mHashtable *htbl)
{
	size_t c_hsh;
	mHList *cur;
	htbl->o_key = NULL;
	htbl->o_val = NULL;
	c_hsh = (*htbl->hsh_f)(&htbl->i_key) % htbl->tblsz;

	for (cur = htbl->tbl[c_hsh]; cur; cur = cur->next)
		if ((*htbl->cmp_f)(&htbl->i_key, &cur->key) == 0)
		{

			htbl->o_key = &cur->key;
			htbl->o_val = &cur->value;
			return htbl->err_n = MHSH_OK;
		}
	return htbl->err_n = MHSH_ERR_NOT_EXISTS;
}
MHSH_RES_VALUE mhash_pop(mHashtable *htbl)
{
	size_t c_hsh;
	mHList *cur, *prev_cur;
	htbl->err_n = MHSH_OK;
	htbl->o_key = NULL;
	htbl->o_val = NULL;

	c_hsh = (*htbl->hsh_f)(&htbl->i_key) % htbl->tblsz;
	for (prev_cur = NULL, cur = htbl->tbl[c_hsh]; cur; prev_cur = cur, cur = cur->next)
		if ((*htbl->cmp_f)(&htbl->i_key, &cur->key) == 0)
			break;

	if (!cur)
		return htbl->err_n = MHSH_ERR_NOT_EXISTS;

	if (prev_cur == NULL)
		htbl->tbl[c_hsh] = cur->next;
	else
		prev_cur->next = cur->next;

	htbl->free_slots->io_val.ull = cur - htbl->lst;

	if (!mStack_push(htbl->free_slots))
		htbl->err_n = MHSH_RUN_OUT_OF_MEM;

	if (htbl->flags & MHSH_DESTROY_KEY_ON_POP)
	{
		assert(htbl->fk_f);
		(*htbl->fk_f)(cur->key.v);
		memset(&cur->key, 0, sizeof(mValue));
	}
	else if (htbl->flags & MHSH_FREE_STR_KEY_ON_POP)
	{
		free(cur->key.s);
		memset(&cur->key, 0, sizeof(mValue));
	}

	if (htbl->flags & MHSH_DESTROY_VALUE_ON_POP)
	{
		assert(htbl->fv_f);
		(*htbl->fv_f)(cur->value.v);
		memset(&cur->value, 0, sizeof(mValue));
	}
	else if (htbl->flags & MHSH_FREE_STR_VALUE_ON_POP)
	{
		free(cur->value.s);
		memset(&cur->value, 0, sizeof(mValue));
	}
	htbl->_private_k = cur->key;
	htbl->_private_v = cur->value;
	htbl->o_key = &htbl->_private_k;
	htbl->o_val = &htbl->_private_v;
	memset(cur, 0, sizeof(mHList));
	return htbl->err_n;
}

m8String *new_m8String()
{
	m8String *res = malloc(sizeof(m8String));
	if (!res)
		return NULL;
	res->sz = mUSTRING_BUF_START_SZ;
	res->n = 0;
	res->s = malloc(sizeof(char) * res->sz);
	if (!res->s)
		goto ERR2;
	*res->s = '\0';
	return res;
ERR2:
	free(res);
	return NULL;
}
void destroy_m8String(m8String *u8s)
{
	free(u8s->s);
	free(u8s);
}
char *m8s_realloc(m8String *u8s, size_t len)
{
	if (u8s->n + len + 1 < u8s->sz)
		return u8s->s;
	size_t new_sz = (u8s->sz << 1) + len;
	char *tmp = realloc(u8s->s, sizeof(char) * new_sz);
	if (!tmp)
		return NULL;
	u8s->sz = new_sz;
	u8s->s = tmp;
	return u8s->s;
}
char *m8s_concat(m8String *u8s, register const char *s, register size_t len)
{
	register char *a;

	if (u8s->n + len + 1 >= u8s->sz) // I ever add one more so I sleep comfortably
		if (!m8s_realloc(u8s, len))
			return NULL;
	a = u8s->s + u8s->n;

	while (len-- && (*a = *s))
		++a, ++s; // if s is shorter than len, a must no go further, so, no *a++=*s++

	u8s->n = a - u8s->s;
	*a = '\0';

	return u8s->s;
}
char *m8s_concatc(m8String *u8s, char c)
{
	if (c == 0)
		return u8s->s;
	if (u8s->n + 1 + 1 >= u8s->sz)
		if (!m8s_realloc(u8s, 1))
			return NULL;
	u8s->s[u8s->n++] = c;
	u8s->s[u8s->n] = '\0';
	return u8s->s;
}
char *m8s_concati(m8String *u8s, register mll i)
{
#define MU8S_CONCATI_BUF_SZ 30
	static char buf[MU8S_CONCATI_BUF_SZ];
	register char *cur;
	cur = buf + MU8S_CONCATI_BUF_SZ;
	*(--cur) = '\0';
	if (i < 0)
	{
		if (!(m8s_concatcm(u8s, '-')))
			return NULL;
		i = -i;
	}
	do
	{
		*(--cur) = i % 10 + '0';
		i /= 10;
	} while (i);

	return m8s_concat(u8s, cur, (buf + MU8S_CONCATI_BUF_SZ - 1) - cur);
#undef MU8S_CONCATI_BUF_SZ
}
char *m8s_concatwcs(m8String *u8s, const mu16 *wcs)
{
	if (!wcs)
		return u8s->s;
	while (*wcs)
		if (!m8s_concatU16c(u8s, *wcs++))
			return NULL;

	return u8s->s;
}

char *m8s_strdup(m8String *u8s)
{
	char *res;
	res = malloc(u8s->n + 1);
	if (res)
		strcpy(res, u8s->s);
	return res;
}
m8String *m8s_clone(m8String *u8s)
{
	m8String *res = malloc(sizeof(m8String));
	if (!res)
		return NULL;
	res->n = u8s->n;
	res->sz = u8s->sz - u8s->n < 10 ? u8s->sz + 10 : u8s->sz;
	res->s = malloc(res->sz);
	if (res->s)
		strcpy(res->s, u8s->s);
	else
	{
		free(res);
		return NULL;
	}
	return res;
}

char *m8s_ltrim(m8String *u8s)
{
	register char *cur, *start;
	cur = start = u8s->s;
	while (*cur && isspace(*cur))
		++cur;
	if (cur > start)
	{
		while ((*start = *cur))
			++start, ++cur;

		u8s->n = start - u8s->s;
	}

	return u8s->s;
}

char *m8s_rtrim(m8String *u8s)
{

	if (u8s->n)
	{
		char *cur = u8s->s + u8s->n;
		while (cur > u8s->s && isspace(cur[-1]))
			--cur;
		*(cur) = '\0';
		u8s->n = cur - u8s->s;
	}
	return u8s->s;
}
char *m8s_replace(m8String *u8s, char *from, char *to)
{
	assert(from && to);
	size_t from_len = strlen(from), to_len = strlen(to);
	if (from_len == 0)
		return u8s->s;
	if (from_len >= to_len)
	{
		char *cur, *sv, *curcur;
		curcur = sv = cur = u8s->s;
		while (*cur && (cur = strstr(cur, from)))
		{
			strncpy(sv, curcur, (cur - curcur));
			sv += (cur - curcur);
			strncpy(sv, to, to_len);
			sv += to_len;
			curcur = cur = cur + from_len;
		}
		while ((*sv = *curcur))
			sv++, curcur++;
		u8s->n = sv - u8s->s;
	}
	else
	{
		if (strstr(u8s->s, from))
		{
			m8String *ns = m8s_clone(u8s); // I clone it just to have a buffer in the same order of magnitude: kB, MB,GB
			if (!ns)
				return NULL;
			m8s_reset(ns);
			char *curi, *curcur;
			curcur = curi = u8s->s;
			while (*curi && (curi = strstr(curi, from)))
			{
				m8s_concat(ns, curcur, curi - curcur);
				curi = curcur = curi + from_len;
				m8s_concat(ns, to, to_len);
			}
			m8s_concat(ns, curcur, (u8s->s + u8s->n) - curcur);
			free(u8s->s);
			memcpy(u8s, ns, sizeof(m8String));
			free(ns);
		}
	}
	return u8s->s;
}
char *transliterate_diac(const char *utf8seq, char ans[5])
{
	static mHashtable *ht;
	if (!ht)
	{
		int i;
		// clang-format off
		char *rep[][2] = {
			{"á","a"},{"Á","A"},{"à","a"},{"À","A"},{"ă","a"},{"Ă","A"},{"â","a"},{"Â","A"},{"å","a"},{"Å","A"},
			{"ã","a"},{"Ã","A"},{"ą","a"},{"Ą","A"},{"ā","a"},{"Ā","A"},{"ä","ae"},{"Ä","AE"},{"æ","ae"},{"Æ","AE"},
			{"ḃ","b"},{"Ḃ","B"},{"ć","c"},{"Ć","C"},{"ĉ","c"},{"Ĉ","C"},{"č","c"},{"Č","C"},{"ċ","c"},{"Ċ","C"},
			{"ç","c"},{"Ç","C"},{"ď","d"},{"Ď","D"},{"ḋ","d"},{"Ḋ","D"},{"đ","d"},{"Đ","D"},{"ð","dh"},{"Ð","Dh"},
			{"é","e"},{"É","E"},{"è","e"},{"È","E"},{"ĕ","e"},{"Ĕ","E"},{"ê","e"},{"Ê","E"},{"ě","e"},{"Ě","E"},
			{"ë","e"},{"Ë","E"},{"ė","e"},{"Ė","E"},{"ę","e"},{"Ę","E"},{"ē","e"},{"Ē","E"},{"ḟ","f"},{"Ḟ","F"},
			{"ƒ","f"},{"Ƒ","F"},{"ğ","g"},{"Ğ","G"},{"ĝ","g"},{"Ĝ","G"},{"ġ","g"},{"Ġ","G"},{"ģ","g"},{"Ģ","G"},
			{"ĥ","h"},{"Ĥ","H"},{"ħ","h"},{"Ħ","H"},{"í","i"},{"Í","I"},{"ì","i"},{"Ì","I"},{"î","i"},{"Î","I"},
			{"ï","i"},{"Ï","I"},{"ĩ","i"},{"Ĩ","I"},{"į","i"},{"Į","I"},{"ī","i"},{"Ī","I"},{"ĵ","j"},{"Ĵ","J"},
			{"ķ","k"},{"Ķ","K"},{"ĺ","l"},{"Ĺ","L"},{"ľ","l"},{"Ľ","L"},{"ļ","l"},{"Ļ","L"},{"ł","l"},{"Ł","L"},
			{"ṁ","m"},{"Ṁ","M"},{"ń","n"},{"Ń","N"},{"ň","n"},{"Ň","N"},{"ñ","n"},{"Ñ","N"},{"ņ","n"},{"Ņ","N"},
			{"ó","o"},{"Ó","O"},{"ò","o"},{"Ò","O"},{"ô","o"},{"Ô","O"},{"ő","o"},{"Ő","O"},{"õ","o"},{"Õ","O"},
			{"ø","oe"},{"Ø","OE"},{"ō","o"},{"Ō","O"},{"ơ","o"},{"Ơ","O"},{"ö","oe"},{"Ö","OE"},{"ṗ","p"},{"Ṗ","P"},
			{"ŕ","r"},{"Ŕ","R"},{"ř","r"},{"Ř","R"},{"ŗ","r"},{"Ŗ","R"},{"ś","s"},{"Ś","S"},{"ŝ","s"},{"Ŝ","S"},
			{"š","s"},{"Š","S"},{"ṡ","s"},{"Ṡ","S"},{"ş","s"},{"Ş","S"},{"ș","s"},{"Ș","S"},{"ß","ss"},{"ť","t"},
			{"Ť","T"},{"ṫ","t"},{"Ṫ","T"},{"ţ","t"},{"Ţ","T"},{"ț","t"},{"Ț","T"},{"ŧ","t"},{"Ŧ","T"},{"ú","u"},
			{"Ú","U"},{"ù","u"},{"Ù","U"},{"ŭ","u"},{"Ŭ","U"},{"û","u"},{"Û","U"},{"ů","u"},{"Ů","U"},{"ű","u"},
			{"Ű","U"},{"ũ","u"},{"Ũ","U"},{"ų","u"},{"Ų","U"},{"ū","u"},{"Ū","U"},{"ư","u"},{"Ư","U"},{"ü","ue"},
			{"Ü","UE"},{"ẃ","w"},{"Ẃ","W"},{"ẁ","w"},{"Ẁ","W"},{"ŵ","w"},{"Ŵ","W"},{"ẅ","w"},{"Ẅ","W"},{"ý","y"},
			{"Ý","Y"},{"ỳ","y"},{"Ỳ","Y"},{"ŷ","y"},{"Ŷ","Y"},{"ÿ","y"},{"Ÿ","Y"},{"ź","z"},{"Ź","Z"},{"ž","z"},
			{"Ž","Z"},{"ż","z"},{"Ż","Z"},{"þ","th"},{"Þ","Th"},{"µ","u"},{"а","a"},{"А","a"},{"б","b"},{"Б","b"},
			{"в","v"},{"В","v"},{"г","g"},{"Г","g"},{"д","d"},{"Д","d"},{"е","e"},{"Е","E"},{"ё","e"},{"Ё","E"},
			{"ж","zh"},{"Ж","zh"},{"з","z"},{"З","z"},{"и","i"},{"И","i"},{"й","j"},{"Й","j"},{"к","k"},{"К","k"},
			{"л","l"},{"Л","l"},{"м","m"},{"М","m"},{"н","n"},{"Н","n"},{"о","o"},{"О","o"},{"п","p"},{"П","p"},
			{"р","r"},{"Р","r"},{"с","s"},{"С","s"},{"т","t"},{"Т","t"},{"у","u"},{"У","u"},{"ф","f"},{"Ф","f"},
			{"х","h"},{"Х","h"},{"ц","c"},{"Ц","c"},{"ч","ch"},{"Ч","ch"},{"ш","sh"},{"Ш","sh"},{"щ","sch"},{"Щ","sch"},
			{"ъ",""},{"Ъ",""},{"ы","y"},{"Ы","y"},{"ь",""},{"Ь",""},{"э","e"},{"Э","e"},{"ю","ju"},{"Ю","ju"},
			{"я","ja"},{"Я","ja"},{"€","EUR"},///if you add something remember that the ans buffer is 5
		};
		// clang-format on
		ht = new_mHashtable(sizeof(rep) / sizeof(*rep), mhash_f_sbuf, mcmp_sbuf);
		assert(ht);
#ifdef DHASH_H_INCLUDED
		dcomment(ht, "Static hashtable  function transliterate_diac()");
		dcomment(ht->tbl, "Static hashtable  function transliterate_diac()");
		dcomment(ht->lst, "Static hashtable  function transliterate_diac()");
		dcomment(ht->free_slots, "Static hashtable  function transliterate_diac()");
		dcomment(ht->free_slots->val, "Static hashtable  function transliterate_diac()");
#endif
		for (i = 0; i < sizeof(rep) / sizeof(*rep); i++)
		{
			// if(strlen(rep[i][0])<strlen(rep[i][1]))
			// printf("%s shorter than %s\n",rep[i][0],rep[i][1]);
			// if (strlen(rep[i][0]) > 2)
			//	printf("%s %d bytes\n", rep[i][0], (int)strlen(rep[i][0]));
			strcpy(ht->i_key.sbuf, rep[i][0]);
			strcpy(ht->i_val.sbuf, rep[i][1]);
			if (mhash_insert(ht) == MHSH_RUN_OUT_OF_MEM)
				assert(0);
			else if (ht->err_n == MHSH_ERR_EXISTS)
			{
				printf("%s key already exists\n", ht->o_key->sbuf);
			}
		}
	}
	strcpy(ht->i_key.sbuf, utf8seq);
	if (mhash_get(ht) == MHSH_ERR_NOT_EXISTS)
	{
		ans[0] = '\0';
		return NULL;
	}
	strcpy(ans, ht->o_val->sbuf);
	return ans;
}

size_t strlen_mb(register const char *s)
{

	int seq_len;
	size_t res = 0;
	char seq[5];

	while ((seq_len = read_utf8_seq(s, seq)))
	{
		res++;
		if (seq_len > 0)
			s += seq_len;
		else
			s -= seq_len;
	}
	return res;
}
int mU16c_to_m8c(mu16 c, char ans[5])
{
	int res = 0;

	if (c < 0x80)
	{
		res = 1;
		ans[0] = (char)c;
		ans[1] = '\0';
	}
	else if (c < 0x800)
	{
		res = 2;
		ans[0] = (char)(((c >> 6) & 0x1F) | 0xC0);
		ans[1] = (char)(((c >> 0) & 0x3F) | 0x80);
		ans[2] = '\0';
	}
	else if ((unsigned int)c < 0x10000)
	{
		res = 3;
		ans[0] = (char)(((c >> 12) & 0x0F) | 0xE0);
		ans[1] = (char)(((c >> 6) & 0x3F) | 0x80);
		ans[2] = (char)(((c >> 0) & 0x3F) | 0x80);
		ans[3] = '\0';
	}
	else if ((unsigned int)c <= 0x10FFFF)
	{
		res = 4;
		ans[0] = (char)((((unsigned int)c >> 18) & 0x07) | 0xF0);
		ans[1] = (char)((((unsigned int)c >> 12) & 0x3F) | 0x80);
		ans[2] = (char)((((unsigned int)c >> 6) & 0x3F) | 0x80);
		ans[3] = (char)((((unsigned int)c >> 0) & 0x3F) | 0x80);
		ans[4] = '\0';
	}

	return res;
}
int read_utf8_seq(const char *stream, char ans[5])
{

	register unsigned char *cur = (unsigned char *)ans;
	register unsigned char *buf = (unsigned char *)stream;
//clang-format off
#define MP_READ_UTF8_INTERNAL_MACRO(a)			\
	do						\
	{						\
		if ((*buf & 0xC0) != 0x80)		\
			goto NOT_VALID_SEQ;		\
		*cur++ = *buf++;			\
	} while (buf - (unsigned char *)stream < (a))
//clang-format on
	if (!(buf && *buf))
		return *cur = '\0';
	*cur++ = *buf++;

	if ((unsigned char)ans[0] < 0x80)
	{
	}
	else if (((unsigned char)ans[0] & 0xE0) == 0xC0)
		MP_READ_UTF8_INTERNAL_MACRO(2);
	else if (((unsigned char)ans[0] & 0xF0) == 0xE0)
		MP_READ_UTF8_INTERNAL_MACRO(3);
	else if (((unsigned char)ans[0] & 0xF8) == 0xF0)
		MP_READ_UTF8_INTERNAL_MACRO(4);
	else
		goto NOT_VALID_SEQ;

#undef MP_READ_UTF8_INTERNAL_MACRO
	*cur = '\0';
	return (int)(buf - (unsigned char *)stream);
NOT_VALID_SEQ:
	*cur = '\0';
	return (int)((unsigned char *)stream - buf);
}

char *m8s_concatU16c(m8String *s, mu16 c)
{
	char buf[5];
	if (c == 0)
		return s->s;
	int len = mU16c_to_m8c(c, buf);

	return m8s_concat(s, buf, len);
}
mU16String *new_mU16String()
{
	mU16String *res = malloc(sizeof(mU16String));
	if (!res)
		return NULL;
	res->sz = mUSTRING_BUF_START_SZ;
	res->n = 0;
	res->s = malloc(sizeof(mu16) * res->sz);
	if (!res->s)
		goto ERR2;
	*res->s = '\0';
	return res;
ERR2:
	free(res);
	return NULL;
}
void destroy_mU16String(mU16String *u16s)
{
	free(u16s->s);
	free(u16s);
}
mu16 *mU16s_realloc(mU16String *u16s, size_t len)
{
	if (u16s->n + len + 1 < u16s->sz)
		return u16s->s;
	size_t new_sz = (u16s->sz << 1) + len;
	mu16 *tmp = realloc(u16s->s, sizeof(mu16) * new_sz);
	if (!tmp)
		return NULL;
	u16s->sz = new_sz;
	u16s->s = tmp;
	return u16s->s;
}
mu16 *mU16s_concat(mU16String *u16s, register const mu16 *s, register size_t len)
{
	register mu16 *a;

	if (u16s->n + len + 1 >= u16s->sz)
		if (!mU16s_realloc(u16s, len))
			return NULL;

	a = u16s->s + u16s->n;
	while (len-- && (*a = *s))
		++a, ++s;

	u16s->n = a - u16s->s;
	*a = '\0';
	return u16s->s;
}
mu16 *mU16s_concatc(mU16String *u16s, mu16 c)
{
	if (c == 0)
		return u16s->s;
	if (u16s->n + 1 + 1 >= u16s->sz)
		if (!mU16s_realloc(u16s, 1))
			return NULL;
	u16s->s[u16s->n++] = c;
	u16s->s[u16s->n] = '\0';
	return u16s->s;
}
/* --------------------------------------------------------------------- */

char *mU16s_to_mU8s(mU16String *s16, m8String *s8)
{

	int len;
	char temp_buf[5];
	mu16 *cur = s16->s;
	m8s_reset(s8);
	while (*cur)
	{
		len = mU16c_to_m8c(*cur++, temp_buf);
		if (!m8s_concat(s8, temp_buf, len))
			return NULL;
	}

	return s8->s;
}

mu16 *m8s_to_mU16s(m8String *u8s, mU16String *u16s)
{
	int len;
	char lbuf[5];
	unsigned char *cur;
	mu16 c;
	cur = (unsigned char *)u8s->s;
	while (*cur)
	{
		len = read_utf8_seq((char *)cur, lbuf);
		if (len == 0)
			break;
		else if (len < 0)
		{
			if (!mU16s_concatcm(u16s, 0xFFFD))
				return NULL;
			cur += -len;
			continue;
		}
		switch (len)
		{
		case 1:
			c = lbuf[0];
			break;
		case 2:
			c = (((unsigned int)lbuf[0] & 0x1F) << 6) + (lbuf[1] & 0x3F);
			break;
		case 3:
			c = (((unsigned int)lbuf[0] & 0xF) << 12) + (((unsigned int)lbuf[1] & 0x3F) << 6) +
				(((unsigned int)lbuf[2] & 0x3F));
			break;
		case 4:
			c = (((unsigned int)lbuf[0] & 0x7) << 18) + (((unsigned int)lbuf[1] & 0x3F) << 12) +
				(((unsigned int)lbuf[2] & 0x3F) << 6) + (((unsigned int)lbuf[3] & 0x3F));
			break;
		default:
			assert(0);
		}

		if (!mU16s_concatcm(u16s, c))
			return NULL;
		cur += len;
	}
	return u16s->s;
}
static MP_ERRS mp_loadfile(const char *file_name, char **ans, size_t *ans_len)
{

	FILE *f = fopen(file_name, "rb");
	MP_ERRS res = MP_OK;
	if (!f)
	{
		res = MP_CANNOT_OPEN_FILE;
		goto ERR1;
	}
	fseek(f, 0, SEEK_END);
	*ans_len = ftell(f);
	*ans = malloc(sizeof(char) * *ans_len);
	if (!*ans)
	{
		res = MP_RUN_OUT_OF_MEMORY;
		goto ERR2;
	}
	fseek(f, 0, SEEK_SET);
	if (fread(*ans, sizeof(char), *ans_len, f) != *ans_len)
	{
		res = MP_ERR_READING_FILE;
		goto ERR3;
	}
	fclose(f);

	return res;
ERR3:
	free(*ans);
ERR2:
	fclose(f);
ERR1:
	*ans = NULL;
	*ans_len = 0;
	return res;
}
static const char *scan_16(const char *cur, mu16 *res)
{
	const unsigned char *lcur = (const unsigned char *)cur;
	*res = *lcur++;
	*res += ((mu16)*lcur << 8);
	cur += 2;
	return cur;
}
static const char *scan_text(const char *cur, mUString *us, const char *end, MP_ERRS *err_n)
{
	mu16 uc;
	*err_n = MP_OK;
	mUs_reset(us);
	if (end - cur < 1)
	{
		*err_n = MP_NOT_VALID_GRAMMAR;
		return NULL;
	}
	do
	{
		cur = scan_16(cur, &uc);
		if (!mUs_concat_U16c(us, uc))
		{
			*err_n = MP_RUN_OUT_OF_MEMORY;
			return NULL;
		}
	} while (uc && end - cur > 1);

	if (uc)
	{
		*err_n = MP_NOT_VALID_GRAMMAR;
		return NULL;
	}

	return cur;
}
typedef enum mp_fld_type
{
	MP_EMPTY,
	MP_BYTE,
	MP_BOOL,
	MP_STRING,
	MP_INT

} MP_FIELD_TYPE;
typedef struct mp_fld
{
	MP_FIELD_TYPE type;
	union v {
		char b;
		muchar *s;
		mu16 i;
	} v;
} mpField;
typedef struct mp_rec
{
	mpField *flds;
	size_t n;
	size_t sz;
	mUString *us;

} mpRecord;

MP_ERRS mp_range_to_chset(mUString *us, mu16 start_range, mu16 end_range)
{
	while (start_range <= end_range && start_range > 0)
	{
		if (!mUs_concat_U16c(us, start_range))
			return MP_RUN_OUT_OF_MEMORY;
		start_range++;
	}
	return MP_OK;
}
static const char *mp_read_record(mpRecord *ans, const char *cur, const char *end, MP_ERRS *err_n)
{
	mu16 r_n;
	if (*cur++ != 'M')
	{
		*err_n = MP_NOT_VALID_GRAMMAR;
		return NULL;
	} // Rec Id
	cur = scan_16(cur, &r_n);
	ans->n = 0;
	if (r_n >= ans->sz)
	{
		size_t new_sz = r_n + ans->sz;
		if (ans->flds)
			free(ans->flds);

		ans->flds = malloc(sizeof(mpField) * new_sz);

		if (!ans->flds)
		{
			*err_n = MP_RUN_OUT_OF_MEMORY;
			return NULL;
		}
		ans->sz = new_sz;
	}
	while (r_n-- && cur <= end)
	{
		mpField *l = ans->flds + ans->n++;
		switch (*cur++)
		{
		case 'E':
			l->type = MP_EMPTY;
			break;
		case 'b':
			l->type = MP_BYTE;
			l->v.b = *cur++;
			break;
		case 'B':
			l->type = MP_BOOL;
			l->v.b = *cur++;
			break;
		case 'I':
			l->type = MP_INT;
			cur = scan_16(cur, &l->v.i);
			break;
		case 'S':
			l->type = MP_STRING;
			cur = scan_text(cur, ans->us, end, err_n);
			if (*err_n != MP_OK)
				return cur;
			if (!(l->v.s = ustrdup(ans->us->s)))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				return cur;
			}
			break;
		default:
			*err_n = MP_NOT_VALID_GRAMMAR;
			return cur;
		}
	}
	*err_n = MP_OK;
	return cur;
}
static void destroy_mGram(mGram *g)
{

	if (g)
	{
		if (g->dfa)
		{
			while (g->ndfa--)
				if (g->dfa[g->ndfa].edge)
					free(g->dfa[g->ndfa].edge);
			free(g->dfa);
		}
		if (g->lalr)
		{
			while (g->nlalr--)
			{
				if (g->lalr[g->nlalr].action)
					free(g->lalr[g->nlalr].action);
				if (g->lalr[g->nlalr].h)
					destroy_mHashtable(g->lalr[g->nlalr].h, NULL, NULL);
			}

			free(g->lalr);
		}

		if (g->rule)
		{
			while (g->nrule--)
				if (g->rule[g->nrule].symbol)
					free(g->rule[g->nrule].symbol);
			free(g->rule);
		}
		if (g->sym)
		{
			while (g->nsym--)
				if (g->sym[g->nsym].Name)
					free((char *)g->sym[g->nsym].Name);
			free(g->sym);
		}
		if (g->chset)
		{
			while (g->ncharset--)
			{
				if (g->chset[g->ncharset].h)
					destroy_mHashtable(g->chset[g->ncharset].h, NULL, NULL);
				if (g->chset[g->ncharset].chset)
					free(g->chset[g->ncharset].chset);
			}

			free(g->chset);
		}
		if (g->grp)
		{
			while (g->ngrp--)
			{
				if (g->grp[g->ngrp].name)
					free(g->grp[g->ngrp].name);
				if (g->grp[g->ngrp].nest)
					free(g->grp[g->ngrp].nest);
			}
			free(g->grp);
		}
#if MP_LOAD_PROPERTY
		while (g->n_props--)
		{
			if (g->prop_name[g->n_props])
				free(g->prop_name[g->n_props]);
			;
			if (g->prop_value[g->n_props])
				free(g->prop_value[g->n_props]);
		}
		free(g->prop_name);
		free(g->prop_value);
#endif
		free(g);
	}
}

static mGram *new_mGram(const char *src, size_t len, MP_ERRS *err_n)
{
	mGram *res = NULL;
	mUString *us = NULL;

	const char *cur = src, *end = src + len;
	int i;
	mpField *fld;
	if (!(res = calloc(1, sizeof(mGram))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	if (!(us = new_mUString()))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	if (!(cur = scan_text(cur, us, end, err_n)))
		goto ERR;

#if MP_CODING == MP_UTF8
	if (strcmp(us->s, "GOLD Parser Tables/v5.0"))
	{
		*err_n = MP_NOT_VALID_GRAMMAR;
		goto ERR;
	}
#elif MP_CODING == MP_UNICODE
	if (wcscmp(us->s, L"GOLD Parser Tables/v5.0"))
	{
		*err_n = MP_NOT_VALID_GRAMMAR;
		goto ERR;
	}
#endif // MP_CODING

	mpRecord rec = {0, 0, 0, 0};
	rec.us = us;

#if MP_LOAD_PROPERTY
	if (!(res->prop_name = calloc(8, sizeof(muchar *))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	};
	if (!(res->prop_value = calloc(8, sizeof(muchar *))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	};
	res->n_props = 0;
#endif
	while (cur < end)
	{
		cur = mp_read_record(&rec, cur, end, err_n);
		if (*err_n != MP_OK)
			goto ERR;
		if (rec.flds[0].type != MP_BYTE)
		{
			*err_n = MP_NOT_VALID_GRAMMAR;
			goto ERR;
		}
		switch (rec.flds[0].v.b)
		{
		case 'p':
#if MP_LOAD_PROPERTY
			res->prop_name[res->n_props] = rec.flds[2].v.s;
			res->prop_value[res->n_props] = rec.flds[3].v.s;
			res->n_props++;
#else
			free(rec.flds[2].v.s);
			free(= rec.flds[3].v.s);
#endif
			break;
		case 't':
			fld = rec.flds + 1;
			res->nsym = (fld++)->v.i;
			if (!(res->sym = calloc(res->nsym, sizeof(mSymbol))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};

			res->ncharset = (fld++)->v.i;
			if (!(res->chset = calloc(res->ncharset, sizeof(mCharSet))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};

			res->nrule = (fld++)->v.i;
			if (!(res->rule = calloc(res->nrule, sizeof(mRule))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};

			res->ndfa = (fld++)->v.i;
			if (!(res->dfa = calloc(res->ndfa, sizeof(mDfa))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};

			res->nlalr = (fld++)->v.i;
			if (!(res->lalr = calloc(res->nlalr, sizeof(mLalr))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};
			res->ngrp = fld->v.i;
			if (!(res->grp = calloc(res->ngrp, sizeof(mGroup))))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};
			break;
		case 'I':
			res->init_dfa = rec.flds[1].v.i;
			res->init_lalr = rec.flds[2].v.i;
			break;
		case 'c':
			mUs_reset(us);
			fld = rec.flds + 5;
			i = rec.flds[3].v.i;
			while (i--)
			{
				if ((*err_n = mp_range_to_chset(us, fld->v.i, fld[1].v.i)) != MP_OK)
					goto ERR;
				fld += 2;
			}

			if (!(res->chset[rec.flds[1].v.i].chset = ustrdup(us->s)))
			{
				*err_n = MP_RUN_OUT_OF_MEMORY;
				goto ERR;
			};
			if (us->n > MP_SWITCH_TO_HASHTABLE_THRESHOLD)
			{
				muchar *cur = res->chset[rec.flds[1].v.i].chset;
				mHashtable *h;

#if MP_CODING == MP_UTF8
				muchar *save_start = cur;
				h = new_mHashtable(us->n << 2, mhash_f_sbuf, mcmp_sbuf);
				if (!h)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				};
				while (cur - save_start < (ptrdiff_t)us->n)
				{
					int n_seq;
					n_seq = read_utf8_seq(cur, h->i_key.sbuf);
					if (n_seq < 0)
					{
						*err_n = MP_NOT_VALID_GRAMMAR;
						goto ERR;
					};
					cur += n_seq;
					if (mhash_insert(h) == MHSH_RUN_OUT_OF_MEM)
					{
						*err_n = MP_RUN_OUT_OF_MEMORY;
						goto ERR;
					}
					else if (h->err_n == MHSH_ERR_EXISTS)
					{
						printf("Char set %d contains more than once char %s", (int)rec.flds[1].v.i, h->i_key.sbuf);
					}
				}

#elif MP_CODING == MP_UNICODE
				h = new_mHashtable(us->n, mhash_f_ull, mcmp_ull);
				if (!h)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				};
				while (*cur)
				{
					h->i_key.ull = *cur++;
					if (mhash_insert(h) == MHSH_RUN_OUT_OF_MEM)
					{
						*err_n = MP_RUN_OUT_OF_MEMORY;
						goto ERR;
					}
					else if (h->err_n == MHSH_ERR_EXISTS)
					{
						printf("Char set %d contains more than once char %d", (int)rec.flds[1].v.i, (int)h->i_key.ull);
					}
				}
#endif
				res->chset[rec.flds[1].v.i].h = h;
			}

			break;
		case 'S':
			res->sym[rec.flds[1].v.i].Name = rec.flds[2].v.s;
			res->sym[rec.flds[1].v.i].Type = rec.flds[3].v.i;

			break;
		case 'g': {
			mGroup *gr = res->grp + rec.flds[1].v.i;
			fld = rec.flds + 2;
			gr->name = (fld++)->v.s;
			gr->cont_ix = (fld++)->v.i;
			gr->start_ix = (fld++)->v.i;
			gr->end_ix = (fld++)->v.i;
			gr->advance_mode = (fld++)->v.i;
			gr->end_mode = (fld++)->v.i;

			fld++;
			gr->nnest = (fld++)->v.i;
			if (gr->nnest > 0)
			{
				short *tmp;
				tmp = gr->nest = malloc(gr->nnest * sizeof(short));
				if (!tmp)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				}
				while (fld - rec.flds < (ptrdiff_t)rec.n)
					*tmp++ = (fld++)->v.i;
			}
		}
		break;

		case 'R':
			res->rule[rec.flds[1].v.i].NonTerminal = rec.flds[2].v.i;
			i = res->rule[rec.flds[1].v.i].nsymbol = (int)(rec.n - 4);
			if (i > 0)
			{
				short *tmp;
				tmp = res->rule[rec.flds[1].v.i].symbol = malloc(i * sizeof(short));
				if (!tmp)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				}
				fld = rec.flds + 4;
				while (fld - rec.flds < (ptrdiff_t)rec.n)
					*tmp++ = (fld++)->v.i;
			}
			break;
		case 'D':
			res->dfa[rec.flds[1].v.i].Accept = rec.flds[2].v.b;
			res->dfa[rec.flds[1].v.i].AcceptIndex = rec.flds[3].v.i;
			i = res->dfa[rec.flds[1].v.i].nedge = (int)(rec.n - 5) / 3;
			if (i > 0)
			{
				mEdge *tmp;
				tmp = res->dfa[rec.flds[1].v.i].edge = calloc(i, sizeof(mEdge));
				if (!tmp)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				}
				fld = rec.flds + 5;
				while (fld - rec.flds < (ptrdiff_t)rec.n)
				{
					tmp->CharSetIndex = (fld++)->v.i;
					(tmp++)->TargetIndex = (fld++)->v.i;
					fld++;
				}
			}
			break;
		case 'L':
			i = res->lalr[rec.flds[1].v.i].naction = (int)(rec.n - 3) / 4;
			if (i > 0)
			{
				mAction *tmp;
				tmp = res->lalr[rec.flds[1].v.i].action = calloc(i, sizeof(mAction));
				if (!tmp)
				{
					*err_n = MP_RUN_OUT_OF_MEMORY;
					goto ERR;
				}
				if (i > MP_SWITCH_TO_HASHTABLE_THRESHOLD)
				{
					res->lalr[rec.flds[1].v.i].h = new_mHashtable(i, mhash_f_ull, mcmp_ull);
					if (!res->lalr[rec.flds[1].v.i].h)
					{
						*err_n = MP_RUN_OUT_OF_MEMORY;
						goto ERR;
					}
				}
				else
					res->lalr[rec.flds[1].v.i].h = NULL;
				fld = rec.flds + 3;
				while (fld - rec.flds < (ptrdiff_t)rec.n)
				{
					tmp->SymbolIndex = (fld++)->v.i;
					if (res->lalr[rec.flds[1].v.i].h)
					{
						res->lalr[rec.flds[1].v.i].h->i_key.ull = tmp->SymbolIndex;
						res->lalr[rec.flds[1].v.i].h->i_val.v = tmp;
						mhash_insert(res->lalr[rec.flds[1].v.i].h);
						switch (res->lalr[rec.flds[1].v.i].h->err_n)
						{
						case MHSH_OK:
							break;
						case MHSH_RUN_OUT_OF_MEM:
							*err_n = MP_RUN_OUT_OF_MEMORY;
							goto ERR;
						case MHSH_ERR_EXISTS:
							printf("LALR %d Contains key %d more than once\n", (int)rec.flds[1].v.i,
								   (int)tmp->SymbolIndex);
							break;
						default:
							printf("ERR:HSHTBL NOT DEFINED FILE %s LINE %d\n", __FILE__, __LINE__);
							break;
						}
					}

					tmp->Action = (fld++)->v.i;
					(tmp++)->Target = (fld++)->v.i;
					fld++;
				}
			}
			break;
		}
	}

	destroy_mUString(us);
	free(rec.flds);
	return res;
ERR:
	if (rec.flds)
		free(rec.flds);
	if (us)
		destroy_mUString(us);
	if (res)
		destroy_mGram(res);
	return NULL;
}

static void clean_mtree_chunk_mem(mTree *t, size_t sz)
{
	while (sz--)
	{
		if (t->token.lexeme)
			free((char *)t->token.lexeme);
		if (t->chs)
			free(t->chs);
		t++;
	}
}
MP_ERRS mpPush_token(mParser *mp, short symbol, muchar *lexeme)
{

	if (mp->ntokens >= mp->sztoks)
	{
		size_t new_sz = mp->sztoks << 1;
		mToken *tmp = realloc(mp->tokens, sizeof(mToken) * new_sz);
		if (!tmp)
			MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, MP_RUN_OUT_OF_MEMORY);
		memset(tmp + mp->ntokens, 0, sizeof(mToken) * (new_sz - mp->ntokens));
		mp->sztoks = new_sz;
		mp->tokens = tmp;
	}
	mp->tokens[mp->ntokens].id = symbol;
	if (lexeme)
	{
		if (!(mp->tokens[mp->ntokens].lexeme = ustrdup(lexeme)))
			MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, MP_RUN_OUT_OF_MEMORY);
	}
	else
		mp->tokens[mp->ntokens].lexeme = NULL;
	mp->ntokens++;
	return MP_OK;
}

size_t mpPop_token(mParser *mp)
{
	if (mp->ntokens < 1)
		return 0;

	if (mp->tokens[--mp->ntokens].lexeme)
		free((char *)mp->tokens[mp->ntokens].lexeme);
	memset(mp->tokens + mp->ntokens, 0, sizeof(mToken));
	return 1;
}
void destroy_mParser(mParser *mp)
{

	if (!mp)
		return;
	if (mp->grm)
		destroy_mGram(mp->grm);
	if (mp->tree_data.mem_chunks)
	{
		size_t sz = mp->tree_data.n;

		while (mStack_pop(mp->tree_data.mem_chunks))
		{
			clean_mtree_chunk_mem(mp->tree_data.mem_chunks->io_val.v, sz);
			free(mp->tree_data.mem_chunks->io_val.v);
			sz = mp->tree_data.sz;
		}
		destroy_mStack(mp->tree_data.mem_chunks, NULL);
	}
	if (mp->tree_stack)
		destroy_mStack(mp->tree_stack, NULL);

	while (mpPop_token(mp))
		;

	if (mp->lex)
		destroy_mUString(mp->lex);
	free(mp->tokens);

	free(mp);
}

mParser *new_mParserA(const char *src, size_t len, MP_ERRS *err_n)
{

	mParser *res;
	if (!(res = calloc(1, sizeof(mParser))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	*err_n = MP_OK;

	if (!(res->grm = new_mGram(src, len, err_n)))
		goto ERR;

	/**treeData**/
	if (!(res->tree_data.mem_chunks = new_mStack(5)))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	res->tree_data.sz = MP_TREE_MEM_CHUNK_SZ;
	res->tree_data.n = 0;
	if (!(res->tree_data.mem_chunks->io_val.v = res->tree_data.cur_buf = calloc(res->tree_data.sz, sizeof(mTree))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	mStack_push(res->tree_data.mem_chunks);
	if (!(res->tree_stack = new_mStack(5)))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	};

	res->sztoks = MP_TOKEN_BUF_SZ;
	res->ntokens = 0;
	if (!(res->tokens = calloc(res->sztoks, sizeof(mToken))))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	if (!(res->lex = new_mUString()))
	{
		*err_n = MP_RUN_OUT_OF_MEMORY;
		goto ERR;
	}
	return res;
ERR:
	if (res)
		destroy_mParser(res);
	return NULL;
}
mParser *new_mParserF(const char *file_name, MP_ERRS *err_n)
{
	char *gt;
	size_t src_len;
	mParser *res = NULL;
	if ((*err_n = mp_loadfile(file_name, &gt, &src_len)) != MP_OK)
		return NULL;
	if (gt)
	{
		res = new_mParserA(gt, src_len, err_n);
		free(gt);
	}

	return res;
}

void mpReset(mParser *mp)
{
	size_t sz;

	mp->reduction = 0;
	mp->err_msg = NULL;
	mp->err_n = MP_OK;
	mp->in.cur = mp->in.exp = NULL;
	mp->in.MEOF = 0;
	mp->in.exp_len = 0;

	mp->in.row_n = 1;

	mp->lalr_state = mp->grm->init_lalr;

	mp->out_tree = NULL;
	sz = mp->tree_data.n;
	while (mp->tree_data.mem_chunks->n > 1)
	{
		mStack_pop(mp->tree_data.mem_chunks);
		clean_mtree_chunk_mem(mp->tree_data.mem_chunks->io_val.v, sz);
		free(mp->tree_data.mem_chunks->io_val.v);
		sz = mp->tree_data.sz;
	}
	clean_mtree_chunk_mem(mp->tree_data.mem_chunks->val[0].v, sz);
	mp->tree_data.cur_buf = mp->tree_data.mem_chunks->val[0].v;
	mp->tree_data.n = 0;
	memset(mp->tree_data.cur_buf, 0, sz * sizeof(mTree));

	while (mpPop_token(mp))
		;
	while (mStack_pop(mp->tree_stack))
		;
	mUs_reset(mp->lex);
}
static mTree *mp_get_leaf(mParser *mp)
{
	if (mp->tree_data.n >= mp->tree_data.sz)
	{
		if (!(mp->tree_data.cur_buf = calloc(mp->tree_data.sz, sizeof(mTree))))
			MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, NULL);

		mp->tree_data.mem_chunks->io_val.v = mp->tree_data.cur_buf;
		if (!mStack_push(mp->tree_data.mem_chunks))
			MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, NULL);
		mp->tree_data.n = 0;
	}
	return mp->tree_data.cur_buf + mp->tree_data.n++;
}

static mAction *mp_get_action(mParser *mp)
{
	assert(mp->lalr_state >= 0 && mp->lalr_state < mp->grm->nlalr);
	mLalr *l = mp->grm->lalr + mp->lalr_state;
	if (l->h)
	{
		l->h->i_key.ull = (mull)mp->symbol;

		if (mhash_get(l->h) == MHSH_OK)
			return l->h->o_val->v;
		else
			return NULL;
	}
	short i;
	for (i = 0; i < l->naction; i++)
		if (l->action[i].SymbolIndex == mp->symbol)
			return l->action + i;
	return NULL;
}
static mTree *mp_push_new_leaf(mParser *mp, mSymbol *sy, const muchar *lex, mTree **chs, short nchs)
{
	mTree *res = mp_get_leaf(mp);
	if (!res)
		return NULL;
	res->symbol.Type = sy->Type;
	res->symbol.Name = sy->Name;
	res->token.id = mp->symbol;
	res->chs = chs;
	res->nchs = nchs;
	res->state = mp->lalr_state;
	res->rule = mp->reduce_rule;
	res->token.lexeme = lex;
	mp->tree_stack->io_val.v = res;
	if (!(mStack_push(mp->tree_stack)))
		MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, NULL);
	return res;
}
static int mp_chset_contains(mParser *mp, mCharSet *chs)
{
#if MP_CODING == MP_UTF8
	if (chs->h)
	{
		strcpy(chs->h->i_key.sbuf, mp->in.out);
		if (mhash_get(chs->h) == MHSH_OK)
			return 1;
		else
			return 0;
	}
	return strstr(chs->chset, mp->in.out) ? 1 : 0;
#elif MP_CODING == MP_UNICODE
	if (chs->h)
	{
		chs->h->i_key.ull = mp->in.out;

		if (mhash_get(chs->h) == MHSH_OK)
			return 1;
		else
			return 0;
	}
	return wcschr(chs->chset, mp->in.out) ? 1 : 0;
#endif
}
static void mp_move_next(mParser *mp)
{
	if (*mp->in.cur == '\n' && mp->in.cur[1])
	{
		mp->in.row_n++;
		mp->in.start_row_pos = mp->in.cur + 1;
	}
	else if (!*mp->in.cur)
	{
		mp->in.MEOF = 1;
		return;
	}

#if MP_CODING == MP_UTF8
	mp->in.cur += mp->in.l_seq;

#elif MP_CODING == MP_UNICODE
	mp->in.cur++;
#endif // MP_CODING
}

static MP_ERRS mp_next_char(mParser *mp)
{
	MP_ERRS res = MP_OK;
	if (*mp->in.cur)
	{

#if MP_CODING == MP_UTF8
		int n_seq = read_utf8_seq(mp->in.cur, mp->in.out);
		if (n_seq < 0)
			MP_RAISE_ERR(mp, MP_NOT_VALID_UTF8_SEQ, MP_ERR_MSG_UTF8_SEQ_NOT_VALID, MP_NOT_VALID_UTF8_SEQ);
		mp->in.l_seq = n_seq;
#elif MP_CODING == MP_UNICODE
		mp->in.out = *mp->in.cur;
#endif // MP_CODING
	}
	else
		mp->in.MEOF = 1;
	return res;
}
static MP_ERRS mp_skip_comment(mParser *mp, int sym_ix)
{
	int i;
	mSymbol *sy;
	const muchar *us;
	const muchar *nl = MP_STR_N;

	for (i = 0; i < mp->grm->ngrp; i++)
		if (mp->grm->grp[i].start_ix == sym_ix)
			break;
	assert(i < mp->grm->ngrp);
	sy = mp->grm->sym + mp->grm->grp[i].end_ix;
	if (ustrcmp(sy->Name, MP_STR_NewLine) == 0)
	{
		i = (int)ustrlen(nl);
		us = ustrstr(mp->in.cur, nl);
	}

	else
	{
		i = (int)ustrlen(sy->Name);
		us = ustrstr(mp->in.cur, sy->Name);
	}

	if (us)
	{
		us += i;
		while (mp->in.cur < us)
		{
			if (*mp->in.cur == '\n' && mp->in.cur[1])
			{
				mp->in.row_n++;
				mp->in.start_row_pos = mp->in.cur + 1;
			}
			mp->in.cur++;
		}
		mp_next_char(mp);
	}
	else
	{
		while (*(mp->in.cur))
			mp->in.cur++;

		mp->in.MEOF = 1;
	}

	return MP_OK;
}

static short mp_scan(mParser *mp)
{
	mDfa *dfa;
	mGram *grm = mp->grm;
	mInput *in = &mp->in;
	mUString *lex = mp->lex;
	int last_accepted = -1;

	dfa = grm->dfa + grm->init_dfa;
	mUs_reset(lex);

	if (mp_next_char(mp) != MP_OK)
		return -1; // it can raise  not valid utf8 seq
	if (mp->in.MEOF)
		return 0;

	while (1)
	{
		int i;
		short nedge;

		short idx;

		if (!in->MEOF)
		{
			nedge = dfa->nedge;
			for (i = 0; i < nedge; i++)
			{
				idx = dfa->edge[i].CharSetIndex;
				if (mp_chset_contains(mp, grm->chset + idx))
				{
					dfa = grm->dfa + dfa->edge[i].TargetIndex;
#if MP_CODING == MP_UTF8
					if (!m8s_concat(lex, in->out, in->l_seq))
#elif MP_CODING == MP_UNICODE
					if (!mU16s_concatcm(lex, in->out))
#endif // MP_CODING
						MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, -1);
					if (dfa->Accept)
						last_accepted = dfa->AcceptIndex;
					else
						last_accepted = -1;
					break;
				}
			}
		}
		if (!in->MEOF && nedge > 0 && i == nedge && last_accepted == -1)
		{
#if MP_CODING == MP_UTF8
			sprintf(_err_buf, MP_ERR_MSG_UNEXPECTED_CHAR, mp->in.cur, mp->in.row_n,
					(int)(1 + (mp->in.cur - mp->in.start_row_pos)));

#elif MP_CODING == MP_UNICODE
			swprintf(_err_buf, (sizeof(_err_buf) / sizeof(*_err_buf)) - 1, MP_ERR_MSG_UNEXPECTED_CHAR, mp->in.cur,
					 mp->in.row_n, (int)(1 + (mp->in.cur - mp->in.start_row_pos)));
#endif
			MP_RAISE_ERR(mp, MP_NOT_VALID_CHAR, _err_buf, -1);
		}
		else if (in->MEOF && last_accepted == -1)
			MP_RAISE_ERR(mp, MP_NOT_VALID_END, MP_ERR_MSG_UNEXPECTED_END, -1);
		else if (in->MEOF || i == nedge)
		{
			if (last_accepted != -1)
			{
				switch (grm->sym[last_accepted].Type)
				{
				case mSyEOF:
					printf("Symbol EOF found\n");
					in->MEOF = 1;
				case mSyNonterminal:
				case mSyTerminal:
					return last_accepted;

				case mSyGroupStart:

					mp_skip_comment(mp, last_accepted);
					mUs_reset(lex);
					if (in->MEOF)
						return 0;
					last_accepted = -1;
					dfa = grm->dfa + grm->init_dfa;
					continue;
				case mSyNoise:
					mUs_reset(lex);
					if (in->MEOF)
						return 0;
					last_accepted = -1;
					dfa = grm->dfa + grm->init_dfa;
					continue;
				case mSyError:
					printf("Symbol Error found\n");
#if MP_CODING == MP_UTF8
					sprintf(_err_buf, MP_ERR_MSG_UNKNOWN_ERR, (int)grm->sym[last_accepted].Type);

#elif MP_CODING == MP_UNICODE
					swprintf(_err_buf, (sizeof(_err_buf) / sizeof(*_err_buf)) - 2, MP_ERR_MSG_UNKNOWN_ERR,
							 (int)grm->sym[last_accepted].Type);

#endif
					MP_RAISE_ERR(mp, MP_UNKNOWN_ERR, _err_buf, -1);

				case mSyDecremented:
				case mSyGroupEnd:
				default:
#if MP_CODING == MP_UTF8
					sprintf(_err_buf, MP_ERR_MSG_UNKNOWN_ERR, (int)grm->sym[last_accepted].Type);
#elif MP_CODING == MP_UNICODE
					swprintf(_err_buf, (sizeof(_err_buf) / sizeof(*_err_buf)) - 2, MP_ERR_MSG_UNKNOWN_ERR,
							 (int)grm->sym[last_accepted].Type);
#endif
					MP_RAISE_ERR(mp, MP_UNKNOWN_ERR, _err_buf, -1);
				}
			}
			else
				break;
		}

		mp_move_next(mp);
		if (mp_next_char(mp) != MP_OK)
			return -1; // it can raise  not valid utf8 seq
	}

	// accept

	return last_accepted;
}

static int mparser_parse(mParser *mp)
{

	mAction *a;
	mSymbol *sy;
	mRule *rl;
	mToken *tk;
	if (mp->reduction)
	{
		mRule *rl = mp->grm->rule + mp->reduce_rule;
		mTree **tl = NULL, **cur = NULL;
		size_t nchs = 0;
		muchar *temp;
		mp->symbol = rl->NonTerminal;
		mpPush_token(mp, mp->symbol, NULL);
		if (rl->nsymbol)
		{
			short new_lalr_state = mp->grm->init_lalr;

			nchs = rl->nsymbol;

			if (nchs && !(tl = calloc(nchs, sizeof(mTree *))))
				MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, -1);
			if (tl)
				cur = tl + nchs - 1;
			while (cur >= tl)
			{
				assert(mStack_pop(mp->tree_stack));
				*cur = mp->tree_stack->io_val.v;
				new_lalr_state = (*cur)->state;
				cur--;
			}
			mp->lalr_state = mp->grm->init_lalr;
			// revert lalr_state
			if (mp->tree_stack->n == 0)
				mp->lalr_state = mp->grm->init_lalr;
			else
				mp->lalr_state = new_lalr_state; // parser->stack[parser->nstack].state;
		}
		sy = mp->grm->sym + mp->symbol;
		if (mp->lex->n)
		{
			if (!(temp = ustrdup(mp->lex->s)))
				MP_RAISE_ERR(mp, MP_RUN_OUT_OF_MEMORY, MP_ERR_MSG_ROOM, -1);
		}
		else
			temp = NULL;

		if (!(mp->out_tree = mp_push_new_leaf(mp, sy, temp, tl, (short)nchs)))
			return -1;
	}
	while (1)
	{
		if (mp->ntokens < 1)
		{
			if ((mp->symbol = mp_scan(mp)) < 0)
				return -1;
			mpPush_token(mp, mp->symbol, mp->lex->n ? mp->lex->s : NULL);
			tk = mp->tokens;
			mp->symbol = tk->id;
		}
		else
		{
			tk = mp->tokens + (mp->ntokens - 1);
			mp->symbol = tk->id;
		}
		a = mp_get_action(mp);
		if (!a && mp->symbol > 0)
		{
#if MP_CODING == MP_UTF8
			sprintf(_err_buf, MP_ERR_MSG_UNEXPECTED_TK, tk->lexeme, mp->in.row_n,
					(int)(1 + (mp->in.cur - mp->in.start_row_pos) - ustrlen(tk->lexeme)));
#elif MP_CODING == MP_UNICODE
			swprintf(_err_buf, (sizeof(_err_buf) / sizeof(*_err_buf)) - 2, MP_ERR_MSG_UNEXPECTED_TK, tk->lexeme,
					 mp->in.row_n, (int)(1 + (mp->in.cur - mp->in.start_row_pos) - ustrlen(tk->lexeme)));
#endif

			MP_RAISE_ERR(mp, MP_NOT_VALID_CHAR, _err_buf, -1);
		}
		else if (!a && mp->symbol == 0)
		{
			MP_RAISE_ERR(mp, MP_NOT_VALID_END, MP_ERR_MSG_UNEXPECTED_END, -1);
		}
		else if (!a)
			assert(0);

		switch (a->Action)
		{
		case ActionShift:
			if (mp->symbol < 0 || mp->symbol >= mp->grm->nsym)
			{
				printf("Symbol %d not known\n", (int)mp->symbol);
				break;
			}
			sy = mp->grm->sym + mp->symbol;
			if (!mp_push_new_leaf(mp, sy, tk->lexeme, NULL, 0))
				return -1;
			assert(mp->ntokens);
			mp->tokens[--mp->ntokens].lexeme = NULL;
			mp->lalr_state = a->Target;
			break;
		case ActionReduce:
			assert(a->Target >= 0 && a->Target < mp->grm->nrule);
			rl = mp->grm->rule + a->Target;
			mUs_reset(mp->lex);
			mp->reduce_rule = a->Target;

			mp->reduction = 1;
			return rl->NonTerminal;
		case ActionGoto:
			mp->lalr_state = a->Target;
			mpPop_token(mp);
			break;
		case ActionAccept:
			return 0;
			break;
		}
	}

	return -1;
}

#if MP_CODING == MP_UTF8
MP_ERRS mpExec(mParser *mp, const char *exp)
#elif MP_CODING == MP_UNICODE
MP_ERRS mpExec(mParser *mp, const wchar_t *exp)
#endif
{
	int p;
	if (!(exp && *exp))
	{
		mp->err_msg = NULL;
		mp->out_tree = NULL;
		return mp->err_n = MP_OK;
	}
	mpReset(mp);
	mp->in.start_row_pos = mp->in.cur = mp->in.exp = exp;
	mp->in.exp_len = ustrlen(exp);
	do
	{
		p = mparser_parse(mp);
	} while (p > 0);
	return mp->err_n;
}
