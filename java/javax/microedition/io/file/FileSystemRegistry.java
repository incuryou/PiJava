package javax.microedition.io.file;

import java.util.Enumeration;
import java.util.Vector;

/** No filesystem backend: root discovery is empty and registration unsupported. */
public class FileSystemRegistry {
    private static final Vector listeners = new Vector();
    private FileSystemRegistry() {}

    public static Enumeration listRoots() {
        return new Vector().elements();
    }
    public static synchronized boolean addFileSystemListener(FileSystemListener listener) {
        if (listener == null) throw new NullPointerException();
        return false;
    }
    public static synchronized boolean removeFileSystemListener(FileSystemListener listener) {
        if (listener == null) throw new NullPointerException();
        return false;
    }
}
