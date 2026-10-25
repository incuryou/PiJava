package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class TiledLayer extends Layer {
    Image image;
    int cols, rows, tileW, tileH, tcols;
    int[] cells;
    int[] anim;
    int animCount;
    public TiledLayer(int columns, int rows, Image img, int tileWidth, int tileHeight) {
        super(columns * tileWidth, rows * tileHeight);
        cols = columns; this.rows = rows; image = img; tileW = tileWidth; tileH = tileHeight;
        cells = new int[columns * rows];
        tcols = img.getWidth() / tileWidth; if (tcols < 1) tcols = 1;
    }
    public int getColumns() { return cols; }
    public int getRows() { return rows; }
    public int getCellWidth() { return tileW; }
    public int getCellHeight() { return tileH; }
    public void setCell(int col, int row, int tile) { cells[row * cols + col] = tile; }
    public int getCell(int col, int row) { return cells[row * cols + col]; }
    public void fillCells(int col, int row, int numCols, int numRows, int tile) {
        int r = 0;
        while (r < numRows) {
            int c = 0;
            while (c < numCols) { setCell(col + c, row + r, tile); c = c + 1; }
            r = r + 1;
        }
    }
    public int createAnimatedTile(int staticTile) {
        if (anim == null) anim = new int[8];
        if (animCount >= anim.length) {
            int[] next = new int[anim.length * 2];
            System.arraycopy(anim, 0, next, 0, anim.length);
            anim = next;
        }
        anim[animCount] = staticTile;
        animCount = animCount + 1;
        return -animCount;
    }
    public void setAnimatedTile(int animatedTile, int staticTile) {
        int i = -animatedTile - 1;
        if (i >= 0 && i < animCount) anim[i] = staticTile;
    }
    public int getAnimatedTile(int animatedTile) {
        int i = -animatedTile - 1;
        if (i >= 0 && i < animCount) return anim[i];
        return 0;
    }
    public void setStaticTileSet(Image img, int tileWidth, int tileHeight) {
        image = img; tileW = tileWidth; tileH = tileHeight;
        tcols = img.getWidth() / tileWidth; if (tcols < 1) tcols = 1;
    }
    public final boolean collidesWith(Sprite other, boolean pixelLevel) { return false; }
    public void paint(Graphics g) {
        if (!visible || image == null) return;
        int row = 0;
        while (row < rows) {
            int col = 0;
            while (col < cols) {
                int tile = cells[row * cols + col];
                if (tile < 0) tile = getAnimatedTile(tile);
                if (tile > 0) {
                    int ti = tile - 1;
                    g.drawRegion(image, (ti % tcols) * tileW, (ti / tcols) * tileH, tileW, tileH, 0, x + col * tileW, y + row * tileH, 0);
                }
                col = col + 1;
            }
            row = row + 1;
        }
    }
}
