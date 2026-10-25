package javax.microedition.rms;
public interface RecordListener {
    void recordAdded(RecordStore recordStore, int recordId);
    void recordChanged(RecordStore recordStore, int recordId);
    void recordDeleted(RecordStore recordStore, int recordId);
}
