package java.io;

public abstract class Writer {
    protected Object lock;
    protected Writer() { lock = this; }
    protected Writer(Object lock) {
        if (lock == null) throw new NullPointerException();
        this.lock = lock;
    }
    public void write(int c) throws IOException { write(new char[] {(char) c}, 0, 1); }
    public void write(char[] cbuf) throws IOException { write(cbuf, 0, cbuf.length); }
    public abstract void write(char[] cbuf, int off, int len) throws IOException;
    public void write(String str) throws IOException { write(str, 0, str.length()); }
    public void write(String str, int off, int len) throws IOException {
        if (str == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > str.length() - len) throw new IndexOutOfBoundsException();
        char[] chars = new char[len];
        for (int i = 0; i < len; i++) chars[i] = str.charAt(off + i);
        write(chars, 0, len);
    }
    public abstract void flush() throws IOException;
    public abstract void close() throws IOException;
}
