#include "js.h"
#include "jsparse.h"
#include "jscompile.h"
#include "jsrun.h"

#include <assert.h>

static const char *astname[] = {
	"list", "ident", "number", "string", "regexp", "undef", "null", "true",
	"false", "this", "array", "object", "prop_val", "prop_get", "prop_set",
	"index", "member", "call", "new", "funexp", "delete", "void", "typeof",
	"preinc", "predec", "postinc", "postdec", "pos", "neg", "bitnot",
	"lognot", "logor", "logand", "bitor", "bitxor", "bitand", "eq", "ne",
	"eq3", "ne3", "lt", "gt", "le", "ge", "instanceof", "in", "shl", "shr",
	"ushr", "add", "sub", "mul", "div", "mod", "cond", "ass", "ass_mul",
	"ass_div", "ass_mod", "ass_add", "ass_sub", "ass_shl", "ass_shr",
	"ass_ushr", "ass_bitand", "ass_bitxor", "ass_bitor", "comma",
	"var-init", "block", "fundec", "nop", "var", "if", "do-while", "while",
	"for", "for-var", "for-in", "for-in-var", "continue", "break",
	"return", "with", "switch", "throw", "try", "debugger", "label",
	"case", "default",
};

static const char *opname[] = {
#include "opnames.h"
};

static void pstmlist(int d, js_Ast *list);
static void pexpi(int d, int i, js_Ast *exp);
static void pstm(int d, js_Ast *stm);
static void slist(int d, js_Ast *list);
static void sblock(int d, js_Ast *list);

static inline void pc(int c)
{
	putchar(c);
}

static inline void ps(const char *s)
{
	fputs(s, stdout);
}

static inline void in(int d)
{
	while (d-- > 0)
		putchar('\t');
}

static inline void nl(void)
{
	putchar('\n');
}

