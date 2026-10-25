import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

/** Regression checks for compact primitive/reference array storage and its consumers. */
public class ArrayChecks {
    private static int checks;
    private static int componentInitializations;
    private static class UninitializedComponent {
        static { componentInitializations++; }
    }
    private static void check(boolean ok, String reason) {
        checks++;
        if (!ok) throw new RuntimeException("Array check " + checks + ": " + reason);
    }

    private static void defaultsAndStores() {
        byte[] b = new byte[5]; boolean[] z = new boolean[5];
        char[] c = new char[5]; short[] s = new short[5];
        int[] i = new int[5]; float[] f = new float[5];
        long[] j = new long[5]; double[] d = new double[5]; Object[] a = new Object[5];
        for (int n = 0; n < 5; n++) {
            check(b[n] == 0 && !z[n], "byte/boolean default at " + n);
            check(c[n] == 0 && s[n] == 0, "char/short default at " + n);
            check(i[n] == 0 && Float.floatToRawIntBits(f[n]) == 0, "int/float default at " + n);
            check(j[n] == 0L && Double.doubleToRawLongBits(d[n]) == 0L, "long/double default at " + n);
            check(a[n] == null, "reference default at " + n);
        }
        b[0] = -128; b[1] = -1; b[2] = 0; b[3] = 127; b[4] = (byte) 255;
        check(b[0] == -128 && b[1] == -1 && b[3] == 127 && b[4] == -1, "signed byte loads");
        z[0] = true; z[2] = true; z[4] = true; z[2] = false;
        check(z[0] && !z[1] && !z[2] && !z[3] && z[4], "boolean stores and adjacent elements");
        c[0] = '\u0000'; c[1] = '\u007f'; c[2] = '\u8000'; c[3] = '\uffff'; c[4] = '\ud800';
        check(c[2] == 32768 && c[3] == 65535 && c[4] == 55296, "unsigned char loads");
        s[0] = -32768; s[1] = -1; s[2] = 0; s[3] = 32767; s[4] = (short) 65535;
        check(s[0] == -32768 && s[1] == -1 && s[3] == 32767 && s[4] == -1, "signed short loads");
        i[0] = -2147483648; i[1] = -1; i[3] = 2147483647; i[4] = 0x89abcdef;
        check(i[0] == -2147483648 && i[1] == -1 && i[3] == 2147483647 && i[4] == 0x89abcdef, "int bit patterns");
        j[0] = -9223372036854775808L; j[1] = -1L; j[3] = 9223372036854775807L; j[4] = 0x0123456789abcdefL;
        check(j[0] == -9223372036854775808L && j[1] == -1L && j[3] == 9223372036854775807L && j[4] == 0x0123456789abcdefL, "long bit patterns");
        f[0] = Float.intBitsToFloat(0x00000001); f[1] = Float.intBitsToFloat(0x7f7fffff);
        f[2] = Float.intBitsToFloat(0xff7fffff); f[3] = Float.intBitsToFloat(0x7f800000);
        check(Float.floatToRawIntBits(f[0]) == 1 && Float.floatToRawIntBits(f[1]) == 0x7f7fffff && Float.floatToRawIntBits(f[2]) == 0xff7fffff && Float.floatToRawIntBits(f[3]) == 0x7f800000, "float extrema and subnormal");
        d[0] = Double.longBitsToDouble(1L); d[1] = Double.longBitsToDouble(0x7fefffffffffffffL);
        d[2] = Double.longBitsToDouble(0xffefffffffffffffL); d[3] = Double.longBitsToDouble(0x7ff0000000000000L);
        check(Double.doubleToRawLongBits(d[0]) == 1L && Double.doubleToRawLongBits(d[1]) == 0x7fefffffffffffffL && Double.doubleToRawLongBits(d[2]) == 0xffefffffffffffffL && Double.doubleToRawLongBits(d[3]) == 0x7ff0000000000000L, "double extrema and subnormal");
        Object marker = new Object(); a[0] = marker; a[4] = "end";
        check(a[0] == marker && a[1] == null && a[4].equals("end"), "reference identity and neighboring slots");
        j[2] = 7L;
        check(j[2]++ == 7L && j[2] == 8L, "long postfix array increment");
        check(++j[2] == 9L && j[2] == 9L, "long prefix array increment");
        check((j[2] += 0x100000000L) == 0x100000009L, "long array compound assignment result");
        check((j[1] = j[2] = 37L) == 37L && j[1] == 37L && j[2] == 37L, "long chained array assignment");
        d[4] = -2.5;
        check(d[4]++ == -2.5 && d[4] == -1.5, "double postfix array increment");
        check(++d[4] == -0.5 && d[4] == -0.5, "double prefix array increment");
        check((d[4] *= 4.0) == -2.0 && d[4] == -2.0, "double array compound assignment result");
        check((d[1] = d[2] = 1.25) == 1.25 && d[1] == 1.25 && d[2] == 1.25, "double chained array assignment");
        int index = 0; j[0] = 4; long before = j[index++]++;
        check(index == 1 && before == 4 && j[0] == 5, "wide array index evaluated once");
        check(new byte[0].length == 0 && new double[0].length == 0 && new Object[0].length == 0, "zero length arrays");
        boolean caught = false;
        try { b[-1] = 1; } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught, "negative primitive array index");
        caught = false;
        try { long unused = j[j.length]; } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught, "wide array upper bound");
        caught = false;
        try { Object unused = a[a.length]; } catch (ArrayIndexOutOfBoundsException expected) { caught = true; }
        check(caught, "reference array upper bound");
        caught = false;
        try { int[] unused = new int[-1]; } catch (NegativeArraySizeException expected) { caught = true; }
        check(caught, "negative array length");
        caught = false;
        try { byte[] missing = null; byte unused = missing[0]; } catch (NullPointerException expected) { caught = true; }
        check(caught, "null array load");
        caught = false;
        try { double[] missing = null; missing[0] = 1; } catch (NullPointerException expected) { caught = true; }
        check(caught, "null wide array store");
    }

    private static boolean sameElement(Object a, int ai, Object b, int bi, int kind) {
        if (kind == 0) return ((byte[]) a)[ai] == ((byte[]) b)[bi];
        if (kind == 1) return ((boolean[]) a)[ai] == ((boolean[]) b)[bi];
        if (kind == 2) return ((char[]) a)[ai] == ((char[]) b)[bi];
        if (kind == 3) return ((short[]) a)[ai] == ((short[]) b)[bi];
        if (kind == 4) return ((int[]) a)[ai] == ((int[]) b)[bi];
        if (kind == 5) return Float.floatToRawIntBits(((float[]) a)[ai]) == Float.floatToRawIntBits(((float[]) b)[bi]);
        if (kind == 6) return ((long[]) a)[ai] == ((long[]) b)[bi];
        if (kind == 7) return Double.doubleToRawLongBits(((double[]) a)[ai]) == Double.doubleToRawLongBits(((double[]) b)[bi]);
        return ((Object[]) a)[ai] == ((Object[]) b)[bi];
    }

    private static void copiesAndRawBits() {
        Object marker = new Object();
        Object[] sources = {
            new byte[] {-128, -1, 0, 1, 127}, new boolean[] {true, false, true, true, false},
            new char[] {0, '\u007f', '\u8000', '\udfff', '\uffff'}, new short[] {-32768, -1, 0, 1, 32767},
            new int[] {-2147483648, -1, 0, 0x89abcdef, 2147483647},
            new float[] {Float.intBitsToFloat(0x80000000), Float.intBitsToFloat(0x7fc12345), Float.intBitsToFloat(0xffc54321), Float.intBitsToFloat(0x7f800001), 1.25f},
            new long[] {-9223372036854775808L, -1L, 0L, 0x0123456789abcdefL, 9223372036854775807L},
            new double[] {Double.longBitsToDouble(0x8000000000000000L), Double.longBitsToDouble(0x7ff8123456789abcL), Double.longBitsToDouble(0xfff8abcdef012345L), Double.longBitsToDouble(0x7ff0000000000001L), -1.25},
            new Object[] {marker, "two", null, marker, new Object()}
        };
        Object[] targets = {new byte[5], new boolean[5], new char[5], new short[5], new int[5], new float[5], new long[5], new double[5], new Object[5]};
        float[] floats = (float[]) sources[5]; double[] doubles = (double[]) sources[7];
        int[] floatBits = {0x80000000, 0x7fc12345, 0xffc54321, 0x7f800001};
        long[] doubleBits = {0x8000000000000000L, 0x7ff8123456789abcL, 0xfff8abcdef012345L, 0x7ff0000000000001L};
        for (int n = 0; n < 4; n++) {
            check(Float.floatToRawIntBits(floats[n]) == floatBits[n], "float raw store/load bits at " + n);
            check(Double.doubleToRawLongBits(doubles[n]) == doubleBits[n], "double raw store/load bits at " + n);
        }
        for (int kind = 0; kind < sources.length; kind++) {
            Object src = sources[kind], dst = targets[kind];
            System.arraycopy(src, 0, dst, 0, 5);
            for (int n = 0; n < 5; n++) check(sameElement(src, n, dst, n, kind), "full copy kind " + kind + " element " + n);
            System.arraycopy(dst, 0, dst, 1, 4);
            check(sameElement(src, 0, dst, 0, kind), "right overlap preserves first kind " + kind);
            for (int n = 1; n < 5; n++) check(sameElement(src, n - 1, dst, n, kind), "right overlap kind " + kind + " element " + n);
            System.arraycopy(src, 0, dst, 0, 5);
            System.arraycopy(dst, 1, dst, 0, 4);
            for (int n = 0; n < 4; n++) check(sameElement(src, n + 1, dst, n, kind), "left overlap kind " + kind + " element " + n);
            check(sameElement(src, 4, dst, 4, kind), "left overlap preserves last kind " + kind);
            System.arraycopy(src, 5, dst, 5, 0);
            System.arraycopy(dst, 0, dst, 0, 5);
            check(sameElement(src, 1, dst, 0, kind) && sameElement(src, 4, dst, 4, kind), "zero and identity copy kind " + kind);
        }
    }

    // expected: 1 = null, 2 = type, 3 = bounds. A wrong exception propagates.
    private static void rejectsCopy(Object src, int from, Object dst, int to, int count, int expected, String reason) {
        int actual = 0;
        try { System.arraycopy(src, from, dst, to, count); }
        catch (NullPointerException e) { actual = 1; }
        catch (ArrayStoreException e) { actual = 2; }
        catch (IndexOutOfBoundsException e) { actual = 3; }
        check(actual == expected, reason);
    }

    private static void copyErrors() {
        int[] values = {10, 20, 30};
        rejectsCopy(null, 0, values, 0, 0, 1, "null copy source with zero length");
        rejectsCopy(values, 0, null, 0, 0, 1, "null copy destination with zero length");
        rejectsCopy(new Object(), 0, values, 0, 0, 2, "nonarray copy source");
        rejectsCopy(values, 0, new Object(), 0, 0, 2, "nonarray copy destination");
        rejectsCopy(values, 0, new long[3], 0, 0, 2, "primitive type mismatch at zero length");
        rejectsCopy(new byte[3], 0, new boolean[3], 0, 3, 2, "byte and boolean copy incompatibility");
        rejectsCopy(new char[3], 0, new short[3], 0, 3, 2, "char and short copy incompatibility");
        rejectsCopy(new float[3], 0, values, 0, 3, 2, "float and int copy incompatibility");
        rejectsCopy(new double[3], 0, new long[3], 0, 3, 2, "double and long copy incompatibility");
        rejectsCopy(values, 0, new Object[3], 0, 0, 2, "primitive to reference copy incompatibility");
        rejectsCopy(new Object[3], 0, values, 0, 0, 2, "reference to primitive copy incompatibility");
        rejectsCopy(values, -1, values, 0, 1, 3, "negative source copy offset");
        rejectsCopy(values, 0, values, -1, 1, 3, "negative destination copy offset");
        rejectsCopy(values, 0, values, 0, -1, 3, "negative copy length");
        rejectsCopy(values, 3, values, 0, 1, 3, "source copy past end");
        rejectsCopy(values, 0, values, 3, 1, 3, "destination copy past end");
        rejectsCopy(values, 4, values, 0, 0, 3, "source zero copy offset past end");
        rejectsCopy(values, 0, values, 4, 0, 3, "destination zero copy offset past end");
        rejectsCopy(values, 2147483647, values, 0, 1, 3, "source copy offset overflow");
        rejectsCopy(values, 0, values, 2147483647, 1, 3, "destination copy offset overflow");
        rejectsCopy(values, 1, values, 0, 2147483647, 3, "source copy length overflow");
        rejectsCopy(values, 0, values, 1, 2147483647, 3, "destination copy length overflow");
        check(values[0] == 10 && values[1] == 20 && values[2] == 30, "invalid copies leave destination unchanged");
        Object[] source = {"first", null, new Object(), "last"};
        String[] target = {"old0", "old1", "old2", "old3", "old4"};
        rejectsCopy(source, 0, target, 1, 4, 2, "reference copy reports first incompatible element");
        check(target[0].equals("old0") && target[1] == source[0] && target[2] == null, "reference copy commits valid prefix");
        check(target[3].equals("old3") && target[4].equals("old4"), "reference copy preserves incompatible slot and suffix");
        String[] strings = {"a", "b"}; Object[] objects = new Object[2];
        System.arraycopy(strings, 0, objects, 0, 2);
        check(objects[0] == strings[0] && objects[1] == strings[1], "covariant reference copy");
        System.arraycopy(new Object[0], 0, new String[0], 0, 0);
        check(true, "empty reference copy accepts unrelated component classes");
    }

    private static void componentInitialization() {
        check(componentInitializations == 0, "component class starts uninitialized");
        UninitializedComponent[] empty = new UninitializedComponent[0];
        System.arraycopy(empty, 0, new Object[0], 0, 0);
        check(componentInitializations == 0, "zero length widening copy does not initialize component class");
        UninitializedComponent[] values = new UninitializedComponent[2];
        Object[] widened = new Object[2];
        System.arraycopy(values, 0, widened, 0, 2);
        check(componentInitializations == 0 && widened[0] == null && widened[1] == null,
                "all null widening copy does not initialize component class");
        System.arraycopy(new Object[] {null}, 0, values, 1, 1);
        check(componentInitializations == 0 && values[0] == null && values[1] == null,
                "all null narrowing copy does not initialize component class");
    }

    private static void dimensionsAndCovariance() {
        int[][][] cube = new int[2][3][4];
        check(cube.length == 2 && cube[0].length == 3 && cube[1][2].length == 4, "multidimensional lengths");
        check(cube[0] != cube[1] && cube[0][0] != cube[0][1], "multidimensional children are distinct");
        cube[1][2][3] = 12345;
        check(cube[1][2][3] == 12345 && cube[0][2][3] == 0, "multidimensional primitive store");
        long[][][] partial = new long[2][3][];
        check(partial[0] != null && partial[0][0] == null && partial[1][2] == null, "unallocated trailing dimension stays null");
        partial[1][2] = new long[] {0x123456789abcdef0L};
        check(partial[1][2][0] == 0x123456789abcdef0L, "partial multidimensional leaf assignment");
        int[][] ragged = new int[3][]; ragged[0] = new int[0]; ragged[2] = new int[] {7, 8};
        check(ragged[0].length == 0 && ragged[1] == null && ragged[2].length == 2, "ragged null and mixed dimensions");
        check(new byte[2][0].length == 2 && new byte[2][0][3][0].length == 2, "zero inner dimensions");
        boolean caught = false;
        try { int[][] unused = new int[0][-1]; } catch (NegativeArraySizeException expected) { caught = true; }
        check(caught, "negative trailing dimension even after zero dimension");
        Object[] covariant = new String[2]; covariant[0] = "text"; covariant[1] = null;
        check(covariant[0].equals("text") && covariant[1] == null, "covariant reference stores");
        caught = false;
        try { covariant[0] = new Object(); } catch (ArrayStoreException expected) { caught = true; }
        check(caught && covariant[0].equals("text"), "incompatible reference store preserves element");
        Object[] rows = new String[2][]; rows[0] = new String[] {"row"}; rows[1] = null;
        check(((String[]) rows[0])[0].equals("row") && rows[1] == null, "covariant multidimensional stores");
        caught = false;
        try { rows[1] = new Object[1]; } catch (ArrayStoreException expected) { caught = true; }
        check(caught && rows[1] == null, "multidimensional component type checked on store");
        Object[] primitiveRows = new int[2][]; primitiveRows[0] = new int[] {17};
        check(((int[]) primitiveRows[0])[0] == 17, "primitive array is a reference multidimensional component");
        caught = false;
        try { primitiveRows[1] = new long[1]; } catch (ArrayStoreException expected) { caught = true; }
        check(caught, "primitive multidimensional component mismatch");
        Object[][] mixed = new Object[2][]; mixed[0] = new String[1]; mixed[1] = new Object[] {new byte[1], "mixed"};
        check(mixed[0] instanceof String[] && mixed[1][0] instanceof byte[], "mixed reference dimensions");
        Object primitive = new int[1]; Object nested = new int[1][1];
        check(primitive instanceof int[] && !(primitive instanceof Object[]), "primitive array runtime type");
        check(nested instanceof Object[] && nested instanceof int[][] && !(nested instanceof long[][]), "multidimensional runtime type");
        int[][] copied = new int[3][]; System.arraycopy(ragged, 0, copied, 0, 3);
        check(copied[0] == ragged[0] && copied[1] == null && copied[2] == ragged[2], "multidimensional copy retains child identity");
    }

    private static void consumers() throws IOException {
        char[] chars = {'A', 0, '\u0080', '\u20ac', '\uffff'};
        String text = new String(chars);
        check(text.length() == 5 && text.charAt(0) == 'A' && text.charAt(1) == 0 && text.charAt(4) == 65535, "String char array constructor");
        chars[0] = 'Z'; check(text.charAt(0) == 'A', "String constructor copies char source");
        check(new String(chars, 2, 2).charAt(1) == '\u20ac', "String char slice constructor");
        char[] output = text.toCharArray();
        check(output.length == 5 && output[2] == '\u0080' && output[3] == '\u20ac' && output[4] == '\uffff', "String toCharArray compact output");
        char[] destination = new char[7]; text.getChars(1, 5, destination, 2);
        check(destination[0] == 0 && destination[2] == 0 && destination[3] == '\u0080' && destination[5] == '\uffff' && destination[6] == 0, "String getChars offset and guards");
        byte[] bytes = {'x', 'A', 0, 'B', 'y'};
        String ascii = new String(bytes, 1, 3);
        check(ascii.length() == 3 && ascii.charAt(0) == 'A' && ascii.charAt(1) == 0 && ascii.charAt(2) == 'B', "String byte slice constructor");
        bytes[1] = 'Z'; check(ascii.charAt(0) == 'A', "String constructor copies byte source");
        byte[] encoded = ascii.getBytes();
        check(encoded.length == 3 && encoded[0] == 'A' && encoded[1] == 0 && encoded[2] == 'B', "String getBytes compact output");
        byte[] utf8 = {(byte) 0xe2, (byte) 0x82, (byte) 0xac};
        check(new String(utf8, "UTF-8").charAt(0) == '\u20ac', "String UTF8 signed byte input");
        ByteArrayOutputStream stream = new ByteArrayOutputStream(1);
        stream.write(new byte[] {-128, -1, 0, 127}, 0, 4);
        stream.write(255);
        byte[] streamBytes = stream.toByteArray();
        check(streamBytes.length == 5 && streamBytes[0] == -128 && streamBytes[1] == -1 && streamBytes[3] == 127 && streamBytes[4] == -1, "byte stream growth and bulk write");
        ByteArrayInputStream input = new ByteArrayInputStream(streamBytes);
        check(input.read() == 128 && input.read() == 255, "byte stream unsigned scalar read");
        byte[] buffer = new byte[5];
        check(input.read(buffer, 1, 3) == 3 && buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 127 && buffer[3] == -1 && buffer[4] == 0, "byte stream bulk read offset and guards");
        check(input.read() == -1, "byte stream reaches end");
    }

    private static void largeArrays() {
        byte[] bytes = new byte[262144]; byte[] copiedBytes = new byte[262144];
        int[] ints = new int[65536]; int[] copiedInts = new int[65536];
        int[] byteIndexes = {0, 1, 255, 256, 65535, 65536, 131071, 262143};
        int[] intIndexes = {0, 1, 255, 256, 32767, 32768, 65534, 65535};
        check(bytes.length == 262144 && ints.length == 65536, "large compact array lengths");
        for (int n = 0; n < byteIndexes.length; n++) {
            check(bytes[byteIndexes[n]] == 0 && ints[intIndexes[n]] == 0, "large array default sample " + n);
            bytes[byteIndexes[n]] = (byte) (n * 31 - 128);
            ints[intIndexes[n]] = 0x89abcdef + n;
        }
        System.arraycopy(bytes, 0, copiedBytes, 0, bytes.length);
        System.arraycopy(ints, 0, copiedInts, 0, ints.length);
        for (int n = 0; n < byteIndexes.length; n++) {
            check(copiedBytes[byteIndexes[n]] == (byte) (n * 31 - 128) && copiedInts[intIndexes[n]] == 0x89abcdef + n, "large full copy sample " + n);
        }
        check(copiedBytes[131072] == 0 && copiedInts[40000] == 0, "large copy untouched interior");
        System.arraycopy(bytes, 0, bytes, 1, bytes.length - 1);
        System.arraycopy(ints, 0, ints, 1, ints.length - 1);
        check(bytes[65537] == (byte) (5 * 31 - 128) && ints[32769] == 0x89abcdef + 5, "large right overlap crosses width boundaries");
        System.arraycopy(copiedBytes, 1, copiedBytes, 0, copiedBytes.length - 1);
        System.arraycopy(copiedInts, 1, copiedInts, 0, copiedInts.length - 1);
        check(copiedBytes[262142] == (byte) (7 * 31 - 128) && copiedInts[65534] == 0x89abcdef + 7, "large left overlap reaches final element");
    }

    public static int run() throws IOException {
        checks = 0;
        defaultsAndStores();
        copiesAndRawBits();
        copyErrors();
        componentInitialization();
        dimensionsAndCovariance();
        consumers();
        largeArrays();
        return checks;
    }
}
