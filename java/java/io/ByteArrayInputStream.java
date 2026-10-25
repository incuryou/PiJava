package java.io;

public class ByteArrayInputStream extends InputStream {
    protected byte[] buf;
    protected int pos;
    protected int mark;
    protected int count;
    public ByteArrayInputStream(byte[] data) { this(data, 0, data.length); }
    public ByteArrayInputStream(byte[] data, int offset, int length) {
        if (data == null) throw new NullPointerException();
        if (offset < 0 || length < 0 || offset > data.length) throw new IndexOutOfBoundsException();
        buf = data;
        pos = offset;
        mark = offset;
        count = length > data.length - offset ? data.length : offset + length;
    }
    public int read() { return pos < count ? buf[pos++] & 255 : -1; }
    public int read(byte[] b, int off, int len) {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        if (len == 0) return 0;
        if (pos >= count) return -1;
        if (len > count - pos) len = count - pos;
        System.arraycopy(buf, pos, b, off, len);
        pos += len;
        return len;
    }
    public long skip(long n) {
        if (n <= 0) return 0;
        if (n > count - pos) n = count - pos;
        pos += (int) n;
        return n;
    }
    public int available() { return count - pos; }
    public void mark(int readlimit) { mark = pos; }
    public void reset() { pos = mark; }
    public boolean markSupported() { return true; }
    public void close() {}
}
