package java.io;

public final class ConsoleOutputStream extends OutputStream {
    public native void write(int value) throws IOException;
    public native void flush() throws IOException;
    public native void close() throws IOException;
}
