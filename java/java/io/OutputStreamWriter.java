package java.io;

public class OutputStreamWriter extends Writer {
    private OutputStream out;
    private int encoding;
    private int highSurrogate = -1;
    public OutputStreamWriter(OutputStream out) {
        super(out);
        this.out = out;
        encoding = Encoding.LATIN1;
    }
    public OutputStreamWriter(OutputStream out, String enc) throws UnsupportedEncodingException {
        super(out);
        this.out = out;
        encoding = Encoding.find(enc);
    }
    public String getEncoding() { return out == null ? null : Encoding.name(encoding); }
    private void ensureOpen() throws IOException { if (out == null) throw new IOException("Writer closed"); }
    private void encode(int c) throws IOException {
        if (highSurrogate >= 0) {
            int high = highSurrogate;
            highSurrogate = -1;
            if (c >= 56320 && c <= 57343) {
                if (encoding == Encoding.UTF8) {
                    int cp = 65536 + ((high - 55296) << 10) + c - 56320;
                    out.write(240 | (cp >> 18));
                    out.write(128 | ((cp >> 12) & 63));
                    out.write(128 | ((cp >> 6) & 63));
                    out.write(128 | (cp & 63));
                } else out.write('?');
                return;
            }
            out.write('?');
        }
        if (c >= 55296 && c <= 56319) { highSurrogate = c; return; }
        if (c >= 56320 && c <= 57343) { out.write('?'); return; }
        if (encoding != Encoding.UTF8) { out.write(c <= (encoding == Encoding.LATIN1 ? 255 : 127) ? c : '?'); }
        else if (c <= 127) out.write(c);
        else if (c <= 2047) {
            out.write(192 | (c >> 6));
            out.write(128 | (c & 63));
        } else {
            out.write(224 | (c >> 12));
            out.write(128 | ((c >> 6) & 63));
            out.write(128 | (c & 63));
        }
    }
    public void write(char[] cbuf, int off, int len) throws IOException {
        if (cbuf == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > cbuf.length - len) throw new IndexOutOfBoundsException();
        ensureOpen();
        for (int i = 0; i < len; i++) encode(cbuf[off + i]);
    }
    public void flush() throws IOException { ensureOpen(); out.flush(); }
    public void close() throws IOException {
        if (out == null) return;
        OutputStream stream = out;
        try {
            if (highSurrogate >= 0) { stream.write('?'); highSurrogate = -1; }
            stream.flush();
        } finally {
            out = null;
            stream.close();
        }
    }
}
