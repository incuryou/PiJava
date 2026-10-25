package java.io;

public class DataInputStream extends InputStream implements DataInput {
    protected InputStream in;
    public DataInputStream(InputStream in) { this.in = in; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b) throws IOException { return in.read(b); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public long skip(long n) throws IOException { return in.skip(n); }
    public int available() throws IOException { return in.available(); }
    public void close() throws IOException { in.close(); }
    public void mark(int readlimit) { in.mark(readlimit); }
    public void reset() throws IOException { in.reset(); }
    public boolean markSupported() { return in.markSupported(); }
    public void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public void readFully(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off > b.length - len) throw new IndexOutOfBoundsException();
        int n = 0;
        while (n < len) {
            int got = in.read(b, off + n, len - n);
            if (got < 0) throw new EOFException();
            if (got == 0) {
                int c = in.read();
                if (c < 0) throw new EOFException();
                b[off + n++] = (byte) c;
            } else n += got;
        }
    }
    public int skipBytes(int n) throws IOException {
        int total = 0;
        while (total < n) {
            long skipped = in.skip(n - total);
            if (skipped <= 0) break;
            total += (int) skipped;
        }
        return total;
    }
    public int readUnsignedByte() throws IOException {
        int c = in.read();
        if (c < 0) throw new EOFException();
        return c;
    }
    public boolean readBoolean() throws IOException { return readUnsignedByte() != 0; }
    public byte readByte() throws IOException { return (byte) readUnsignedByte(); }
    public int readUnsignedShort() throws IOException { return (readUnsignedByte() << 8) | readUnsignedByte(); }
    public short readShort() throws IOException { return (short) readUnsignedShort(); }
    public char readChar() throws IOException { return (char) readUnsignedShort(); }
    public int readInt() throws IOException { return (readUnsignedShort() << 16) | readUnsignedShort(); }
    public long readLong() throws IOException { return ((long) readInt() << 32) | ((long) readInt() & 4294967295L); }
    public float readFloat() throws IOException { return Float.intBitsToFloat(readInt()); }
    public double readDouble() throws IOException { return Double.longBitsToDouble(readLong()); }
    public String readUTF() throws IOException { return readUTF(this); }
    public static String readUTF(DataInput input) throws IOException {
        int len = input.readUnsignedShort();
        byte[] bytes = new byte[len];
        input.readFully(bytes);
        char[] chars = new char[len];
        int i = 0;
        int count = 0;
        while (i < len) {
            int a = bytes[i++] & 255;
            if (a <= 127) chars[count++] = (char) a;
            else if ((a & 224) == 192) {
                if (i >= len) throw new UTFDataFormatException("Truncated UTF character");
                int b = bytes[i++] & 255;
                if ((b & 192) != 128) throw new UTFDataFormatException("Invalid UTF continuation");
                chars[count++] = (char) (((a & 31) << 6) | (b & 63));
            } else if ((a & 240) == 224) {
                if (len - i < 2) throw new UTFDataFormatException("Truncated UTF character");
                int b = bytes[i++] & 255;
                int c = bytes[i++] & 255;
                if ((b & 192) != 128 || (c & 192) != 128) throw new UTFDataFormatException("Invalid UTF continuation");
                chars[count++] = (char) (((a & 15) << 12) | ((b & 63) << 6) | (c & 63));
            } else throw new UTFDataFormatException("Invalid UTF leading byte");
        }
        return new String(chars, 0, count);
    }
}
