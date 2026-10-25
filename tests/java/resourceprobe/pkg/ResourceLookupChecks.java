package resourceprobe.pkg;

import java.io.InputStream;
import resourceprobe.other.ResourceLookupOther;

/** Own byte fixtures are supplied by resource_lookup_checks.cpp, never by a game JAR. */
public final class ResourceLookupChecks {
    private static InputStream retained;

    private static boolean byteIs(InputStream stream, int expected) throws Exception {
        if (stream == null) return false;
        boolean ok = stream.read() == expected && stream.read() == -1;
        stream.close();
        return ok;
    }

    // Catches a root resource incorrectly winning over the receiver's package.
    public static int relativeCollision() {
        try { return byteIs(ResourceLookupChecks.class.getResourceAsStream("collision.bin"), 65) ? 1 : -1; }
        catch (Exception unexpected) { return -101; }
    }

    // Catches a relative miss incorrectly falling back to a root basename.
    public static int relativeMissing() {
        try { return ResourceLookupChecks.class.getResourceAsStream("root-only.bin") == null ? 1 : -2; }
        catch (Exception unexpected) { return -102; }
    }

    public static int relativePackageOnly() {
        try { return byteIs(ResourceLookupChecks.class.getResourceAsStream("package-only.bin"), 67) ? 1 : -3; }
        catch (Exception unexpected) { return -103; }
    }

    public static int relativeNested() {
        try { return byteIs(ResourceLookupChecks.class.getResourceAsStream("nested/item.bin"), 68) ? 1 : -4; }
        catch (Exception unexpected) { return -104; }
    }

    // The caller lives in pkg; the Class receiver here lives in other.
    public static int foreignClassLiteral() {
        try { return byteIs(ResourceLookupOther.class.getResourceAsStream("receiver.bin"), 69) ? 1 : -5; }
        catch (Exception unexpected) { return -105; }
    }

    public static int foreignObjectClass() {
        try { return byteIs(new ResourceLookupOther().getClass().getResourceAsStream("receiver.bin"), 69) ? 1 : -6; }
        catch (Exception unexpected) { return -106; }
    }

    // Array receivers use their component class's package, including dimensions.
    public static int arrayClassReceivers() {
        try {
            if (!byteIs(ResourceLookupOther[].class.getResourceAsStream("receiver.bin"), 69)) return -20;
            if (!byteIs(new ResourceLookupOther[1][1].getClass().getResourceAsStream("receiver.bin"), 69)) return -21;
            if (!byteIs(int[].class.getResourceAsStream("default.bin"), 71)) return -22;
            return 3;
        } catch (Exception unexpected) { return -113; }
    }

    public static int absoluteRoot() {
        try {
            if (!byteIs(ResourceLookupChecks.class.getResourceAsStream("/collision.bin"), 66)) return -7;
            if (!byteIs(ResourceLookupChecks.class.getResourceAsStream("/resourceprobe/other/receiver.bin"), 69)) return -8;
            return 2;
        } catch (Exception unexpected) { return -107; }
    }

    public static int defaultPackage() {
        try {
            Class receiver = Class.forName("ResourceLookupDefault");
            if (!byteIs(receiver.getResourceAsStream("default.bin"), 71)) return -9;
            if (!byteIs(receiver.getResourceAsStream("/default.bin"), 71)) return -10;
            return 2;
        } catch (Exception unexpected) { return -108; }
    }

    public static int emptyAndMissing() {
        try {
            InputStream empty = ResourceLookupChecks.class.getResourceAsStream("/empty.bin");
            if (empty == null || empty.available() != 0 || empty.read() != -1) return -11;
            empty.close();
            return ResourceLookupChecks.class.getResourceAsStream("/absent-own-fixture.bin") == null ? 2 : -12;
        } catch (Exception unexpected) { return -109; }
    }

    // Characterize existing VM boundaries; this change intentionally preserves them.
    public static int preservedBoundaries() {
        try {
            if (ResourceLookupChecks.class.getResourceAsStream(null) != null) return -23;
            if (!byteIs(ResourceLookupChecks.class.getResourceAsStream("/missing-own-path/collision.bin"), 66)) return -24;
            return 2;
        } catch (Exception unexpected) { return -114; }
    }

    public static int independentStreams() {
        try {
            InputStream first = ResourceLookupChecks.class.getResourceAsStream("/stream.bin");
            InputStream second = ResourceLookupChecks.class.getResourceAsStream("/stream.bin");
            if (first == null || second == null || first == second) return -13;
            if (first.read() != 17 || first.skip(1) != 1 || first.read() != 51) return -14;
            byte[] tail = new byte[2];
            if (first.read(tail, 0, 2) != 2 || tail[0] != 68 || (tail[1] & 255) != 255) return -15;
            if (first.read() != -1 || second.read() != 17 || second.available() != 4) return -16;
            first.close();
            second.close();
            // Resource streams are ByteArrayInputStreams; close leaves them usable.
            if (second.read() != 34 || second.skip(100) != 3 || second.read() != -1) return -17;
            return 5;
        } catch (Exception unexpected) { return -110; }
    }

    public static int setupRetained() {
        try {
            retained = ResourceLookupChecks.class.getResourceAsStream("/gc-stream.bin");
            return retained != null && retained.read() == 33 ? 1 : -18;
        } catch (Exception unexpected) { return -111; }
    }

    public static int verifyRetained() {
        try {
            if (retained == null || retained.available() != 3 || retained.read() != 67
                || retained.skip(1) != 1 || retained.read() != 135 || retained.read() != -1) return -19;
            retained.close();
            retained = null;
            return 1;
        } catch (Exception unexpected) { return -112; }
    }

    /** Positive count on success; first negative value identifies the failed contract. */
    public static int run() {
        int result;
        int checks = 0;
        result = relativeCollision(); if (result < 0) return result; checks += result;
        result = relativeMissing(); if (result < 0) return result; checks += result;
        result = relativePackageOnly(); if (result < 0) return result; checks += result;
        result = relativeNested(); if (result < 0) return result; checks += result;
        result = foreignClassLiteral(); if (result < 0) return result; checks += result;
        result = foreignObjectClass(); if (result < 0) return result; checks += result;
        result = arrayClassReceivers(); if (result < 0) return result; checks += result;
        result = absoluteRoot(); if (result < 0) return result; checks += result;
        result = defaultPackage(); if (result < 0) return result; checks += result;
        result = emptyAndMissing(); if (result < 0) return result; checks += result;
        result = preservedBoundaries(); if (result < 0) return result; checks += result;
        result = independentStreams(); if (result < 0) return result; checks += result;
        return checks;
    }
}
