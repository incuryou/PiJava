package javax.microedition.media.control;
public interface VideoControl extends GUIControl {
 int USE_GUI_PRIMITIVE=0,USE_DIRECT_VIDEO=1;
 Object initDisplayMode(int mode,Object target);
 void setDisplayLocation(int x,int y);
 int getDisplayX(); int getDisplayY();
 void setVisible(boolean visible);
 void setDisplaySize(int width,int height) throws javax.microedition.media.MediaException;
 void setDisplayFullScreen(boolean full) throws javax.microedition.media.MediaException;
 int getSourceWidth();int getSourceHeight();int getDisplayWidth();int getDisplayHeight();
 byte[] getSnapshot(String type) throws javax.microedition.media.MediaException;
}
