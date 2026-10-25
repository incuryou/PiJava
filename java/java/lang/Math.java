package java.lang;
public final class Math {
    private Math() {}
    public static final double E = 2.7182818284590452354;
    public static final double PI = 3.14159265358979323846;
    public static int abs(int a) { return a < 0 ? -a : a; }
    public static long abs(long a) { return a < 0 ? -a : a; }
    public static float abs(float a) { return Float.intBitsToFloat(Float.floatToIntBits(a) & 0x7fffffff); }
    public static double abs(double a) { return Double.longBitsToDouble(Double.doubleToLongBits(a) & 0x7fffffffffffffffL); }
    public static int max(int a, int b) { return a > b ? a : b; }
    public static int min(int a, int b) { return a < b ? a : b; }
    public static long max(long a, long b) { return a > b ? a : b; }
    public static long min(long a, long b) { return a < b ? a : b; }
    public static float max(float a, float b) {
        if (a != a) return a;
        if (a == 0 && b == 0 && Float.floatToIntBits(a) == 0x80000000) return b;
        return a >= b ? a : b;
    }
    public static float min(float a, float b) {
        if (a != a) return a;
        if (a == 0 && b == 0 && Float.floatToIntBits(b) == 0x80000000) return b;
        return a <= b ? a : b;
    }
    public static double max(double a, double b) {
        if (a != a) return a;
        if (a == 0 && b == 0 && Double.doubleToLongBits(a) == 0x8000000000000000L) return b;
        return a >= b ? a : b;
    }
    public static double min(double a, double b) {
        if (a != a) return a;
        if (a == 0 && b == 0 && Double.doubleToLongBits(b) == 0x8000000000000000L) return b;
        return a <= b ? a : b;
    }
    // Existing extension: use a double intermediate so 0.49999997f stays below 0.5.
    public static int round(float a) { return (int) floor((double) a + 0.5); }
    public static native double pow(double a, double b);
    public static native double sqrt(double a);
    public static native double sin(double a);
    public static native double cos(double a);
    public static native double tan(double a);
    public static native double ceil(double a);
    public static native double floor(double a);
    public static double toRadians(double a) { return a / 180.0 * PI; }
    public static double toDegrees(double a) { return a / PI * 180.0; }
}
