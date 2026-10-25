import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
public class ThrowableGcChecks {
    public static Throwable saved;
    private static Throwable makeSaved() { return new Exception("retained trace"); }
    public static int setup() { saved = makeSaved(); return 1; }
    public static int verify() throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        saved.printStackTrace(new PrintStream(bytes, false, "UTF-8"));
        return bytes.toString("UTF-8").indexOf("ThrowableGcChecks.makeSaved") >= 0 ? 1 : 0;
    }
    public static int release() { saved = null; return 1; }
}
