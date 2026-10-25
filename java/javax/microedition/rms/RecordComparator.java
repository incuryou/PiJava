package javax.microedition.rms;
public interface RecordComparator {
    int PRECEDES = -1;
    int EQUIVALENT = 0;
    int FOLLOWS = 1;
    int compare(byte[] rec1, byte[] rec2);
}
