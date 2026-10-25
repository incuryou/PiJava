package java.lang;

public final class Integer {
    public static final int MAX_VALUE = 2147483647;
    public static final int MIN_VALUE = -2147483648;
    private final int value;
    public Integer(int value) { this.value = value; }
    public Integer(String text) throws NumberFormatException { this.value = parseInt(text); }
    public byte byteValue() { return (byte) value; }
    public short shortValue() { return (short) value; }
    public int intValue() { return value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }
    public static Integer valueOf(int value) { return new Integer(value); }
    public static Integer valueOf(String text) throws NumberFormatException { return new Integer(parseInt(text)); }
    public static Integer valueOf(String text, int radix) throws NumberFormatException { return new Integer(parseInt(text, radix)); }
    public static int parseInt(String text) throws NumberFormatException { return parseInt(text, 10); }
    public static int parseInt(String text, int radix) throws NumberFormatException {
        if (text == null || radix < 2 || radix > 36 || text.length() == 0)
            throw new NumberFormatException("Invalid integer");
        int index = 0;
        char first = text.charAt(0);
        boolean negative = first == '-';
        if (negative || first == '+') index++;
        if (index == text.length()) throw new NumberFormatException("Missing integer digits");
        // Accumulate negatively so MIN_VALUE does not need a positive counterpart.
        int limit = negative ? MIN_VALUE : -MAX_VALUE;
        int minimumBeforeMultiply = limit / radix;
        int result = 0;
        while (index < text.length()) {
            int digit = Character.digit(text.charAt(index++), radix);
            if (digit < 0 || result < minimumBeforeMultiply)
                throw new NumberFormatException("Invalid integer or overflow");
            result *= radix;
            if (result < limit + digit) throw new NumberFormatException("Integer overflow");
            result -= digit;
        }
        return negative ? result : -result;
    }
    private static char digitChar(int digit) { return (char) (digit < 10 ? '0' + digit : 'a' + digit - 10); }
    public static String toString(int value) { return toString(value, 10); }
    public static String toString(int value, int radix) {
        if (radix < 2 || radix > 36) radix = 10;
        char[] chars = new char[33];
        int pos = chars.length;
        boolean negative = value < 0;
        int remaining = negative ? value : -value;
        do {
            chars[--pos] = digitChar(-(remaining % radix));
            remaining /= radix;
        } while (remaining != 0);
        if (negative) chars[--pos] = '-';
        return new String(chars, pos, chars.length - pos);
    }
    private static String unsignedString(int value, int shift) {
        char[] chars = new char[32];
        int pos = chars.length;
        int mask = (1 << shift) - 1;
        do {
            chars[--pos] = digitChar(value & mask);
            value >>>= shift;
        } while (value != 0);
        return new String(chars, pos, chars.length - pos);
    }
    public static String toHexString(int value) { return unsignedString(value, 4); }
    public static String toOctalString(int value) { return unsignedString(value, 3); }
    public static String toBinaryString(int value) { return unsignedString(value, 1); }
    public boolean equals(Object other) { return other instanceof Integer && ((Integer) other).value == value; }
    public int hashCode() { return value; }
    public String toString() { return toString(value); }
}
