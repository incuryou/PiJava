package java.lang;

/** CLDC mutable string buffer backed by the VM's native character builder. */
public final class StringBuffer {
    private final StringBuilder builder;
    public StringBuffer() { builder = new StringBuilder(); }
    public StringBuffer(int capacity) { builder = new StringBuilder(capacity); }
    public StringBuffer(String text) { builder = new StringBuilder(text); }
    public int length() { return builder.length(); }
    public int capacity() { return builder.capacity(); }
    public void ensureCapacity(int minimum) { builder.ensureCapacity(minimum); }
    public void setLength(int length) { builder.setLength(length); }
    public char charAt(int index) { return builder.charAt(index); }
    public void setCharAt(int index, char value) { builder.setCharAt(index, value); }
    public void getChars(int start, int end, char[] destination, int offset) {
        builder.getChars(start, end, destination, offset);
    }
    public StringBuffer append(StringBuffer value) { builder.append(value); return this; }
    public StringBuffer append(Object value) { builder.append(value); return this; }
    public StringBuffer append(String value) { builder.append(value); return this; }
    public StringBuffer append(boolean value) { builder.append(value); return this; }
    public StringBuffer append(char value) { builder.append(value); return this; }
    public StringBuffer append(int value) { builder.append(value); return this; }
    public StringBuffer append(long value) { builder.append(value); return this; }
    public StringBuffer append(float value) { builder.append(value); return this; }
    public StringBuffer append(double value) { builder.append(value); return this; }
    public StringBuffer append(char[] value) { builder.append(value); return this; }
    public StringBuffer append(char[] chars, int offset, int count) {
        builder.append(chars, offset, count); return this;
    }
    public StringBuffer insert(int offset, Object value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, String value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, boolean value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, char value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, int value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, long value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, float value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, double value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, char[] value) { builder.insert(offset, value); return this; }
    public StringBuffer insert(int offset, char[] chars, int start, int count) {
        builder.insert(offset, chars, start, count); return this;
    }
    public StringBuffer delete(int start, int end) { builder.delete(start, end); return this; }
    public StringBuffer deleteCharAt(int index) { builder.deleteCharAt(index); return this; }
    public StringBuffer reverse() { builder.reverse(); return this; }
    public String substring(int start) { return builder.substring(start); }
    public String substring(int start, int end) { return builder.substring(start, end); }
    public String toString() { return builder.toString(); }
}
