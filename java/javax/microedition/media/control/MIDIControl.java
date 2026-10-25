package javax.microedition.media.control;
public interface MIDIControl extends javax.microedition.media.Control {
 int CONTROL_CHANGE=0xb0,NOTE_ON=0x90;
 boolean isBankQuerySupported();
 int[] getProgram(int channel) throws javax.microedition.media.MediaException;
 int getChannelVolume(int channel);
 void setChannelVolume(int channel,int volume);
 void setProgram(int channel,int bank,int program);
 void shortMidiEvent(int type,int data1,int data2);
 int longMidiEvent(byte[] data,int offset,int length);
 int[] getBankList(boolean custom) throws javax.microedition.media.MediaException;
 int[] getProgramList(int bank) throws javax.microedition.media.MediaException;
 String getProgramName(int bank,int program) throws javax.microedition.media.MediaException;
 String getKeyName(int bank,int program,int key) throws javax.microedition.media.MediaException;
}
