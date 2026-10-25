package java.lang;
public class String {
    public String() {}
    public String(StringBuffer buffer) { this(buffer.toString()); }
    public static String valueOf(char[] value) { return new String(value); }
    public static String valueOf(char[] value, int offset, int count) { return new String(value, offset, count); }
    public static String copyValueOf(char[] value) { return new String(value); }
    public static String copyValueOf(char[] value, int offset, int count) { return new String(value, offset, count); }
    public boolean startsWith(String prefix, int offset) {
        if (prefix == null) throw new NullPointerException();
        if (offset < 0 || offset > length() - prefix.length()) return false;
        for (int i=0;i<prefix.length();i++) if (charAt(offset+i)!=prefix.charAt(i)) return false;
        return true;
    }
    public int lastIndexOf(int ch, int from) {
        if (ch < 0 || ch > 65535) return -1;
        int i=Math.min(from,length()-1);
        while(i>=0) { if(charAt(i)==ch)return i; i--; } return -1;
    }
    public int lastIndexOf(String part, int from) {
        if(part==null)throw new NullPointerException();
        for(int i=Math.min(from,length()-part.length());i>=0;i--) if(startsWith(part,i))return i;
        return -1;
    }
    public String(byte[] data) { initBytes(data, 0, data == null ? 0 : data.length); }
    public String(byte[] data, int offset, int length) { initBytes(data, offset, length); }
    public String(char[] data) { initChars(data, 0, data == null ? 0 : data.length); }
    public String(char[] data, int offset, int count) { initChars(data, offset, count); }
    public String(byte[] data, String charset) throws java.io.UnsupportedEncodingException { initBytesCharset(data, 0, data == null ? 0 : data.length, charset); }
    public String(byte[] data, int offset, int length, String charset) throws java.io.UnsupportedEncodingException { initBytesCharset(data, offset, length, charset); }
    private native void initBytes(byte[] data, int offset, int length);
    private native void initChars(char[] data, int offset, int count);
    private native void initBytesCharset(byte[] data, int offset, int length, String charset);
    public native int length();
    public native char charAt(int index);
    public native boolean equals(Object other);
    public native String intern();
    public native boolean startsWith(String prefix);
    public native boolean endsWith(String suffix);
    public native int indexOf(int ch);
    public native int indexOf(int ch, int from);
    public native int indexOf(String part);
    public native int indexOf(String part, int from);
    public native String substring(int begin);
    public native String substring(int begin, int end);
    public native String trim();
    public String toString() { return this; }
    public int hashCode() {
        int h = 0;
        int i = 0;
        while (i < length()) {
            h = h * 31 + charAt(i);
            i = i + 1;
        }
        return h;
    }
    public boolean equalsIgnoreCase(String other) {
        if (other == null || other.length() != length()) return false;
        int i = 0;
        while (i < length()) {
            int a = charAt(i);
            int b = other.charAt(i);
            if (a >= 'A' && a <= 'Z') a = a + 32;
            if (b >= 'A' && b <= 'Z') b = b + 32;
            if (a != b) return false;
            i = i + 1;
        }
        return true;
    }
    public int compareTo(String other) {
        int n = length() < other.length() ? length() : other.length();
        int i = 0;
        while (i < n) {
            int d = charAt(i) - other.charAt(i);
            if (d != 0) return d;
            i = i + 1;
        }
        return length() - other.length();
    }
    public String toLowerCase() {
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < length()) {
            int c = charAt(i);
            if (c >= 'A' && c <= 'Z') c = c + 32;
            sb.append((char) c);
            i = i + 1;
        }
        return sb.toString();
    }
    public String toUpperCase() {
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < length()) {
            int c = charAt(i);
            if (c >= 'a' && c <= 'z') c = c - 32;
            sb.append((char) c);
            i = i + 1;
        }
        return sb.toString();
    }
    public int lastIndexOf(int ch) {
        int i = length() - 1;
        while (i >= 0) {
            if (charAt(i) == ch) return i;
            i = i - 1;
        }
        return -1;
    }
    public int lastIndexOf(String part) {
        if (part == null) return -1;
        int i = length() - part.length();
        while (i >= 0) {
            if (substring(i, i + part.length()).equals(part)) return i;
            i = i - 1;
        }
        return -1;
    }
    public String replace(char a, char b) {
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < length()) {
            int c = charAt(i);
            if (c == a) c = b;
            sb.append((char) c);
            i = i + 1;
        }
        return sb.toString();
    }
    public native byte[] getBytes();
    public native byte[] getBytes(String charset) throws java.io.UnsupportedEncodingException;
    public String concat(String other) {
        StringBuilder sb = new StringBuilder();
        sb.append(this);
        if (other != null) sb.append(other);
        return sb.toString();
    }
    public boolean regionMatches(boolean ignoreCase, int toffset, String other, int ooffset, int len) {
        if (other == null || toffset < 0 || ooffset < 0 || toffset + len > length() || ooffset + len > other.length()) return false;
        int i = 0;
        while (i < len) {
            int a = charAt(toffset + i);
            int b = other.charAt(ooffset + i);
            if (ignoreCase) {
                if (a >= 'A' && a <= 'Z') a = a + 32;
                if (b >= 'A' && b <= 'Z') b = b + 32;
            }
            if (a != b) return false;
            i = i + 1;
        }
        return true;
    }
    public static native String valueOf(int value);
    public static native String valueOf(float value);
    public static native String valueOf(double value);
    public static String valueOf(long value) {
        StringBuilder sb = new StringBuilder();
        sb.append(value);
        return sb.toString();
    }
    public static String valueOf(char ch) {
        StringBuilder sb = new StringBuilder();
        sb.append(ch);
        return sb.toString();
    }
    public static String valueOf(boolean flag) { return flag ? "true" : "false"; }
    public char[] toCharArray() {
        int n = length();
        char[] out = new char[n];
        getChars(0, n, out, 0);
        return out;
    }
    public String(String other) {
        if (other == null) throw new NullPointerException();
        char[] ch = other.toCharArray();
        initChars(ch, 0, ch.length);
    }
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        int i = srcBegin;
        while (i < srcEnd) {
            dst[dstBegin + (i - srcBegin)] = charAt(i);
            i = i + 1;
        }
    }
    public static String valueOf(Object obj) {
        if (obj == null) return "null";
        return obj.toString();
    }
}
