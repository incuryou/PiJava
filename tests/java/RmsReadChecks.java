import javax.microedition.rms.*;
public class RmsReadChecks {
    private static int checks;
    private static void check(boolean ok, String text) { checks++; if (!ok) throw new RuntimeException(text); }
    public static int run() throws Exception {
        String name = "rms70_" + System.currentTimeMillis();
        boolean caught = false;
        try { RecordStore.openRecordStore(name, false); }
        catch (RecordStoreNotFoundException expected) { caught = true; }
        check(caught, "missing store throws instead of creating one");
        RecordStore store = RecordStore.openRecordStore(name, true);
        check(store.getNumRecords() == 0, "created store empty");
        byte[] settings = new byte[] {3,3,4,0,3};
        caught = false;
        try { settings = store.getRecord(1); }
        catch (InvalidRecordIDException expected) { caught = true; }
        check(caught && settings != null && settings[2] == 4, "farm default settings preserved on missing record");
        int id = store.addRecord(settings, 0, settings.length);
        byte[] read = store.getRecord(id);
        check(read != settings && read.length == 5 && read[2] == 4, "record returns copy");
        read[2] = 99;
        check(store.getRecord(id)[2] == 4, "caller cannot mutate stored data");
        check(store.getRecordSize(id) == 5, "record size");
        byte[] target = new byte[7];
        check(store.getRecord(id, target, 1) == 5 && target[3] == 4, "buffer read and offset");
        caught = false;
        try { store.getRecord(id, target, 3); } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught && target[3] == 4, "small destination rejected without partial write");
        caught = false;
        try { store.getRecord(id, target, -1); } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught, "negative offset rejected");
        caught = false;
        try { store.getRecord(id, null, 0); } catch (NullPointerException expected) { caught = true; }
        check(caught, "null buffer rejected");
        caught = false;
        try { store.getRecordSize(0); } catch (InvalidRecordIDException expected) { caught = true; }
        check(caught, "invalid size lookup throws");
        caught = false;
        try { store.getRecord(999, target, 0); } catch (InvalidRecordIDException expected) { caught = true; }
        check(caught, "invalid buffered lookup throws");
        int empty = store.addRecord(new byte[0],0,0);
        check(store.getRecord(empty) == null && store.getRecordSize(empty) == 0, "existing empty record distinguished from missing id");
        check(store.getRecord(empty, target, target.length) == 0, "zero length buffer copy");
        store.closeRecordStore();
        store = RecordStore.openRecordStore(name, false);
        check(store.getNumRecords() == 2 && store.getRecord(id)[2] == 4, "persisted record read after reopen");
        store.closeRecordStore();
        return checks;
    }
}
