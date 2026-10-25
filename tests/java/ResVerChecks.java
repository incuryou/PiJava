// P2.5 fixture: every Java-reachable array/image write path, plus read-only paths that must NOT bump versions.
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class ResVerChecks {
    public static int[] ints = new int[64];
    public static byte[] bytes = new byte[64];
    public static Object[] refs = new Object[8];
    public static int[] dst = new int[64];
    public static Image img;
    public static int sink;
    public static void iastore() { ints[3] = 7; }
    public static void iastoreLoop() { for (int i = 0; i < ints.length; i++) ints[i] = i * 3; }
    public static void bastore() { bytes[5] = (byte) 9; }
    public static void aastore() { refs[1] = "x"; }
    public static void arraycopyPrim() { System.arraycopy(ints, 0, dst, 0, 16); }
    public static void arraycopyRef() { Object[] s = new Object[]{"a", "b"}; System.arraycopy(s, 0, refs, 2, 2); }
    public static void arraycopyEmpty() { System.arraycopy(ints, 0, dst, 0, 0); }
    public static void streamRead() throws Exception {
        java.io.ByteArrayInputStream in = new java.io.ByteArrayInputStream(new byte[]{1, 2, 3, 4});
        in.read(bytes, 10, 4);
    }
    public static void readOnly() { int s = 0; for (int i = 0; i < ints.length; i++) s += ints[i]; s += bytes[5] + dst.length; sink = s; }
    public static void stringFromBytes() { sink = new String(bytes, 0, 4).length(); }
    public static byte[] getBytes() { return "hello".getBytes(); }
    public static void makeImage() { img = Image.createImage(8, 8); }
    public static void drawImage() { Graphics g = img.getGraphics(); g.setColor(0xff0000); g.fillRect(0, 0, 4, 4); }
    public static void reallocInts() { ints = new int[64]; }
}
