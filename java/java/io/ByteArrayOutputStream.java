package java.io;

public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf;
    protected int count;
    public ByteArrayOutputStream() { this(32); }
    public ByteArrayOutputStream(int size) {
        if (size < 0) throw new IllegalArgumentException("Negative initial size");
        buf = new byte[size];
    }
    private void ensureCapacity(int extra) {
        if (extra > 2147483647 - count) throw new OutOfMemoryError();
        int needed = count + extra;
        if (needed <= buf.length) return;
        int capacity = buf.length <= 1073741823 ? buf.length * 2 : 2147483647;
        if (capacity < needed) capacity = needed;
        byte[] next = new byte[capacity];
        System.arraycopy(buf, 0, next, 0, count);
        buf = next;
    }
    public void write(int b) {
        ensureCapacity(1);
        buf[count++] = (byte) b;
    }
    public void write(byte[] b, int off, int len) {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        ensureCapacity(len);
        System.arraycopy(b, off, buf, count, len);
        count += len;
    }
    public void writeTo(OutputStream out) throws IOException { out.write(buf, 0, count); }
    public void reset() { count = 0; }
    public byte[] toByteArray() {
        byte[] out = new byte[count];
        System.arraycopy(buf, 0, out, 0, count);
        return out;
    }
    public int size() { return count; }
    public String toString() { return new String(buf, 0, count); }
    public String toString(String enc) throws UnsupportedEncodingException {
        InputStreamReader reader = new InputStreamReader(new ByteArrayInputStream(buf, 0, count), enc);
        char[] chars = new char[count];
        int length = 0;
        try {
            while (length < chars.length) {
                int n = reader.read(chars, length, chars.length - length);
                if (n < 0) break;
                length += n;
            }
        } catch (IOException e) { throw new UnsupportedEncodingException(e.getMessage()); }
        return new String(chars, 0, length);
    }
    public void close() {}
}
