package java.io;

public abstract class Reader {
    protected Object lock;
    protected Reader() { lock = this; }
    protected Reader(Object lock) {
        if (lock == null) throw new NullPointerException();
        this.lock = lock;
    }
    public int read() throws IOException {
        char[] one = new char[1];
        return read(one, 0, 1) < 0 ? -1 : one[0];
    }
    public int read(char[] cbuf) throws IOException { return read(cbuf, 0, cbuf.length); }
    public abstract int read(char[] cbuf, int off, int len) throws IOException;
    public long skip(long n) throws IOException {
        if (n < 0) throw new IllegalArgumentException("Negative skip");
        char[] buffer = new char[(int) (n < 256 ? n : 256)];
        long total = 0;
        while (total < n) {
            int got = read(buffer, 0, (int) (n - total < buffer.length ? n - total : buffer.length));
            if (got < 0) break;
            if (got == 0) {
                if (read() < 0) break;
                total++;
            } else total += got;
        }
        return total;
    }
    public boolean ready() throws IOException { return false; }
    public boolean markSupported() { return false; }
    public void mark(int readAheadLimit) throws IOException { throw new IOException("Mark not supported"); }
    public void reset() throws IOException { throw new IOException("Mark not supported"); }
    public abstract void close() throws IOException;
}
