package java.lang;

public class Throwable {
    public String detail;
    public Throwable() { fillInStackTrace(); }
    public Throwable(String message) {
        detail = message;
        fillInStackTrace();
    }
    public String getMessage() { return detail; }
    public String getLocalizedMessage() { return getMessage(); }
    public String toString() {
        String name = getClass().getName();
        String message = getLocalizedMessage();
        return message == null ? name : name + ": " + message;
    }
    public native Throwable fillInStackTrace();
    private native String traceText();
    public void printStackTrace() { printStackTrace(System.err); }
    public void printStackTrace(java.io.PrintStream stream) {
        if (stream == null) throw new NullPointerException();
        stream.println(toString() + traceText());
        stream.flush();
    }
}
