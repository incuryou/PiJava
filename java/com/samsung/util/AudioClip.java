package com.samsung.util;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.control.VolumeControl;
/** Resource-backed Samsung audio adapter. MIDI and decoder-supported MP3 are
 * real mixer voices; MMF/SMAF is explicitly unavailable, never a silent success.
 * Loop 0/255 means infinite, 1..254 means that many plays; volume range is 0..5.
 */
public class AudioClip {
 public static final int TYPE_MMF=1,TYPE_MP3=2,TYPE_MIDI=3;
 private Player player;
 private boolean paused;
 private static String type(int t){
  if(t==TYPE_MIDI)return "audio/midi";
  if(t==TYPE_MP3)return "audio/mpeg";
  if(t==TYPE_MMF)throw new IllegalArgumentException("Samsung MMF/SMAF decoder unavailable");
  throw new IllegalArgumentException("Samsung clip type");
 }
 private void load(InputStream in,String mime){
  try {player=Manager.createPlayer(in,mime);player.prefetch();}
  catch(Exception e){if(player!=null)player.close();player=null;throw new IllegalArgumentException("Samsung audio: "+e.toString());}
 }
 public AudioClip(int t,byte[] data,int offset,int length){
  if(data==null)throw new NullPointerException("audio data");
  if(offset<0||length<0||offset>data.length-length)throw new ArrayIndexOutOfBoundsException("audio range");
  load(new ByteArrayInputStream(data,offset,length),type(t));
 }
 public AudioClip(int t,String filename){
  if(filename==null)throw new NullPointerException("audio filename");
  String mime=type(t);InputStream in=getClass().getResourceAsStream(filename);
  if(in==null)throw new IllegalArgumentException("audio resource missing");
  try {load(in,mime);}finally {try{in.close();}catch(Exception ignored){}}
 }
 private void valid(){if(player==null)throw new IllegalStateException("audio closed");}
 public void play(int loop,int volume){
  valid();if(loop<0||loop>255||volume<0||volume>5)throw new IllegalArgumentException("audio loop/volume");
  try {if(player.getState()==Player.STARTED)player.stop();player.setMediaTime(0);
   player.setLoopCount(loop==0||loop==255?-1:loop);
   VolumeControl v=(VolumeControl)player.getControl("VolumeControl");if(v!=null)v.setLevel(volume*20);
   player.start();paused=false;
  }catch(javax.microedition.media.MediaException e){throw new IllegalStateException(e.toString());}
 }
 public void pause(){valid();try{if(player.getState()==Player.STARTED){player.stop();paused=true;}}catch(javax.microedition.media.MediaException e){throw new IllegalStateException(e.toString());}}
 public void resume(){valid();try{if(paused&&player.getMediaTime()<player.getDuration()){player.start();paused=false;}}catch(javax.microedition.media.MediaException e){throw new IllegalStateException(e.toString());}}
 public void stop(){valid();try{if(player.getState()==Player.STARTED)player.stop();player.setMediaTime(0);paused=false;}catch(javax.microedition.media.MediaException e){throw new IllegalStateException(e.toString());}}
 public void close(){if(player!=null){player.close();player=null;}paused=false;}
 public boolean isRunning(){return player!=null&&player.getState()==Player.STARTED;}
}
