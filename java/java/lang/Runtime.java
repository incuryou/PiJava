package java.lang;
public class Runtime {
    public static Runtime getRuntime() { return new Runtime(); }
    public long freeMemory() { return 1048576L; }
    public long totalMemory() { return 2097152L; }
    public void gc() {}
    public void exit(int status) {}
}
