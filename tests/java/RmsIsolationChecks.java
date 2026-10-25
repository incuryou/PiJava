import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.*;
public class RmsIsolationChecks extends MIDlet {
    private static RecordStore held;
    protected void startApp() {}
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
    public static int read(String name) {
        try {
            RecordStore s = RecordStore.openRecordStore(name, false);
            int value = s.getNumRecords() == 0 ? -3 : s.getRecord(1)[0] & 255;
            s.closeRecordStore(); return value;
        } catch (RecordStoreNotFoundException missing) { return -1; }
        catch (RecordStoreException error) { return -2; }
    }
    public static int put(String name, int value) {
        try {
            RecordStore s = RecordStore.openRecordStore(name, true);
            byte[] data = new byte[] {(byte)value};
            if (s.getNumRecords() == 0) s.addRecord(data, 0, 1);
            else s.setRecord(1, data, 0, 1);
            s.closeRecordStore(); return value;
        } catch (RecordStoreException error) { return -2; }
    }
    public static int hold(String name, int value) throws Exception {
        held = RecordStore.openRecordStore(name, true);
        byte[] data = new byte[] {(byte)value};
        if (held.getNumRecords() == 0) held.addRecord(data, 0, 1);
        else held.setRecord(1, data, 0, 1);
        return value;
    }
    public static int erase(String name) {
        try { RecordStore.deleteRecordStore(name); return 1; }
        catch (RecordStoreNotFoundException missing) { return -1; }
        catch (RecordStoreException error) { return -2; }
    }
}
