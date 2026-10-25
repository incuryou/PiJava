package java.lang;
public final class Float {
    public Float(float value) { _set(value); }
    public Float(String value) { _set(parseFloat(value)); }
    private native void _set(float value);
    public native float floatValue();
    public static native float parseFloat(String value);
    public static Float valueOf(String value) { return new Float(parseFloat(value)); }
    public static boolean isNaN(float value) { return value!=value; }
    public static boolean isInfinite(float value) { return value==1.0f/0.0f || value==-1.0f/0.0f; }
    public boolean isNaN() { return isNaN(floatValue()); }
    public boolean isInfinite() { return isInfinite(floatValue()); }
    public String toString() { return String.valueOf(floatValue()); }
    public static native int floatToRawIntBits(float value);
    public static native int floatToIntBits(float value);
    public static native float intBitsToFloat(int bits);
}
