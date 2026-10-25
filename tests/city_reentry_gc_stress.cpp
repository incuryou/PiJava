// P2.2-S5 A2 on a real game: city_extended_repro unchanged, plus the test-only hook that forces a full GC at every
// run() entry (nested re-entry included) and RootScope stale verification. Run with J2ME_GC_STRESS=N as well to
// force collections at the host safepoint too. Prints S5REENTRY_STRESS forced=.. nested=.. stale=.. at exit.
#include <cstdio>
#include <cstdlib>
static unsigned long long g_s5_forced=0, g_s5_nested=0;
static void s5_test_reentry_gc();
#define J2ME_TEST_REENTRY_GC_HOOK() s5_test_reentry_gc()
#define J2ME_TEST_ROOTSCOPE_VERIFY 1
#include "city_extended_repro.cpp"
static void s5_test_reentry_gc(){ ++g_s5_forced; if(VMS_EXEC(g_run_depth)>0)++g_s5_nested; gc_collect("s5-stress"); gc_verify_frames("forced"); } // P2.4: verify active frames after every forced GC
static struct S5Exit{ ~S5Exit(){ std::printf("S5REENTRY_STRESS forced=%llu nested=%llu stale_at_close=%llu scopes=%llu residue_cleared=%llu verify_runs=%llu verify_refs=%llu verify_dead=%llu\n",
    g_s5_forced,g_s5_nested,(unsigned long long)g_native_roots.stale_at_close,(unsigned long long)g_native_roots.scopes,(unsigned long long)g_native_roots.residue_cleared,(unsigned long long)g_gc_verify.runs,(unsigned long long)g_gc_verify.refs,(unsigned long long)g_gc_verify.dead); std::fflush(stdout);} } g_s5_exit;
