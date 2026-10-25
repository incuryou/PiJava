import java.io.*;

public class IoChecks {
    private static int checks;
    private static void check(boolean ok) {
        checks++;
        if (!ok) throw new RuntimeException("IO check " + checks + " failed");
    }
    private static class SingleByteInput extends InputStream {
        private int left = 3;
        public int read() { return left > 0 ? left-- : -1; }
        public int read(byte[] b, int off, int len) throws IOException {
            if (len == 0) return 0;
            return super.read(b, off, len > 1 ? 1 : len);
        }
    }
    private static class FailingInput extends InputStream {
        private int reads;
        public int read() throws IOException {
            if (reads++ == 0) return 7;
            throw new IOException("deliberate");
        }
    }
    public static int run() throws IOException {
        checks = 0;
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(0);
        DataOutput out = new DataOutputStream(bytes);
        out.writeBoolean(true);
        out.writeBoolean(false);
        out.writeByte(-2);
        out.writeShort(-1234);
        out.writeChar('\u20ac');
        out.writeInt(0x89abcdef);
        out.writeLong(0x0123456789abcdefL);
        out.writeFloat(-12.5f);
        out.writeDouble(0.125d);
        String text = new String(new char[] {'A', 0, '\u0080', '\u07ff', '\u0800', '\ud83d', '\ude00'});
        out.writeUTF(text);
        byte[] encoded = bytes.toByteArray();
        DataInput in = new DataInputStream(new ByteArrayInputStream(encoded));
        check(in.readBoolean());
        check(!in.readBoolean());
        check(in.readByte() == -2);
        check(in.readShort() == -1234);
        check(in.readChar() == '\u20ac');
        check(in.readInt() == 0x89abcdef);
        check(in.readLong() == 0x0123456789abcdefL);
        check(in.readFloat() == -12.5f);
        check(in.readDouble() == 0.125d);
        check(in.readUTF().equals(text));
        boolean caught = false;
        try { in.readUnsignedByte(); } catch (EOFException e) { caught = true; }
        check(caught);
        ByteArrayInputStream input = new ByteArrayInputStream(new byte[] {1, 2, 3, 4}, 1, 2);
        check(input.read() == 2);
        input.mark(10);
        check(input.skip(100) == 1);
        check(input.read(new byte[0], 0, 0) == 0);
        check(input.read() == -1);
        input.reset();
        check(input.read() == 3);
        check(input.skip(-1) == 0);
        input.close();
        input.reset();
        check(input.read() == 3);
        caught = false;
        try { input.read(null, 0, 0); } catch (NullPointerException e) { caught = true; }
        check(caught);
        caught = false;
        try { input.read(new byte[1], 1, 1); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught);
        byte[] target = new byte[3];
        new DataInputStream(new SingleByteInput()).readFully(target);
        check(target[0] == 3 && target[1] == 2 && target[2] == 1);
        caught = false;
        try { new DataInputStream(new ByteArrayInputStream(new byte[1])).readFully(target); }
        catch (EOFException e) { caught = true; }
        check(caught);
        caught = false;
        try { new DataInputStream(new ByteArrayInputStream(new byte[] {1})).readInt(); }
        catch (EOFException e) { caught = true; }
        check(caught);
        check(new FailingInput().read(new byte[2]) == 1);
        bytes.reset();
        DataOutputStream dataOut = new DataOutputStream(bytes);
        dataOut.writeUTF(new String(new char[] {0, '\ud83d', '\ude00'}));
        byte[] utf = bytes.toByteArray();
        check(utf.length == 10 && utf[0] == 0 && utf[1] == 8);
        check((utf[2] & 255) == 192 && (utf[3] & 255) == 128);
        check((utf[4] & 255) == 237 && (utf[7] & 255) == 237);
        check(dataOut.size() == 10);
        caught = false;
        try { new DataInputStream(new ByteArrayInputStream(new byte[] {0, 2, (byte) 192, 65})).readUTF(); }
        catch (UTFDataFormatException e) { caught = true; }
        check(caught);
        caught = false;
        try { new DataInputStream(new ByteArrayInputStream(new byte[] {0, 1, (byte) 224})).readUTF(); }
        catch (UTFDataFormatException e) { caught = true; }
        check(caught);
        caught = false;
        try { new DataInputStream(new ByteArrayInputStream(new byte[] {0, 2, 65})).readUTF(); }
        catch (EOFException e) { caught = true; }
        check(caught);
        caught = false;
        try { bytes.write(new byte[2], -1, 1); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught);
        byte[] copy = bytes.toByteArray();
        copy[0] = 99;
        check(bytes.toByteArray()[0] == 0);
        bytes.reset();
        bytes.close();
        bytes.write(255);
        check(bytes.size() == 1 && bytes.toByteArray()[0] == -1);
        check(new ArrayStoreException() instanceof RuntimeException);
        check(new IllegalThreadStateException() instanceof IllegalArgumentException);
        check(new NoClassDefFoundError() instanceof Error);
        check(new OutOfMemoryError() instanceof VirtualMachineError);
        bytes.reset();
        OutputStreamWriter writer = new OutputStreamWriter(bytes, "UTF-8");
        writer.write(new char[] {'A', '\u0000', '\u20ac', '\ud83d'});
        writer.flush();
        writer.write(new char[] {'\ude00'});
        writer.close();
        check(bytes.size() == 9);
        check(bytes.toString("UTF-8").equals(new String(new char[] {'A', '\u0000', '\u20ac', '\ud83d', '\ude00'})));
        caught = false;
        try { bytes.toString("unsupported-charset"); } catch (UnsupportedEncodingException e) { caught = true; }
        check(caught);
        InputStreamReader reader = new InputStreamReader(new ByteArrayInputStream(bytes.toByteArray()), "UTF8");
        char[] decoded = new char[5];
        int charsRead = reader.read(decoded);
        check(charsRead == 5);
        check(decoded[0] == 'A' && decoded[1] == 0 && decoded[2] == '\u20ac' && decoded[3] == '\ud83d' && decoded[4] == '\ude00');
        check(reader.read() == -1 && reader.read(new char[0]) == 0);
        reader.close();
        check(reader.getEncoding() == null);
        caught = false;
        try { reader.read(); } catch (IOException e) { caught = true; }
        check(caught);
        reader = new InputStreamReader(new ByteArrayInputStream(new byte[] {(byte) 192, 'A'}), "UTF-8");
        check(reader.read() == 65533 && reader.read() == 'A');
        reader = new InputStreamReader(new ByteArrayInputStream(new byte[] {(byte) 226, 'B'}), "UTF-8");
        check(reader.read() == 65533 && reader.read() == 'B');
        caught = false;
        try { new OutputStreamWriter(bytes, "unsupported-charset"); } catch (UnsupportedEncodingException e) { caught = true; }
        check(caught);
        bytes.reset();
        PrintStream print = new PrintStream(bytes);
        print.print("value=");
        print.println(12);
        check(bytes.toString().equals("value=12\n"));
        check(!print.checkError());
        print.close();
        print.write(1);
        check(print.checkError());
        return checks;
    }
}
