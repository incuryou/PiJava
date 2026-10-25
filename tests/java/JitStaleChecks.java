// 0.2.113/0.2.114: fixtures for JitRegionStaleGuard / JitNegativeDepthGlue / JitBadExitSideEffects / JitLastExitReset / JitStaleLogLimit (compiled-region stale-code guard).
public class JitStaleChecks {
    public static int sum(int n) { int s = 0; for (int i = 0; i < n; i++) { s += i; } return s; }
    public static int other(int n) { int a = n; int b = 0; while (a > 0) { b += a; a--; } return b; }
    // 0.2.114: read-modify-write int[] loop (region with iastore = side effects) for JitBadExitSideEffects
    public static int bump(int[] a, int n) { for (int i = 0; i < n; i++) { a[i] = a[i] + 1; } return n; }   // no dup2: the whole loop body compiles
}
