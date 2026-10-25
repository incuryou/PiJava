package java.io;

public abstract class InputStream {
    public abstract int read() throws IOException;
    public int read(byte[] b) throws IOException { return read(b, 0, b.length); }
    public int read(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        if (len == 0) return 0;
        int first = read();
        if (first < 0) return -1;
        b[off] = (byte) first;
        int n = 1;
        try {
            while (n < len) {
                int c = read();
                if (c < 0) break;
                b[off + n++] = (byte) c;
            }
        } catch (IOException e) { }
        return n;
    }
    public long skip(long n) throws IOException {
        long skipped = 0;
        while (skipped < n && read() >= 0) skipped++;
        return skipped;
    }
    public int available() throws IOException { return 0; }
    public void close() throws IOException {}
    public void mark(int readlimit) {}
    public void reset() throws IOException { throw new IOException("Mark not supported"); }
    public boolean markSupported() { return false; }
}
