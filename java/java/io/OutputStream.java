package java.io;

public class OutputStream {
    public void write(int b) throws IOException { throw new IOException("Output stream has no destination"); }
    public void write(byte[] b) throws IOException { write(b, 0, b.length); }
    public void write(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        for (int i = 0; i < len; i++) write(b[off + i] & 255);
    }
    public void flush() throws IOException {}
    public void close() throws IOException {}
}
