package java.lang;
public final class Short {
    public static final short MIN_VALUE = -32768;
    public static final short MAX_VALUE = 32767;
    short value;
    public Short(short value) { this.value = value; }
    public short shortValue() { return value; }
    public boolean equals(Object other) {
        return other instanceof Short && ((Short)other).value == value;
    }
    public int hashCode() { return (int)value; }
    public static short parseShort(String text) { return parseShort(text,10); }
    public static short parseShort(String text,int radix) {
        int n=Integer.parseInt(text,radix);
        if(n<-32768 || n>32767)throw new NumberFormatException(text);
        return (short)n;
    }
    public String toString() { return Integer.toString(value); }
}
