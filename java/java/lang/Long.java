package java.lang;

/** CLDC long wrapper and signed radix conversions. */
public final class Long {
    public static final long MIN_VALUE = -9223372036854775808L;
    public static final long MAX_VALUE = 9223372036854775807L;
    private final long value;

    public Long(long value) { this.value = value; }
    public long longValue() { return value; }
    public float floatValue() { return (float) value; }
    public double doubleValue() { return (double) value; }
    /** Retained compatibility extension. */
    public static Long valueOf(long value) { return new Long(value); }

    public static String toString(long value) { return toString(value, 10); }
    public static String toString(long value, int radix) {
        if (radix < 2 || radix > 36) radix = 10;
        char[] buffer = new char[65];
        int position = buffer.length;
        boolean negative = value < 0;
        // Work with a negative magnitude so MIN_VALUE never needs negation.
        long remaining = negative ? value : -value;
        do {
            int digit = (int) -(remaining % radix);
            buffer[--position] = (char) (digit < 10 ? '0' + digit : 'a' + digit - 10);
            remaining /= radix;
        } while (remaining != 0);
        if (negative) buffer[--position] = '-';
        return new String(buffer, position, buffer.length - position);
    }

    public static long parseLong(String text) { return parseLong(text, 10); }
    public static long parseLong(String text, int radix) {
        if (text == null || text.length() == 0 || radix < 2 || radix > 36)
            throw new NumberFormatException("invalid long");
        boolean negative = text.charAt(0) == '-';
        int index = negative ? 1 : 0;
        if (index == text.length()) throw new NumberFormatException("invalid long");
        long limit = negative ? MIN_VALUE : -MAX_VALUE;
        long multiplyLimit = limit / radix;
        long result = 0;
        while (index < text.length()) {
            int digit = Character.digit(text.charAt(index++), radix);
            if (digit < 0 || result < multiplyLimit)
                throw new NumberFormatException("invalid long");
            result *= radix;
            if (result < limit + digit) throw new NumberFormatException("long overflow");
            result -= digit;
        }
        return negative ? result : -result;
    }

    public String toString() { return toString(value); }
    public boolean equals(Object other) {
        return other instanceof Long && value == ((Long) other).value;
    }
    public int hashCode() { return (int) (value ^ (value >>> 32)); }
}