static void pargs(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pexpi(d, 0, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
}

static void parray(int d, js_Ast *list)
{
	ps("[");
	while (list) {
		assert(list->type == AST_LIST);
		pexpi(d, 0, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
	ps("]");
}

static void pobject(int d, js_Ast *list)
{
	ps("{");
	while (list) {
		js_Ast *kv = list->a;
		assert(list->type == AST_LIST);
		switch (kv->type) {
		case EXP_PROP_VAL:
			pexpi(d, 0, kv->a);
			ps(": ");
			pexpi(d, 0, kv->b);
			break;
		case EXP_PROP_GET:
			ps("get ");
			pexpi(d, 0, kv->a);
			ps("() {\n");
			pstmlist(d, kv->b);
			in(d); ps("}");
			break;
		case EXP_PROP_SET:
			ps("set ");
			pexpi(d, 0, kv->a);
			ps("(");
			pexpi(d, 0, kv->b);
			ps(") {\n");
			pstmlist(d, kv->c);
			in(d); ps("}");
			break;
		}
		list = list->b;
		if (list)
			ps(", ");
	}
	ps("}");
}

static void pstr(const char *s)
{
	int c;
	pc('"');
	while ((c = *s++)) {
		switch (c) {
		case '"': ps("\\\""); break;
		case '\\': ps("\\\\"); break;
		case '\n': ps("\\n"); break;
		default: pc(c); break;
		}
	}
	pc('"');
}

static void pregexp(const char *prog, int flags)
{
	pc('/');
	ps(prog);
	pc('/');
	if (flags & JS_REGEXP_G) pc('g');
	if (flags & JS_REGEXP_I) pc('i');
	if (flags & JS_REGEXP_M) pc('m');
}

static void pbin(int d, int i, js_Ast *exp, const char *op)
{
	if (i) pc('(');
	pexpi(d, 1, exp->a);
	ps(op);
	pexpi(d, 1, exp->b);
	if (i) pc(')');
}

static void puna(int d, int i, js_Ast *exp, const char *pre, const char *suf)
{
	if (i) pc('(');
	ps(pre);
	pexpi(d, 1, exp->a);
	ps(suf);
	if (i) pc(')');
}

static void pexpi(int d, int i, js_Ast *exp)
{
	switch (exp->type) {
	case AST_IDENTIFIER: ps(exp->string); break;
	case AST_NUMBER: printf("%.9g", exp->number); break;
	case AST_STRING: pstr(exp->string); break;
	case AST_REGEXP: pregexp(exp->string, exp->number); break;

	case EXP_UNDEF: break;
	case EXP_NULL: ps("null"); break;
	case EXP_TRUE: ps("true"); break;
	case EXP_FALSE: ps("false"); break;
	case EXP_THIS: ps("this"); break;

	case EXP_OBJECT: pobject(d, exp->a); break;
	case EXP_ARRAY: parray(d, exp->a); break;

	case EXP_DELETE: puna(d, i, exp, "delete ", ""); break;
	case EXP_VOID: puna(d, i, exp, "void ", ""); break;
	case EXP_TYPEOF: puna(d, i, exp, "typeof ", ""); break;
	case EXP_PREINC: puna(d, i, exp, "++", ""); break;
	case EXP_PREDEC: puna(d, i, exp, "--", ""); break;
	case EXP_POSTINC: puna(d, i, exp, "", "++"); break;
	case EXP_POSTDEC: puna(d, i, exp, "", "--"); break;
	case EXP_POS: puna(d, i, exp, "+", ""); break;
	case EXP_NEG: puna(d, i, exp, "-", ""); break;
	case EXP_BITNOT: puna(d, i, exp, "~", ""); break;
	case EXP_LOGNOT: puna(d, i, exp, "!", ""); break;

	case EXP_LOGOR: pbin(d, i, exp, " || "); break;
	case EXP_LOGAND: pbin(d, i, exp, " && "); break;
	case EXP_BITOR: pbin(d, i, exp, " | "); break;
	case EXP_BITXOR: pbin(d, i, exp, " ^ "); break;
	case EXP_BITAND: pbin(d, i, exp, " & "); break;
	case EXP_EQ: pbin(d, i, exp, " == "); break;
	case EXP_NE: pbin(d, i, exp, " != "); break;
	case EXP_EQ3: pbin(d, i, exp, " === "); break;
	case EXP_NE3: pbin(d, i, exp, " !== "); break;
	case EXP_LT: pbin(d, i, exp, " < "); break;
	case EXP_GT: pbin(d, i, exp, " > "); break;
	case EXP_LE: pbin(d, i, exp, " <= "); break;
	case EXP_GE: pbin(d, i, exp, " >= "); break;
	case EXP_INSTANCEOF: pbin(d, i, exp, " instanceof "); break;
	case EXP_IN: pbin(d, i, exp, " in "); break;
	case EXP_SHL: pbin(d, i, exp, " << "); break;
	case EXP_SHR: pbin(d, i, exp, " >> "); break;
	case EXP_USHR: pbin(d, i, exp, " >>> "); break;
	case EXP_ADD: pbin(d, i, exp, " + "); break;
	case EXP_SUB: pbin(d, i, exp, " - "); break;
	case EXP_MUL: pbin(d, i, exp, " * "); break;
	case EXP_DIV: pbin(d, i, exp, " / "); break;
	case EXP_MOD: pbin(d, i, exp, " % "); break;
	case EXP_ASS: pbin(d, i, exp, " = "); break;
	case EXP_ASS_MUL: pbin(d, i, exp, " *= "); break;
	case EXP_ASS_DIV: pbin(d, i, exp, " /= "); break;
	case EXP_ASS_MOD: pbin(d, i, exp, " %= "); break;
	case EXP_ASS_ADD: pbin(d, i, exp, " += "); break;
	case EXP_ASS_SUB: pbin(d, i, exp, " -= "); break;
	case EXP_ASS_SHL: pbin(d, i, exp, " <<= "); break;
	case EXP_ASS_SHR: pbin(d, i, exp, " >>= "); break;
	case EXP_ASS_USHR: pbin(d, i, exp, " >>>= "); break;
	case EXP_ASS_BITAND: pbin(d, i, exp, " &= "); break;
	case EXP_ASS_BITXOR: pbin(d, i, exp, " ^= "); break;
	case EXP_ASS_BITOR: pbin(d, i, exp, " |= "); break;

	case EXP_COMMA: pbin(d, 1, exp, ", "); break;

	case EXP_COND:
		if (i) pc('(');
		pexpi(d, 1, exp->a);
		ps(" ? ");
		pexpi(d, 1, exp->b);
		ps(" : ");
		pexpi(d, 1, exp->c);
		if (i) pc(')');
		break;

	case EXP_INDEX:
		if (i) pc('(');
		pexpi(d, 1, exp->a);
		pc('[');
		pexpi(d, 0, exp->b);
		pc(']');
		if (i) pc(')');
		break;

	case EXP_MEMBER:
		if (i) pc('(');
		pexpi(d, 1, exp->a);
		pc('.');
		pexpi(d, 1, exp->b);
		if (i) pc(')');
		break;

	case EXP_CALL:
		if (i) pc('(');
		pexpi(d, 1, exp->a);
		pc('(');
		pargs(d, exp->b);
		pc(')');
		if (i) pc(')');
		break;

	case EXP_NEW:
		if (i) pc('(');
		ps("new ");
		pexpi(d, 1, exp->a);
		pc('(');
		pargs(d, exp->b);
		pc(')');
		if (i) pc(')');
		break;

	case EXP_FUNC:
		ps("(function ");
		if (exp->a) pexpi(d, 1, exp->a);
		pc('(');
		pargs(d, exp->b);
		ps(") {\n");
		pstmlist(d, exp->c);
		in(d); ps("})");
		break;

	default:
		ps("<UNKNOWN>");
		break;
	}
}

static void pexp(int d, js_Ast *exp)
{
	pexpi(d, 0, exp);
}

static void pvar(int d, js_Ast *var)
{
	assert(var->type == EXP_VAR);
	pexp(d, var->a);
	if (var->b) {
		ps(" = ");
		pexp(d, var->b);
	}
}

static void pvarlist(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pvar(d, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
}

static void pblock(int d, js_Ast *block)
{
	assert(block->type == STM_BLOCK);
	in(d); ps("{\n");
	pstmlist(d, block->a);
	in(d); pc('}');
}

static void pstmh(int d, js_Ast *stm)
{
	if (stm->type == STM_BLOCK)
		pblock(d, stm);
	else
		pstm(d+1, stm);
}

static void pcaselist(int d, js_Ast *list)
{
	while (list) {
		js_Ast *stm = list->a;
		if (stm->type == STM_CASE) {
			in(d); ps("case "); pexp(d, stm->a); ps(":\n");
			pstmlist(d, stm->b);
		}
		if (stm->type == STM_DEFAULT) {
			in(d); ps("default:\n");
			pstmlist(d, stm->a);
		}
		list = list->b;
	}
}

static void pstm(int d, js_Ast *stm)
{
	if (stm->type == STM_BLOCK) {
		pblock(d, stm);
		return;
	}

	in(d);

	switch (stm->type) {
	case STM_FUNC:
		ps("function ");
		pexpi(d, 1, stm->a);
		pc('(');
		pargs(d, stm->b);
		ps(")\n");
		in(d); ps("{\n");
		pstmlist(d, stm->c);
		in(d); ps("}");
		break;

	case STM_NOP:
		pc(';');
		break;

	case STM_VAR:
		ps("var ");
		pvarlist(d, stm->a);
		ps(";");
		break;

	case STM_IF:
		ps("if ("); pexp(d, stm->a); ps(")\n");
		pstmh(d, stm->b);
		if (stm->c) {
			nl(); in(d); ps("else\n");
			pstmh(d, stm->c);
		}
		break;

	case STM_DO:
		ps("do\n");
		pstmh(d, stm->a);
		nl();
		in(d); ps("while ("); pexp(d, stm->b); ps(");");
		break;

	case STM_WHILE:
		ps("while ("); pexp(d, stm->a); ps(")\n");
		pstmh(d, stm->b);
		break;

	case STM_FOR:
		ps("for (");
		pexp(d, stm->a); ps("; ");
		pexp(d, stm->b); ps("; ");
		pexp(d, stm->c); ps(")\n");
		pstmh(d, stm->d);
		break;
	case STM_FOR_VAR:
		ps("for (var ");
		pvarlist(d, stm->a); ps("; ");
		pexp(d, stm->b); ps("; ");
		pexp(d, stm->c); ps(")\n");
		pstmh(d, stm->d);
		break;
	case STM_FOR_IN:
		ps("for (");
		pexp(d, stm->a); ps(" in ");
		pexp(d, stm->b); ps(")\n");
		pstmh(d, stm->c);
		break;
	case STM_FOR_IN_VAR:
		ps("for (var ");
		pvarlist(d, stm->a); ps(" in ");
		pexp(d, stm->b); ps(")\n");
		pstmh(d, stm->c);
		break;

	case STM_CONTINUE:
		if (stm->a) {
			ps("continue "); pexp(d, stm->a); ps(";");
		} else {
			ps("continue;");
		}
		break;

	case STM_BREAK:
		if (stm->a) {
			ps("break "); pexp(d, stm->a); ps(";");
		} else {
			ps("break;");
		}
		break;

	case STM_RETURN:
		if (stm->a) {
			ps("return "); pexp(d, stm->a); ps(";");
		} else {
			ps("return;");
		}
		break;

	case STM_WITH:
		ps("with ("); pexp(d, stm->a); ps(")\n");
		pstm(d, stm->b);
		break;

	case STM_SWITCH:
		ps("switch (");
		pexp(d, stm->a);
		ps(")\n");
		in(d); ps("{\n");
		pcaselist(d, stm->b);
		in(d); ps("}");
		break;

	case STM_THROW:
		ps("throw "); pexp(d, stm->a); ps(";");
		break;

	case STM_TRY:
		ps("try\n");
		pstmh(d, stm->a);
		if (stm->b && stm->c) {
			nl(); in(d); ps("catch ("); pexp(d, stm->b); ps(")\n");
			pstmh(d, stm->c);
		}
		if (stm->d) {
			nl(); in(d); ps("finally\n");
			pstmh(d, stm->d);
		}
		break;

	case STM_LABEL:
		pexp(d, stm->a); ps(": "); pstm(d, stm->b);
		break;

	case STM_DEBUGGER:
		ps("debugger;");
		break;

	default:
		pexp(d, stm); pc(';');
	}
}

static void pstmlist(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pstm(d+1, list->a);
		nl();
		list = list->b;
	}
}

void jsP_dumpsyntax(js_State *J, js_Ast *prog)
{
	if (prog->type == AST_LIST)
		pstmlist(-1, prog);
	else {
		pstm(0, prog);
		nl();
	}
}

static void snode(int d, js_Ast *node)
{
	void (*afun)(int,js_Ast*) = snode;
	void (*bfun)(int,js_Ast*) = snode;
	void (*cfun)(int,js_Ast*) = snode;
	void (*dfun)(int,js_Ast*) = snode;

	if (!node) {
		return;
	}

	if (node->type == AST_LIST) {
		slist(d, node);
		return;
	}

	pc('(');
	ps(astname[node->type]);
	switch (node->type) {
	case AST_IDENTIFIER: pc(' '); ps(node->string); break;
	case AST_STRING: pc(' '); pstr(node->string); break;
	case AST_REGEXP: pc(' '); pregexp(node->string, node->number); break;
	case AST_NUMBER: printf(" %.9g", node->number); break;
	case STM_BLOCK: afun = sblock; break;
	case STM_FUNC: case EXP_FUNC: cfun = sblock; break;
	case STM_SWITCH: bfun = sblock; break;
	case STM_CASE: bfun = sblock; break;
	case STM_DEFAULT: afun = sblock; break;
	}
	if (node->a) { pc(' '); afun(d, node->a); }
	if (node->b) { pc(' '); bfun(d, node->b); }
	if (node->c) { pc(' '); cfun(d, node->c); }
	if (node->d) { pc(' '); dfun(d, node->d); }
	pc(')');
}

static void slist(int d, js_Ast *list)
{
	pc('[');
	while (list) {
		assert(list->type == AST_LIST);
		snode(d, list->a);
		list = list->b;
		if (list)
			pc(' ');
	}
	pc(']');
}

static void sblock(int d, js_Ast *list)
{
	ps("[\n");
	in(d+1);
	while (list) {
		assert(list->type == AST_LIST);
		snode(d+1, list->a);
		list = list->b;
		if (list) {
			nl();
			in(d+1);
		}
	}
	nl(); in(d); pc(']');
}

void jsP_dumplist(js_State *J, js_Ast *prog)
{
	if (prog->type == AST_LIST)
		sblock(0, prog);
	else
		snode(0, prog);
	nl();
}

void jsC_dumpvalue(js_State *J, js_Value v)
{
	switch (v.type) {
	case JS_TUNDEFINED: ps("undefined"); break;
	case JS_TNULL: ps("null"); break;
	case JS_TBOOLEAN: ps(v.u.boolean ? "true" : "false"); break;
	case JS_TNUMBER: printf("%.9g", v.u.number); break;
	case JS_TSTRING: pstr(v.u.string); break;
	case JS_TREGEXP: printf("<regexp %p>", v.u.p); break;
	case JS_TOBJECT: printf("<object %p>", v.u.p); break;

	case JS_TFUNCTION: printf("<function %p>", v.u.p); break;
	case JS_TCFUNCTION: printf("<cfunction %p>", v.u.p); break;
	case JS_TCLOSURE: printf("<closure %p>", v.u.p); break;
	case JS_TARGUMENTS: printf("<arguments %p>", v.u.p); break;

	case JS_TOBJSLOT: printf("<objslot %p>", v.u.p); break;
	}
}

void jsC_dumpfunction(js_State *J, js_Function *fun)
{
	unsigned char *p = fun->code;
	unsigned char *end = fun->code + fun->len;
	int dest;

	printf("function with %d constants\n", fun->klen);

	while (p < end) {
		int c = *p++;

		printf("%04d: ", (int)(p - fun->code) - 1);
		ps(opname[c]);

		switch (c) {
		case OP_CONST:
		case OP_OBJECTPUT:
		case OP_DEFVAR:
		case OP_VAR:
		case OP_MEMBER:
			pc(' ');
			jsC_dumpvalue(J, fun->klist[*p++]);
			break;
		case OP_CALL:
		case OP_NEW:
			printf(" %d", *p++);
			break;
		case OP_JUMP:
		case OP_JTRUE:
		case OP_JFALSE:
			dest = (*p++) << 8;
			dest += (*p++);
			printf(" %d", dest);
			break;
		}

		nl();
	}
}
