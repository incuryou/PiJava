package java.util;

/** Java's portable 48-bit linear congruential pseudorandom generator. */
public class Random {
    private long seed;
    private static final long MULTIPLIER = 0x5DEECE66DL;
    private static final long MASK = (1L << 48) - 1;

    public Random() { this(System.currentTimeMillis()); }
    public Random(long seed) { setSeed(seed); }
    public synchronized void setSeed(long seed) {
        this.seed = (seed ^ MULTIPLIER) & MASK;
    }
    protected synchronized int next(int bits) {
        seed = (seed * MULTIPLIER + 0xBL) & MASK;
        return (int) (seed >>> (48 - bits));
    }
    public int nextInt() { return next(32); }
    public int nextInt(int bound) {
        if (bound <= 0) throw new IllegalArgumentException("bound must be positive");
        if ((bound & -bound) == bound)
            return (int) ((bound * (long) next(31)) >> 31);
        int bits;
        int value;
        do {
            bits = next(31);
            value = bits % bound;
        } while (bits - value + (bound - 1) < 0);
        return value;
    }
    public long nextLong() { return ((long) next(32) << 32) + next(32); }
    public float nextFloat() { return next(24) / 16777216.0f; }
    public double nextDouble() {
        return (((long) next(26) << 27) + next(27)) / 9007199254740992.0;
    }
    /** Compatibility extension; bytes consume the low eight bits of each int first. */
    public void nextBytes(byte[] bytes) {
        int index = 0;
        while (index < bytes.length) {
            int value = nextInt();
            for (int count = 0; count < 4 && index < bytes.length; count++) {
                bytes[index++] = (byte) value;
                value >>= 8;
            }
        }
    }
}
