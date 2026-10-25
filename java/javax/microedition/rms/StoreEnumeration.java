package javax.microedition.rms;

/** Stable ascending-ID tie breaks make unsorted and comparator-equal rows reproducible. */
final class StoreEnumeration implements RecordEnumeration {
    private RecordStore store;
    private RecordFilter filter;
    private RecordComparator comparator;
    private int[] ids;
    private int size;
    private int index = -1;
    private int version;
    private boolean keep;
    StoreEnumeration(RecordStore store, RecordFilter filter, RecordComparator comparator, boolean keep) {
        this.store = store; this.filter = filter; this.comparator = comparator; this.keep = keep;
        build(false);
    }
    private void requireAlive() { if (store == null) throw new IllegalStateException("enumeration destroyed"); }
    private void update() { requireAlive(); if (store.isOpen() && keep && version != store.version) build(true); }
    private void build(boolean preserve) {
        requireAlive();
        synchronized (store) {
            if (!store.isOpen()) { ids = new int[0]; size = 0; index = -1; return; }
            int oldId = preserve && index >= 0 && index < size ? ids[index] : 0;
            int oldIndex = index;
            int[] next = new int[store.recs.size()];
            int n = 0;
            int i = 0;
            while (i < store.recs.size()) {
                byte[] row = (byte[]) store.recs.elementAt(i);
                if (row != null && (filter == null || filter.matches(RecordStore.copyRecord(row)))) next[n++] = i + 1;
                i++;
            }
            if (comparator != null) {
                i = 1;
                while (i < n) {
                    int id = next[i]; int j = i;
                    while (j > 0 && comparator.compare(RecordStore.copyRecord((byte[]) store.recs.elementAt(next[j - 1] - 1)), RecordStore.copyRecord((byte[]) store.recs.elementAt(id - 1))) > 0) {
                        next[j] = next[j - 1]; j--;
                    }
                    next[j] = id; i++;
                }
            }
            ids = next; size = n; version = store.version; index = -1;
            if (preserve && oldId != 0) {
                i = 0;
                while (i < n) { if (ids[i] == oldId) { index = i; return; } i++; }
                index = oldIndex - 1;
                if (index >= size) index = size - 1;
            }
        }
    }
    public int numRecords() { update(); return store.isOpen() ? size : 0; }
    public boolean hasNextElement() { update(); return store.isOpen() && index + 1 < size; }
    public boolean hasPreviousElement() { update(); return store.isOpen() && size > 0 && (index == -1 || index > 0); }
    public int nextRecordId() throws InvalidRecordIDException {
        update();
        if (!store.isOpen() || index + 1 >= size) throw new InvalidRecordIDException("no next record");
        return ids[++index];
    }
    public int previousRecordId() throws InvalidRecordIDException {
        update();
        if (!store.isOpen() || size == 0 || index == 0) throw new InvalidRecordIDException("no previous record");
        if (index == -1) index = size;
        return ids[--index];
    }
    public byte[] nextRecord() throws RecordStoreException { requireAlive(); store.requireOpen(); return store.getRecord(nextRecordId()); }
    public byte[] previousRecord() throws RecordStoreException { requireAlive(); store.requireOpen(); return store.getRecord(previousRecordId()); }
    public void reset() { update(); index = -1; }
    public void rebuild() { build(false); }
    public void keepUpdated(boolean keep) { requireAlive(); if (keep) build(true); this.keep = keep; }
    public boolean isKeptUpdated() { requireAlive(); return keep; }
    public void destroy() { requireAlive(); store = null; filter = null; comparator = null; ids = null; size = 0; }
}
