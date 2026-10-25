package java.lang;
public final class Character {
    public static boolean isLowerCase(char c) { return c>='a'&&c<='z' || c>=0x00df&&c<=0x00f6 || c>=0x00f8&&c<=0x00ff; }
    public static boolean isUpperCase(char c) { return c>='A'&&c<='Z' || c>=0x00c0&&c<=0x00d6 || c>=0x00d8&&c<=0x00de; }
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;
    public static final char MIN_VALUE = '\u0000';
    public static final char MAX_VALUE = '\uffff';
    private final char value;
    public Character(char value) { this.value = value; }
    public char charValue() { return value; }
    public boolean equals(Object other) { return other instanceof Character && ((Character) other).value == value; }
    public int hashCode() { return value; }
    public String toString() { return new String(new char[] {value}); }
    public static boolean isDigit(char ch) { return ch >= 48 && ch <= 57; }
    public static boolean isLetter(char ch) {
        return (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122);
    }
    public static char toLowerCase(char ch) { return (ch >= 65 && ch <= 90) ? (char) (ch + 32) : ch; }
    public static char toUpperCase(char ch) { return (ch >= 97 && ch <= 122) ? (char) (ch - 32) : ch; }
    public static int digit(char ch, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) return -1;
        int v = -1;
        if (ch >= 48 && ch <= 57) v = ch - 48;
        else if (ch >= 97 && ch <= 122) v = ch - 97 + 10;
        else if (ch >= 65 && ch <= 90) v = ch - 65 + 10;
        if (v < 0 || v >= radix) return -1;
        return v;
    }
}
