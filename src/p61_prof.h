#pragma once
// P6.1 measurement hooks. Compile-time switch only: without -DJ2ME_P61_PROF (the default for every
// production/desktop/Switch build) every macro expands to nothing, so default binaries are unchanged.
// With it, the including test TU must define p61_op / p61_scope_enter / p61_scope_leave.
#if defined(J2ME_P61_PROF)
struct Method;
static void p61_op(const Method* m, int pc, unsigned op);
static int p61_scope_enter(int state, const void* who);
static void p61_scope_leave(int token);
struct P61Scope { int t; P61Scope(int s, const void* w) : t(p61_scope_enter(s, w)) {} ~P61Scope() { p61_scope_leave(t); } };
#define P61_OP(m, pc, op) p61_op((m), (pc), (op))
#define P61_SCOPE(state, who) P61Scope p61_scope_guard_((state), (who))
#else
#define P61_OP(m, pc, op) ((void)0)
#define P61_SCOPE(state, who) ((void)0)
#endif
