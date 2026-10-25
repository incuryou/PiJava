package javax.microedition.rms;
public interface RecordEnumeration {
    int numRecords();
    boolean hasNextElement();
    boolean hasPreviousElement();
    byte[] nextRecord() throws RecordStoreException;
    int nextRecordId() throws InvalidRecordIDException;
    byte[] previousRecord() throws RecordStoreException;
    int previousRecordId() throws InvalidRecordIDException;
    void destroy();
    void reset();
    void rebuild();
    void keepUpdated(boolean keep);
    boolean isKeptUpdated();
}
