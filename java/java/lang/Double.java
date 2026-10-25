package java.lang;
public final class Double {
    public Double(double value) { _set(value); }
    public Double(String value) { _set(parseDouble(value)); }
    private native void _set(double value);
    public native double doubleValue();
    public int intValue() { return (int)doubleValue(); }
    public static native double parseDouble(String value);
    public static Double valueOf(String value) { return new Double(parseDouble(value)); }
    public static boolean isNaN(double value) { return value!=value; }
    public static boolean isInfinite(double value) { return value==1.0/0.0 || value==-1.0/0.0; }
    public boolean isNaN() { return isNaN(doubleValue()); }
    public boolean isInfinite() { return isInfinite(doubleValue()); }
    public String toString() { return String.valueOf(doubleValue()); }
    public static native long doubleToRawLongBits(double value);
    public static native long doubleToLongBits(double value);
    public static native double longBitsToDouble(long bits);
}
