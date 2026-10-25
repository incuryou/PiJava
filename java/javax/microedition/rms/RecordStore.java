package javax.microedition.rms;
import java.util.Vector;

/** Single-suite RMS. The native layer commits complete snapshots atomically. */
public class RecordStore {
    public static final int AUTHMODE_PRIVATE = 0;
    public static final int AUTHMODE_ANY = 1;
    private static final int MAGIC = 0xD24D5332;
    private static Vector opened = new Vector();
    Vector recs = new Vector(); // index is recordId - 1; null is a deleted ID
    private Vector listeners = new Vector();
    private String name;
    private int references;
    private int nextId = 1;
    private int count;
    int version;
    private long modified;
    private RecordStore() {}

    private static void validateName(String name) {
        if (name == null) throw new NullPointerException();
        if (name.length() < 1 || name.length() > 32) throw new IllegalArgumentException("record store name must contain 1..32 characters");
    }
    private static void validateMode(int mode) {
        if (mode != AUTHMODE_PRIVATE && mode != AUTHMODE_ANY) throw new IllegalArgumentException("invalid authorization mode");
        // This backend exposes only the owning suite. Both standard modes
        // permit its owner to read/write; writable concerns foreign suites.
        // Named-suite access remains explicitly unsupported below.
    }
    public static synchronized RecordStore openRecordStore(String name, boolean createIfNecessary) throws RecordStoreException {
        validateName(name);
        int i = 0;
        while (i < opened.size()) {
            RecordStore rs = (RecordStore) opened.elementAt(i++);
            if (rs.name.equals(name)) {
                if (rs.references == 2147483647) throw new RecordStoreException("too many opens");
                rs.references++;
                return rs;
            }
        }
        byte[] packed = loadStore(name);
        if (packed == null && !createIfNecessary) throw new RecordStoreNotFoundException(name);
        RecordStore rs = new RecordStore();
        rs.name = name;
        if (packed != null) rs.unpack(packed);
        else { rs.modified = System.currentTimeMillis(); rs.flush(); }
        rs.references = 1;
        opened.addElement(rs);
        return rs;
    }
    public static RecordStore openRecordStore(String name, boolean createIfNecessary, int authmode, boolean writable) throws RecordStoreException {
        validateName(name);
        validateMode(authmode);
        return openRecordStore(name, createIfNecessary);
    }
    public static RecordStore openRecordStore(String name, String vendorName, String suiteName) throws RecordStoreException {
        validateName(name);
        if (vendorName == null || suiteName == null) throw new NullPointerException();
        throw new SecurityException("named-suite RMS access is not supported");
    }
    public synchronized void setMode(int authmode, boolean writable) throws RecordStoreException {
        requireOpen();
        validateMode(authmode);
    }
    public void closeRecordStore() throws RecordStoreException {
        synchronized (RecordStore.class) {
            synchronized (this) {
                requireOpen();
                references--;
                if (references == 0) {
                    opened.removeElement(this);
                    listeners = new Vector();
                    recs = new Vector();
                }
            }
        }
    }
    public static synchronized void deleteRecordStore(String name) throws RecordStoreException {
        validateName(name);
        int i = 0;
        while (i < opened.size()) {
            if (((RecordStore) opened.elementAt(i++)).name.equals(name)) throw new RecordStoreException("record store is open");
        }
        deleteStore(name);
    }
    public static synchronized native String[] listRecordStores();
    public synchronized int getSizeAvailable() throws RecordStoreNotOpenException {
        requireOpen();
        return availableStoreSize(name, getSize());
    }
    boolean isOpen() { return references > 0; }
    void requireOpen() throws RecordStoreNotOpenException {
        if (!isOpen()) throw new RecordStoreNotOpenException("record store is closed");
    }
    public synchronized String getName() throws RecordStoreNotOpenException { requireOpen(); return name; }
    public synchronized int getVersion() throws RecordStoreNotOpenException { requireOpen(); return version; }
    public synchronized long getLastModified() throws RecordStoreNotOpenException { requireOpen(); return modified; }
    public synchronized int getNumRecords() throws RecordStoreNotOpenException { requireOpen(); return count; }
    private byte[] recordData(int id) throws RecordStoreException {
        requireOpen();
        if (id < 1 || id >= nextId || recs.elementAt(id - 1) == null) throw new InvalidRecordIDException("record " + id);
        return (byte[]) recs.elementAt(id - 1);
    }
    static byte[] copyRecord(byte[] row) {
        if (row.length == 0) return null;
        byte[] copy = new byte[row.length];
        System.arraycopy(row, 0, copy, 0, row.length);
        return copy;
    }
    public synchronized byte[] getRecord(int id) throws RecordStoreException { return copyRecord(recordData(id)); }
    public synchronized int getRecord(int id, byte[] buffer, int offset) throws RecordStoreException {
        byte[] row = recordData(id);
        if (buffer == null) throw new NullPointerException();
        if (offset < 0 || offset > buffer.length - row.length) throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(row, 0, buffer, offset, row.length);
        return row.length;
    }
    public synchronized int getRecordSize(int id) throws RecordStoreException { return recordData(id).length; }
    private static byte[] inputRecord(byte[] data, int offset, int numBytes) {
        if (numBytes < 0 || offset < 0) throw new ArrayIndexOutOfBoundsException();
        if (data == null) {
            if (numBytes != 0) throw new NullPointerException();
        } else if (offset > data.length - numBytes) throw new ArrayIndexOutOfBoundsException();
        byte[] copy = new byte[numBytes];
        if (numBytes != 0) System.arraycopy(data, offset, copy, 0, numBytes);
        return copy;
    }
    public synchronized int addRecord(byte[] data, int offset, int numBytes) throws RecordStoreException {
        requireOpen();
        if (nextId == 2147483647) throw new RecordStoreFullException("record IDs exhausted");
        byte[] copy = inputRecord(data, offset, numBytes);
        int id = nextId;
        recs.addElement(copy); nextId++; count++;
        try { commit(); }
        catch (RecordStoreException failure) { recs.removeElementAt(id - 1); nextId--; count--; throw failure; }
        notifyListeners(id, 1);
        return id;
    }
    public synchronized void setRecord(int id, byte[] data, int offset, int numBytes) throws RecordStoreException {
        byte[] old = recordData(id);
        byte[] copy = inputRecord(data, offset, numBytes);
        recs.setElementAt(copy, id - 1);
        try { commit(); }
        catch (RecordStoreException failure) { recs.setElementAt(old, id - 1); throw failure; }
        notifyListeners(id, 2);
    }
    public synchronized void deleteRecord(int id) throws RecordStoreException {
        byte[] old = recordData(id);
        recs.setElementAt(null, id - 1); count--;
        try { commit(); }
        catch (RecordStoreException failure) { recs.setElementAt(old, id - 1); count++; throw failure; }
        notifyListeners(id, 3);
    }
    public synchronized int getNextRecordID() throws RecordStoreException { requireOpen(); return nextId; }
    public synchronized int getSize() throws RecordStoreNotOpenException {
        requireOpen();
        int total = 28 + recs.size() * 4;
        int i = 0;
        while (i < recs.size()) { byte[] row = (byte[]) recs.elementAt(i++); if (row != null) total += row.length; }
        return total;
    }
    public synchronized RecordEnumeration enumerateRecords(RecordFilter filter, RecordComparator comparator, boolean keepUpdated) throws RecordStoreNotOpenException {
        requireOpen();
        return new StoreEnumeration(this, filter, comparator, keepUpdated);
    }
    public synchronized void addRecordListener(RecordListener listener) {
        if (listener == null) throw new NullPointerException();
        if (!isOpen()) throw new IllegalStateException("record store is closed");
        if (!listeners.contains(listener)) listeners.addElement(listener);
    }
    public synchronized void removeRecordListener(RecordListener listener) { listeners.removeElement(listener); }
    private void notifyListeners(int id, int kind) {
        Object[] snapshot = listeners.toArray();
        int i = 0;
        while (i < snapshot.length) {
            RecordListener listener = (RecordListener) snapshot[i++];
            if (kind == 1) listener.recordAdded(this, id);
            else if (kind == 2) listener.recordChanged(this, id);
            else listener.recordDeleted(this, id);
        }
    }
    private void commit() throws RecordStoreException {
        int oldVersion = version;
        long oldModified = modified;
        version++; modified = System.currentTimeMillis();
        try { flush(); }
        catch (RecordStoreException failure) { version = oldVersion; modified = oldModified; throw failure; }
    }
    private static int readInt(byte[] data, int p) {
        return ((data[p] & 255) << 24) | ((data[p + 1] & 255) << 16) | ((data[p + 2] & 255) << 8) | (data[p + 3] & 255);
    }
    private static void writeInt(byte[] data, int p, int value) {
        data[p] = (byte) (value >> 24); data[p + 1] = (byte) (value >> 16);
        data[p + 2] = (byte) (value >> 8); data[p + 3] = (byte) value;
    }
    private void unpack(byte[] packed) throws RecordStoreException {
        int p = 0;
        int slots = -1;
        boolean modern = packed.length >= 4 && readInt(packed, 0) == MAGIC;
        if (modern) {
            if (packed.length < 28 || readInt(packed, 4) != 1) throw new RecordStoreException("invalid RMS snapshot header");
            nextId = readInt(packed, 8); version = readInt(packed, 12);
            modified = ((long) readInt(packed, 16) << 32) | ((long) readInt(packed, 20) & 0xffffffffL);
            slots = readInt(packed, 24); p = 28;
            if (slots < 0 || slots > (packed.length - 28) / 4 || nextId < 1 || nextId - 1 != slots) throw new RecordStoreException("invalid RMS ID table");
        }
        while (p < packed.length) {
            if (p > packed.length - 4) throw new RecordStoreException("truncated record header");
            if (modern && recs.size() >= slots) throw new RecordStoreException("trailing RMS data");
            int length = readInt(packed, p); p += 4;
            if (modern && length == -1) recs.addElement(null);
            else {
                if (length < 0 || length > packed.length - p) throw new RecordStoreException("truncated record store");
                byte[] row = new byte[length];
                System.arraycopy(packed, p, row, 0, length);
                recs.addElement(row); count++; p += length;
            }
        }
        if (modern && recs.size() != slots) throw new RecordStoreException("truncated RMS ID table");
        if (!modern) nextId = recs.size() + 1;
    }
    private void flush() throws RecordStoreException {
        long total = 28L + (long) recs.size() * 4L;
        int i = 0;
        while (i < recs.size()) { byte[] row = (byte[]) recs.elementAt(i++); if (row != null) total += row.length; }
        if (total > 2147483647L) throw new RecordStoreFullException("record store snapshot is too large");
        byte[] packed = new byte[(int) total];
        writeInt(packed, 0, MAGIC); writeInt(packed, 4, 1); writeInt(packed, 8, nextId); writeInt(packed, 12, version);
        writeInt(packed, 16, (int) (modified >> 32)); writeInt(packed, 20, (int) modified); writeInt(packed, 24, recs.size());
        int p = 28; i = 0;
        while (i < recs.size()) {
            byte[] row = (byte[]) recs.elementAt(i++);
            int n = row == null ? -1 : row.length;
            writeInt(packed, p, n); p += 4;
            if (row != null) { System.arraycopy(row, 0, packed, p, n); p += n; }
        }
        saveStore(name, packed);
    }
    private static native int availableStoreSize(String name, int currentSize);
    private static native byte[] loadStore(String name) throws RecordStoreException;
    private static native void saveStore(String name, byte[] packed) throws RecordStoreException;
    private static native void deleteStore(String name) throws RecordStoreException;
}
