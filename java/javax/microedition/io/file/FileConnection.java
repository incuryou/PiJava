package javax.microedition.io.file;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Enumeration;
import javax.microedition.io.StreamConnection;

/** JSR-75 file contract. Connector exposes no external filesystem backend. */
public interface FileConnection extends StreamConnection {
    boolean isOpen();
    InputStream openInputStream() throws IOException;
    DataInputStream openDataInputStream() throws IOException;
    OutputStream openOutputStream() throws IOException;
    DataOutputStream openDataOutputStream() throws IOException;
    OutputStream openOutputStream(long byteOffset) throws IOException;
    long totalSize();
    long availableSize();
    long usedSize();
    long directorySize(boolean includeSubDirs) throws IOException;
    long fileSize() throws IOException;
    boolean canRead();
    boolean canWrite();
    boolean isHidden();
    void setReadable(boolean readable) throws IOException;
    void setWritable(boolean writable) throws IOException;
    void setHidden(boolean hidden) throws IOException;
    Enumeration list() throws IOException;
    Enumeration list(String filter, boolean includeHidden) throws IOException;
    void create() throws IOException;
    void mkdir() throws IOException;
    boolean exists();
    boolean isDirectory();
    void delete() throws IOException;
    void rename(String newName) throws IOException;
    void truncate(long byteOffset) throws IOException;
    void setFileConnection(String fileName) throws IOException;
    String getName();
    String getPath();
    String getURL();
    long lastModified();
}
