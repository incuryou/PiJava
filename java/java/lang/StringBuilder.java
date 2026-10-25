package java.lang;
public class StringBuilder {
    // Native mutators grow this field after a successful length increase.
    private int capacity;
    public StringBuilder() { this(16); }
    public StringBuilder(int capacity) {
        if (capacity < 0) throw new NegativeArraySizeException();
        this.capacity = capacity;
    }
    public StringBuilder(String text) {
        this(text.length() + 16);
        append(text);
    }
    public int capacity() { return capacity; }
    public native StringBuilder append(String text);
    public native StringBuilder append(int value);
    public StringBuilder append(long value) {
        if (value == 0) return append('0');
        if (value < 0) { append('-'); if (value == -9223372036854775808L) return append("9223372036854775808"); value = -value; }
        char[] tmp = new char[20];
        int n = 0;
        while (value > 0) { tmp[n] = (char) (48 + (int) (value % 10)); value = value / 10; n = n + 1; }
        while (n > 0) { n = n - 1; append(tmp[n]); }
        return this;
    }
    public native StringBuilder append(char ch);
    public StringBuilder append(Object obj) {
        return append(obj == null ? "null" : obj.toString());
    }
    public native int length();
    public native char charAt(int index);
    public native void setCharAt(int index, char ch);
    public native void setLength(int n);
    public native StringBuilder delete(int start, int end);
    public native StringBuilder insert(int offset, String text);
    public native StringBuilder insert(int offset, char ch);
    public native StringBuilder insert(int offset, int value);
    public native StringBuilder reverse();
    public void ensureCapacity(int minimum) {
        if (minimum <= capacity) return;
        int next = capacity > 1073741822 ? 2147483647 : capacity * 2 + 2;
        if (next < minimum) next = minimum;
        capacity = next;
    }
    public StringBuilder append(StringBuffer value) { return append(String.valueOf(value)); }
    public StringBuilder append(boolean value) { return append(value ? "true" : "false"); }
    public StringBuilder append(float value) { return append(String.valueOf(value)); }
    public StringBuilder append(double value) { return append(String.valueOf(value)); }
    public StringBuilder append(char[] chars) { return append(chars, 0, chars.length); }
    public StringBuilder append(char[] chars, int offset, int count) {
        if (chars == null) throw new NullPointerException();
        if (offset < 0 || count < 0 || offset > chars.length - count)
            throw new IndexOutOfBoundsException();
        ensureCapacity(length() + count);
        int end = offset + count;
        while (offset < end) { append(chars[offset]); offset = offset + 1; }
        return this;
    }
    public StringBuilder insert(int offset, Object value) { return insert(offset, String.valueOf(value)); }
    public StringBuilder insert(int offset, boolean value) { return insert(offset, value ? "true" : "false"); }
    public StringBuilder insert(int offset, long value) { return insert(offset, String.valueOf(value)); }
    public StringBuilder insert(int offset, float value) { return insert(offset, String.valueOf(value)); }
    public StringBuilder insert(int offset, double value) { return insert(offset, String.valueOf(value)); }
    public StringBuilder insert(int offset, char[] chars) { return insert(offset, chars, 0, chars.length); }
    public StringBuilder insert(int offset, char[] chars, int start, int count) {
        if (offset < 0 || offset > length()) throw new StringIndexOutOfBoundsException();
        if (chars == null) throw new NullPointerException();
        if (start < 0 || count < 0 || start > chars.length - count)
            throw new IndexOutOfBoundsException();
        return insert(offset, new String(chars, start, count));
    }
    public StringBuilder deleteCharAt(int index) {
        if (index < 0 || index >= length()) throw new StringIndexOutOfBoundsException();
        return delete(index, index + 1);
    }
    public void getChars(int start, int end, char[] destination, int offset) {
        if (start < 0 || end < start || end > length()) throw new StringIndexOutOfBoundsException();
        if (destination == null) throw new NullPointerException();
        if (offset < 0 || offset > destination.length - (end - start)) throw new IndexOutOfBoundsException();
        while (start < end) { destination[offset] = charAt(start); start = start + 1; offset = offset + 1; }
    }
    public String substring(int start) { return substring(start, length()); }
    public String substring(int start, int end) { return toString().substring(start, end); }
    public native String toString();
}
