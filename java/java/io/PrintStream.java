package java.io;

public class PrintStream extends OutputStream {
    private OutputStream out;
    private OutputStreamWriter writer;
    private boolean autoFlush;
    private boolean trouble;
    public PrintStream() { this(new OutputStream()); }
    public PrintStream(OutputStream out) { this(out, false); }
    public PrintStream(OutputStream out, boolean autoFlush) {
        if (out == null) throw new NullPointerException();
        this.out = out;
        this.autoFlush = autoFlush;
        writer = new OutputStreamWriter(out);
    }
    public PrintStream(OutputStream out, boolean autoFlush, String encoding) throws UnsupportedEncodingException {
        if (out == null) throw new NullPointerException();
        this.out = out;
        this.autoFlush = autoFlush;
        writer = new OutputStreamWriter(out, encoding);
    }
    public void write(int b) {
        if (out == null) { trouble = true; return; }
        try {
            out.write(b);
            if (autoFlush && (b & 255) == 10) out.flush();
        } catch (IOException e) { trouble = true; }
    }
    public void write(byte[] b) { write(b, 0, b.length); }
    public void write(byte[] b, int off, int len) {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        if (out == null) { trouble = true; return; }
        try {
            out.write(b, off, len);
            if (autoFlush) out.flush();
        } catch (IOException e) { trouble = true; }
    }
    public void flush() {
        if (out == null) { trouble = true; return; }
        try { writer.flush(); } catch (IOException e) { trouble = true; }
    }
    public void close() {
        if (out == null) return;
        try { writer.close(); } catch (IOException e) { trouble = true; }
        out = null;
    }
    public boolean checkError() {
        if (out != null) flush();
        return trouble || (out instanceof PrintStream && ((PrintStream) out).checkError());
    }
    protected void setError() { trouble = true; }
    protected void clearError() { trouble = false; }
    public void print(String text) {
        if (out == null) { trouble = true; return; }
        if (text == null) text = "null";
        try { writer.write(text); } catch (IOException e) { trouble = true; }
    }
    public void print(char[] text) {
        if (text == null) throw new NullPointerException();
        if (out == null) { trouble = true; return; }
        try { writer.write(text); } catch (IOException e) { trouble = true; }
    }
    public void print(boolean value) { print(String.valueOf(value)); }
    public void print(char value) { print(String.valueOf(value)); }
    public void print(int value) { print(String.valueOf(value)); }
    public void print(long value) { print(String.valueOf(value)); }
    public void print(float value) { print(String.valueOf(value)); }
    public void print(double value) { print(String.valueOf(value)); }
    public void print(Object obj) { print(String.valueOf(obj)); }
    public void println() { print("\n"); if (autoFlush) flush(); }
    public void println(String text) { print(text); println(); }
    public void println(char[] text) { print(text); println(); }
    public void println(boolean value) { print(value); println(); }
    public void println(char value) { print(value); println(); }
    public void println(int value) { print(value); println(); }
    public void println(long value) { print(value); println(); }
    public void println(float value) { print(value); println(); }
    public void println(double value) { print(value); println(); }
    public void println(Object obj) { print(obj); println(); }
}
