import javax.microedition.m3g.*;
import javax.microedition.lcdui.*;

/** P2.1-c6: 2D -> 3D -> 2D ordering on one Image, observed through getRGB after every step. */
public class RenderOrderChecks {
    static int checks;
    static void check(boolean c, String label) { if (!c) throw new IllegalStateException(label); checks++; }
    static int px(Image t, int x, int y) { int[] o = new int[1]; t.getRGB(o, 0, 1, x, y, 1, 1); return o[0] & 0xffffff; }
    static VertexBuffer tri(int color) {
        VertexArray p = new VertexArray(3, 3, 2); p.set(0, 3, new short[] {-1, -1, 0, 1, -1, 0, 0, 1, 0});
        VertexBuffer v = new VertexBuffer(); v.setPositions(p, 1, null); v.setDefaultColor(color); return v;
    }
    static Appearance app() { Appearance a = new Appearance(); PolygonMode m = new PolygonMode(); m.setCulling(PolygonMode.CULL_NONE); a.setPolygonMode(m); return a; }
    public static int run() {
        checks = 0;
        Image t = Image.createImage(32, 32); Graphics g = t.getGraphics();
        // 1) 2D
        g.setColor(0x0000ff); g.fillRect(0, 0, 32, 32);
        check(px(t, 16, 16) == 0x0000ff && px(t, 0, 0) == 0x0000ff, "2D fill visible to getRGB");
        // 2) 3D over the 2D content (no clear: the 2D pixels outside the triangle must survive)
        Graphics3D g3 = Graphics3D.getInstance();
        g3.bindTarget(g, true, 0); g3.setCamera(new Camera(), null);
        g3.render(tri(0xffff0000), new TriangleStripArray(0, new int[] {3}), app(), null); g3.releaseTarget();
        check(px(t, 16, 16) == 0xff0000, "3D triangle visible to getRGB after releaseTarget");
        check(px(t, 0, 0) == 0x0000ff, "2D pixels outside the triangle kept through 3D");
        // 3) 2D after 3D
        g.setColor(0xffff00); g.fillRect(14, 14, 4, 4);
        check(px(t, 16, 16) == 0xffff00, "2D after 3D overwrites the 3D pixels");
        check(px(t, 16, 8) == 0xff0000 || px(t, 16, 8) == 0x0000ff, "untouched pixels keep their 3D/2D value");
        int mid = px(t, 16, 8);
        // 4) the composed image used as a 2D source keeps the same order
        Image u = Image.createImage(32, 32); u.getGraphics().drawImage(t, 0, 0, Graphics.TOP | Graphics.LEFT);
        check(px(u, 16, 16) == 0xffff00 && px(u, 0, 0) == 0x0000ff && px(u, 16, 8) == mid, "drawImage copies the final 2D->3D->2D result");
        // 5) second 3D pass, then getRGB of the whole buffer
        g3.bindTarget(g, true, 0); g3.setCamera(new Camera(), null);
        g3.render(tri(0xff00ff00), new TriangleStripArray(0, new int[] {3}), app(), null); g3.releaseTarget();
        int[] all = new int[32 * 32]; t.getRGB(all, 0, 32, 0, 0, 32, 32);
        check((all[16 * 32 + 16] & 0xffffff) == 0x00ff00 && (all[0] & 0xffffff) == 0x0000ff, "full-buffer getRGB after a second 3D pass");
        return checks;
    }
}
