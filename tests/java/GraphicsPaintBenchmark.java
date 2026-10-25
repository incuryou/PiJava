import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/** Fixed, allocation-free paint workload; host timings are not Switch FPS. */
public final class GraphicsPaintBenchmark {
    public static void paint(Graphics g, Image atlas, int count) {
        for (int i = 0; i < count; ++i) {
            int x = (i * 13) % 232;
            int y = (i * 17) % 308;
            g.setClip(x, y, 8, 12);
            g.drawImage(atlas, x - ((i * 7) % 120), y - ((i * 11) % 116), 20);
        }
        g.setClip(0, 0, 240, 320);
    }
}
