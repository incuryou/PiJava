package java.lang;
public final class Byte {
    public static final byte MIN_VALUE = -128;
    public static final byte MAX_VALUE = 127;
    byte value;
    public Byte(byte value) { this.value = value; }
    public byte byteValue() { return value; }
    public static byte parseByte(String text) { return parseByte(text,10); }
    public static byte parseByte(String text,int radix) {
        int n=Integer.parseInt(text,radix);
        if(n<-128 || n>127)throw new NumberFormatException(text);
        return (byte)n;
    }
    public String toString() { return Integer.toString(value); }
}
