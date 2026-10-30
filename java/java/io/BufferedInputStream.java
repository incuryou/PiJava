package java.io;

/** Buffered reads and a replayable mark over the existing CLDC stream API. */
public class BufferedInputStream extends FilterInputStream {
    protected byte[] buf;
    protected int count;
    protected int pos;
    protected int markpos = -1;
    protected int marklimit;

    public BufferedInputStream(InputStream in) { this(in, 8192); }
    public BufferedInputStream(InputStream in, int size) {
        super(in);
        if (size <= 0) throw new IllegalArgumentException("Buffer size <= 0");
        buf = new byte[size];
    }
    private void ensureOpen() throws IOException {
        if (buf == null || in == null) throw new IOException("Stream closed");
    }
    private void fill() throws IOException {
        if (markpos < 0) {
            pos = 0;
        } else if (pos >= buf.length) {
            if (markpos > 0) {
                int preserved = pos - markpos;
                System.arraycopy(buf, markpos, buf, 0, preserved);
                pos = preserved;
                markpos = 0;
            } else if (buf.length >= marklimit) {
                markpos = -1;
                pos = 0;
            } else {
                int size = buf.length > marklimit - buf.length ? marklimit : buf.length * 2;
                byte[] next = new byte[size];
                System.arraycopy(buf, 0, next, 0, pos);
                buf = next;
            }
        }
        count = pos;
        int n = in.read(buf, pos, buf.length - pos);
        if (n > 0) count = pos + n;
    }
    public synchronized int read() throws IOException {
        ensureOpen();
        if (pos >= count) {
            fill();
            if (pos >= count) return -1;
        }
        return buf[pos++] & 255;
    }
    private int readPart(byte[] b, int off, int len) throws IOException {
        int remaining = count - pos;
        if (remaining == 0) {
            // Without a mark, large requests can go straight to the caller's buffer.
            if (len >= buf.length && markpos < 0) return in.read(b, off, len);
            fill();
            remaining = count - pos;
            if (remaining == 0) return -1;
        }
        int n = remaining < len ? remaining : len;
        System.arraycopy(buf, pos, b, off, n);
        pos += n;
        return n;
    }
    public synchronized int read(byte[] b, int off, int len) throws IOException {
        ensureOpen();
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        if (len == 0) return 0;
        int total = 0;
        while (true) {
            int n = readPart(b, off + total, len - total);
            if (n <= 0) return total == 0 ? n : total;
            total += n;
            if (total == len || in.available() <= 0) return total;
        }
    }
    public synchronized long skip(long n) throws IOException {
        ensureOpen();
        if (n <= 0) return 0;
        int remaining = count - pos;
        if (remaining == 0) {
            if (markpos < 0) return in.skip(n);
            fill();
            remaining = count - pos;
            if (remaining == 0) return 0;
        }
        long skipped = n < remaining ? n : remaining;
        pos += (int) skipped;
        return skipped;
    }
    public synchronized int available() throws IOException {
        ensureOpen();
        int remaining = count - pos;
        int ready = in.available();
        return ready > 2147483647 - remaining ? 2147483647 : remaining + ready;
    }
    public synchronized void mark(int readlimit) { marklimit = readlimit; markpos = pos; }
    public synchronized void reset() throws IOException {
        ensureOpen();
        if (markpos < 0) throw new IOException("Resetting to invalid mark");
        pos = markpos;
    }
    public boolean markSupported() { return true; }
    public synchronized void close() throws IOException {
        if (buf == null) return;
        InputStream source = in;
        buf = null;
        in = null;
        if (source != null) source.close();
    }
}
