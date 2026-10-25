package java.io;

public class InputStreamReader extends Reader {
    private InputStream in;
    private int encoding;
    private int pushedByte = -1;
    private int pendingChar = -1;
    public InputStreamReader(InputStream in) {
        super(in);
        this.in = in;
        encoding = Encoding.LATIN1;
    }
    public InputStreamReader(InputStream in, String enc) throws UnsupportedEncodingException {
        super(in);
        this.in = in;
        encoding = Encoding.find(enc);
    }
    public String getEncoding() { return in == null ? null : Encoding.name(encoding); }
    private void ensureOpen() throws IOException { if (in == null) throw new IOException("Reader closed"); }
    private int nextByte() throws IOException {
        if (pushedByte < 0) return in.read();
        int b = pushedByte;
        pushedByte = -1;
        return b;
    }
    private int decode() throws IOException {
        if (pendingChar >= 0) {
            int c = pendingChar;
            pendingChar = -1;
            return c;
        }
        int first = nextByte();
        if (first < 0) return -1;
        if (encoding == Encoding.LATIN1) return first;
        if (encoding == Encoding.ASCII) return first <= 127 ? first : 65533;
        if (first < 128) return first;
        int remaining;
        int value;
        if (first >= 194 && first <= 223) { remaining = 1; value = first & 31; }
        else if (first >= 224 && first <= 239) { remaining = 2; value = first & 15; }
        else if (first >= 240 && first <= 244) { remaining = 3; value = first & 7; }
        else return 65533;
        for (int i = 0; i < remaining; i++) {
            int b = nextByte();
            if (b < 0) return 65533;
            if ((b & 192) != 128 || (i == 0 && ((first == 224 && b < 160) ||
                (first == 237 && b >= 160) || (first == 240 && b < 144) || (first == 244 && b >= 144)))) {
                pushedByte = b;
                return 65533;
            }
            value = (value << 6) | (b & 63);
        }
        if (value <= 65535) return value;
        value -= 65536;
        pendingChar = 56320 | (value & 1023);
        return 55296 | (value >> 10);
    }
    public int read() throws IOException { ensureOpen(); return decode(); }
    public int read(char[] cbuf, int off, int len) throws IOException {
        if (cbuf == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > cbuf.length - len) throw new IndexOutOfBoundsException();
        ensureOpen();
        if (len == 0) return 0;
        int n = 0;
        while (n < len) {
            if (n > 0 && !ready()) break;
            int c = decode();
            if (c < 0) break;
            cbuf[off + n++] = (char) c;
        }
        return n == 0 ? -1 : n;
    }
    public boolean ready() throws IOException {
        ensureOpen();
        return pendingChar >= 0 || pushedByte >= 0 || in.available() > 0;
    }
    public void close() throws IOException {
        if (in != null) {
            InputStream stream = in;
            in = null;
            stream.close();
        }
    }
}
