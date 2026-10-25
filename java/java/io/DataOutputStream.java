package java.io;

public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;
    protected int written;
    public DataOutputStream(OutputStream out) { this.out = out; }
    private void addWritten(int n) { written = n > 2147483647 - written ? 2147483647 : written + n; }
    public void write(int b) throws IOException { out.write(b); addWritten(1); }
    public void write(byte[] b, int off, int len) throws IOException { out.write(b, off, len); addWritten(len); }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException {
        try { flush(); } finally { out.close(); }
    }
    public int size() { return written; }
    public void writeBoolean(boolean v) throws IOException { write(v ? 1 : 0); }
    public void writeByte(int v) throws IOException { write(v); }
    public void writeShort(int v) throws IOException { write(v >>> 8); write(v); }
    public void writeChar(int v) throws IOException { writeShort(v); }
    public void writeInt(int v) throws IOException { writeShort(v >>> 16); writeShort(v); }
    public void writeLong(long v) throws IOException { writeInt((int) (v >>> 32)); writeInt((int) v); }
    public void writeFloat(float v) throws IOException { writeInt(Float.floatToIntBits(v)); }
    public void writeDouble(double v) throws IOException { writeLong(Double.doubleToLongBits(v)); }
    public void writeBytes(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) write(s.charAt(i));
    }
    public void writeChars(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) writeChar(s.charAt(i));
    }
    public void writeUTF(String s) throws IOException {
        int length = s.length();
        int encoded = 0;
        for (int i = 0; i < length; i++) {
            int c = s.charAt(i);
            encoded += c >= 1 && c <= 127 ? 1 : c <= 2047 ? 2 : 3;
            if (encoded > 65535) throw new UTFDataFormatException("UTF string too long");
        }
        writeShort(encoded);
        for (int i = 0; i < length; i++) {
            int c = s.charAt(i);
            if (c >= 1 && c <= 127) write(c);
            else if (c <= 2047) {
                write(192 | (c >> 6));
                write(128 | (c & 63));
            } else {
                write(224 | (c >> 12));
                write(128 | ((c >> 6) & 63));
                write(128 | (c & 63));
            }
        }
    }
}
