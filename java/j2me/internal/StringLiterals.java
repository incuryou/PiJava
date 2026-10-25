package j2me.internal;

/** VM-owned constant-pool roots. Ordinary statics/arrays are GC and J2S5-visible. */
public final class StringLiterals {
    private static String[] roots;
    private StringLiterals() {}
}
