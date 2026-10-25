import javax.microedition.rms.*;

/** Executed by the interpreter harness; uses disposable stores only. */
public class RmsLifecycleChecks {
    private static int checks;
    private static int events;
    private static RecordStore failureStore;
    private static int failureId;
    private static int failureVersion;
    private static void check(boolean ok, String message) { checks++; if (!ok) throw new RuntimeException(message); }
    private static boolean invalid(RecordStore store, int id) throws Exception {
        try { store.getRecord(id); return false; } catch (InvalidRecordIDException expected) { return true; }
    }
    public static int run() throws Exception {
        checks = 0; events = 0;
        String name = "rms73_" + System.currentTimeMillis();
        boolean caught = false;
        try { RecordStore.openRecordStore("", true); } catch (IllegalArgumentException expected) { caught = true; }
        check(caught, "empty name rejected");
        caught = false;
        try { RecordStore.openRecordStore("123456789012345678901234567890123", true); } catch (IllegalArgumentException expected) { caught = true; }
        check(caught, "name above 32 characters rejected");
        caught = false;
        try { RecordStore.openRecordStore(null, true); } catch (NullPointerException expected) { caught = true; }
        check(caught, "null name rejected");
        caught = false;
        try { RecordStore.openRecordStore(name, true, 19, true); } catch (IllegalArgumentException expected) { caught = true; }
        check(caught, "invalid authorization mode rejected");
        RecordStore owner = RecordStore.openRecordStore(name, true, RecordStore.AUTHMODE_ANY, false);
        check(owner.getName().equals(name), "AUTHMODE_ANY permits its owning suite");
        int ownerId=owner.addRecord(new byte[] {7},0,1);
        check(owner.getRecord(ownerId)[0]==7, "writable false does not restrict owner writes");
        owner.setMode(RecordStore.AUTHMODE_PRIVATE,false);
        owner.setMode(RecordStore.AUTHMODE_ANY,false);
        owner.closeRecordStore();
        owner=RecordStore.openRecordStore(name,false,RecordStore.AUTHMODE_ANY,true);
        check(owner.getRecord(ownerId)[0]==7, "owner record survives mode calls and reopen");
        owner.closeRecordStore(); RecordStore.deleteRecordStore(name);
        caught = false;
        try { RecordStore.openRecordStore(name, "vendor", "suite"); } catch (SecurityException expected) { caught = true; }
        check(caught, "named-suite lookup explicitly rejected");
        RecordStore store = RecordStore.openRecordStore(name, true, RecordStore.AUTHMODE_PRIVATE, false);
        RecordStore alias = RecordStore.openRecordStore(name, false);
        check(alias == store, "repeated open returns same object");
        check(store.getName().equals(name) && store.getNextRecordID() == 1, "fresh name and next ID");
        RecordListener listener = new RecordListener() {
            public void recordAdded(RecordStore rs, int id) { events += 1; }
            public void recordChanged(RecordStore rs, int id) { events += 10; }
            public void recordDeleted(RecordStore rs, int id) { events += 100; }
        };
        store.addRecordListener(listener); store.addRecordListener(listener);
        int first = store.addRecord(new byte[] {30}, 0, 1);
        int second = store.addRecord(new byte[] {20}, 0, 1);
        int third = store.addRecord(new byte[] {10}, 0, 1);
        check(first == 1 && second == 2 && third == 3 && alias.getNumRecords() == 3, "shared state and sequential IDs");
        store.deleteRecord(second);
        check(invalid(store, second) && store.getRecord(third)[0] == 10, "delete leaves a hole and preserves later ID");
        int empty = alias.addRecord(null, 0, 0);
        check(empty == 4 && store.getNextRecordID() == 5, "deleted IDs are never reused");
        check(store.getRecord(empty) == null && store.getRecordSize(empty) == 0, "null input creates zero-byte record");
        byte[] buffer = new byte[1];
        check(store.getRecord(empty, buffer, 1) == 0, "zero-byte record can copy at buffer end");
        store.setRecord(empty, null, 0, 0);
        check(store.getRecord(empty) == null, "null input can replace zero-byte record");
        check(events == 114, "listener duplicate suppressed and receives committed operations");
        store.removeRecordListener(listener);
        int oldVersion = store.getVersion();
        caught = false;
        try { store.setRecord(second, new byte[] {1}, 0, 1); } catch (InvalidRecordIDException expected) { caught = true; }
        check(caught && store.getVersion() == oldVersion, "set deleted ID fails without modification");
        caught = false;
        try { store.deleteRecord(1000); } catch (InvalidRecordIDException expected) { caught = true; }
        check(caught && store.getNumRecords() == 3, "delete invalid ID fails");
        caught = false;
        try { store.addRecord(new byte[1], 1, 1); } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught && store.getNextRecordID() == 5, "invalid input does not consume an ID");
        caught = false;
        try { store.addRecord(null, 0, 1); } catch (NullPointerException expected) { caught = true; }
        check(caught && store.getNumRecords() == 3, "nonzero null input rejected");
        RecordFilter filter = new RecordFilter() {
            public boolean matches(byte[] row) { if (row == null) return false; row[0] += 0; return row[0] >= 10; }
        };
        RecordComparator comparator = new RecordComparator() {
            public int compare(byte[] a, byte[] b) { return a[0] < b[0] ? PRECEDES : a[0] > b[0] ? FOLLOWS : EQUIVALENT; }
        };
        RecordEnumeration fixed = store.enumerateRecords(filter, comparator, false);
        check(fixed.numRecords() == 2 && !fixed.isKeptUpdated(), "filtered snapshot count");
        check(fixed.nextRecordId() == third && fixed.nextRecordId() == first && !fixed.hasNextElement(), "comparator sorts filtered records");
        caught = false;
        try { fixed.nextRecordId(); } catch (InvalidRecordIDException expected) { caught = true; }
        check(caught && fixed.previousRecordId() == third, "exhaustion does not advance enumeration");
        fixed.reset();
        check(fixed.previousRecordId() == first, "first backwards call returns last record");
        fixed.reset();
        RecordEnumeration live = store.enumerateRecords(filter, comparator, true);
        int fifth = store.addRecord(new byte[] {15}, 0, 1);
        check(fixed.numRecords() == 2 && live.numRecords() == 3, "keepUpdated observes adds while snapshot stays fixed");
        check(live.nextRecordId() == third, "live sorted first record");
        store.setRecord(first, new byte[] {5}, 0, 1);
        check(live.numRecords() == 2 && live.nextRecordId() == fifth, "updates rebuild filter and preserve current position");
        store.deleteRecord(third);
        live.reset();
        check(live.numRecords() == 1 && live.nextRecordId() == fifth, "live index removes deleted records");
        fixed.rebuild();
        check(fixed.numRecords() == 1 && fixed.nextRecordId() == fifth, "explicit rebuild refreshes snapshot");
        fixed.keepUpdated(true);
        check(fixed.isKeptUpdated(), "keepUpdated enabled");
        fixed.keepUpdated(false);
        store.setRecord(fifth, new byte[] {19}, 0, 1);
        fixed.reset();
        byte[] row = fixed.nextRecord(); row[0] = 99;
        check(store.getRecord(fifth)[0] == 19, "enumeration returns defensive copies");
        RecordEnumeration mutate = store.enumerateRecords(new RecordFilter() {
            public boolean matches(byte[] data) { if (data != null) data[0] = 99; return true; }
        }, null, false);
        check(store.getRecord(first)[0] == 5 && mutate.numRecords() == 3, "filter cannot mutate stored records");
        mutate.destroy();
        caught = false;
        try { mutate.hasNextElement(); } catch (IllegalStateException expected) { caught = true; }
        check(caught, "destroy invalidates enumeration");
        store.deleteRecord(fifth);
        int next = store.getNextRecordID();
        int version = store.getVersion();
        long modified = store.getLastModified();
        caught = false;
        try { RecordStore.deleteRecordStore(name); } catch (RecordStoreException expected) { caught = true; }
        check(caught, "open store cannot be deleted");
        store.closeRecordStore();
        check(alias.getNumRecords() == 2, "first close preserves second open reference");
        alias.closeRecordStore();
        check(!live.hasNextElement() && !live.hasPreviousElement(), "closed store invalidates enumeration");
        caught = false;
        try { store.getRecord(first); } catch (RecordStoreNotOpenException expected) { caught = true; }
        check(caught, "record read after final close throws NotOpen");
        caught = false;
        try { store.getNumRecords(); } catch (RecordStoreNotOpenException expected) { caught = true; }
        check(caught, "metadata read after final close throws NotOpen");
        caught = false;
        try { store.addRecord(null, 0, 0); } catch (RecordStoreNotOpenException expected) { caught = true; }
        check(caught, "write after close throws NotOpen");
        caught = false;
        try { store.closeRecordStore(); } catch (RecordStoreNotOpenException expected) { caught = true; }
        check(caught, "unbalanced close throws NotOpen");
        store = RecordStore.openRecordStore(name, false);
        check(store != alias && store.getNumRecords() == 2 && store.getNextRecordID() == next, "reopen preserves holes and next ID");
        check(store.getVersion() == version && store.getLastModified() == modified, "version and timestamp persist");
        check(invalid(store, second) && invalid(store, third) && invalid(store, fifth), "all deleted IDs remain absent after reopen");
        check(store.addRecord(new byte[] {42}, 0, 1) == next, "trailing deleted ID is not reused after reopen");
        check(!live.hasNextElement(), "old enumeration stays invalid after reopen");
        store.closeRecordStore();
        RecordStore.deleteRecordStore(name);
        caught = false;
        try { RecordStore.openRecordStore(name, false); } catch (RecordStoreNotFoundException expected) { caught = true; }
        check(caught, "deleteRecordStore removes persistent store");
        caught = false;
        try { RecordStore.deleteRecordStore(name); } catch (RecordStoreNotFoundException expected) { caught = true; }
        check(caught, "delete missing store throws NotFound");
        String punctuation = name + "-";
        String underscore = name + "_";
        store = RecordStore.openRecordStore(punctuation, true);
        alias = RecordStore.openRecordStore(underscore, true);
        store.addRecord(new byte[] {7}, 0, 1); alias.addRecord(new byte[] {8}, 0, 1);
        store.closeRecordStore(); alias.closeRecordStore();
        store = RecordStore.openRecordStore(punctuation, false); alias = RecordStore.openRecordStore(underscore, false);
        check(store.getRecord(1)[0] == 7 && alias.getRecord(1)[0] == 8, "new punctuation names have distinct disk filenames");
        store.closeRecordStore(); alias.closeRecordStore();
        RecordStore.deleteRecordStore(punctuation); RecordStore.deleteRecordStore(underscore);
        StringBuffer unicode = new StringBuffer();
        int u = 0;
        while (u < 32) { unicode.append((char) (0x4e00 + name.charAt(u % name.length()))); u++; }
        String unicodeName = unicode.toString();
        store = RecordStore.openRecordStore(unicodeName, true);
        store.addRecord(new byte[] {11}, 0, 1); store.closeRecordStore();
        store = RecordStore.openRecordStore(unicodeName, false);
        check(store.getRecord(1)[0] == 11 && store.getName().length() == 32, "32-character Unicode name persists");
        store.closeRecordStore(); RecordStore.deleteRecordStore(unicodeName);
        return checks;
    }
    // Optional isolated native harness stages: never invoke against a user's RMS directory.
    public static int legacyFixture() throws Exception {
        checks = 0;
        RecordStore store = RecordStore.openRecordStore("Rms73Legacy_Fixture", false);
        check(store.getNumRecords() == 3 && store.getRecord(1)[0] == 7 && store.getRecord(2) == null && store.getRecord(3)[0] == 9, "legacy length stream read");
        store.deleteRecord(2); store.deleteRecord(3);
        check(store.addRecord(new byte[] {4}, 0, 1) == 4, "legacy store upgrades without losing high ID");
        store.closeRecordStore();
        store = RecordStore.openRecordStore("Rms73Legacy_Fixture", false);
        check(store.getNumRecords() == 2 && store.getNextRecordID() == 5 && invalid(store, 3), "upgraded snapshot round trip");
        store.closeRecordStore(); RecordStore.deleteRecordStore("Rms73Legacy_Fixture");
        return checks;
    }
    public static int prepareIoFailure() throws Exception {
        failureStore = RecordStore.openRecordStore("Rms73IoFixture", true);
        failureId = failureStore.addRecord(new byte[] {6}, 0, 1);
        failureVersion = failureStore.getVersion();
        return 1;
    }
    public static int prepareLegacyFailure() throws Exception {
        failureStore = RecordStore.openRecordStore("Rms73IoFixture", false);
        failureId = 1;
        failureVersion = failureStore.getVersion();
        return 1;
    }
    public static int ioFailure() throws Exception {
        checks = 0;
        boolean caught = false;
        try { failureStore.addRecord(new byte[] {7}, 0, 1); } catch (RecordStoreException expected) { caught = true; }
        check(caught && failureStore.getNextRecordID() == 2 && failureStore.getNumRecords() == 1, "failed save rolls back add and ID");
        caught = false;
        try { failureStore.setRecord(failureId, new byte[] {8}, 0, 1); } catch (RecordStoreException expected) { caught = true; }
        check(caught && failureStore.getRecord(failureId)[0] == 6, "failed save rolls back set");
        caught = false;
        try { failureStore.deleteRecord(failureId); } catch (RecordStoreException expected) { caught = true; }
        check(caught && failureStore.getRecord(failureId)[0] == 6 && failureStore.getVersion() == failureVersion, "failed save rolls back delete and version");
        failureStore.closeRecordStore();
        return checks;
    }
    public static int loadFailure() throws Exception {
        checks = 0; boolean caught = false;
        try { RecordStore.openRecordStore("Rms73InvalidFixture", true); } catch (RecordStoreException expected) { caught = true; }
        check(caught, "unreadable or corrupt existing store cannot be silently recreated");
        return checks;
    }
    public static int deleteFixture() throws Exception {
        RecordStore.deleteRecordStore("Rms73IoFixture");
        boolean caught = false;
        try { RecordStore.openRecordStore("Rms73IoFixture", false); } catch (RecordStoreNotFoundException expected) { caught = true; }
        if (!caught) throw new RuntimeException("deleted store resurrected");
        return 1;
    }
    public static int deleteJournalFailure() throws Exception {
        checks = 0;
        boolean caught = false;
        try { RecordStore.deleteRecordStore("Rms73IoFixture"); } catch (RecordStoreException expected) { caught = true; }
        check(caught, "invalid replacement journal prevents deleting main snapshot");
        RecordStore store = RecordStore.openRecordStore("Rms73IoFixture", false);
        check(store.getRecord(1)[0] == 6, "failed journal deletion preserves main snapshot");
        store.closeRecordStore();
        return checks;
    }
    public static int verifyIoRecovery() throws Exception {
        checks = 0;
        RecordStore store = RecordStore.openRecordStore("Rms73IoFixture", false);
        check(store.getNumRecords() == 1 && store.getRecord(1)[0] == 6 && store.getNextRecordID() == 2, "failed writes preserve durable previous snapshot");
        store.closeRecordStore(); RecordStore.deleteRecordStore("Rms73IoFixture");
        return checks;
    }
}
