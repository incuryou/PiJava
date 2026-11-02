/*
 * MIT License
 * Copyright (c) 2026 Yury Kharchenko
 * Copyright (c) 2026 Roman Lahin
 * Adapted for PiJava from MascotME 7659769711c8d93232b3364a00fe9c2c1e581772.
 * Checked tracks and counts; full 16.16 interpolation, endpoint clamping and per-action scratch.
 * Full MIT license: licenses/mascotme.txt.
 */

package com.mascotcapsule.micro3d.v3;

import java.io.IOException;

public class ActionTable {
	Action[] actions;

	public ActionTable(byte[] data) {
		if (data == null) throw new NullPointerException();
		loadMtraData(data);
	}

	public ActionTable(String name) throws IOException {
		this(Loader.resource(ActionTable.class,name));
	}
	
	private final void loadMtraData(byte[] bytes) {
		Loader loader = new Loader(bytes);

		if (loader.readUByte() != 'M' || loader.readUByte() != 'T') {
			throw new RuntimeException("Not a MTRA file");
		}

		int version = loader.readUByte();
		if (loader.readUByte() != 0 || version < 2 || version > 5) {
			throw new RuntimeException("Unsupported version: " + version);
		}

		int numActions = loader.readUShort();
		int numBones = loader.readUShort();
		
		// Every action has at least its keyframe count and one type byte per bone.
		if(numActions>loader.remaining()/2)throw new RuntimeException("Invalid MTRA action count");
		actions = new Action[numActions];
		
		//Number of bones by transform types
		int[] transTypeCounts = new int[8];
		for (int i = 0; i < 8; i++) {
			transTypeCounts[i] = loader.readUShort();
		}

		int dataSize = loader.readInt();
		//dataSize and transTypeCounts may be used for allocate memory and verify data

		for (int action = 0; action < numActions; action++) {
			int keyframes = loader.readUShort();
			loader.count(numBones,1);
			Action act = new Action(keyframes, numBones);
			actions[action] = act;

			for (int bone = 0; bone < numBones; bone++) {
				act.boneAnims[bone] = readBoneAnim(loader);
			}
			
			if (version < 5) continue;
			
			//Dynamic polygons chunk
			int count = loader.readUShort();
			loader.count(count,6);
			int[] dynamicPolys = new int[count * 2];
			actions[action].dynamicPolys = dynamicPolys;
			
			for (int j = 0; j < count; j++) {
				int frame = loader.readUShort();
				int pattern = loader.readInt();
				if(j>0&&frame<dynamicPolys[(j-1)*2])throw new RuntimeException("Unsorted MTRA pattern keys");
				
				dynamicPolys[j * 2] = frame;
				dynamicPolys[j * 2 + 1] = pattern;
			}
		}
	}
	
	private final BoneAnim readBoneAnim(Loader loader) {
		int type = loader.readUByte();
		BoneAnim boneAnim = new BoneAnim(type);
		
		switch (type) {
			case 0:
				int[] tmpTrans = new int[12];
				for(int i = 0; i < 12; i++) tmpTrans[i] = loader.readShort();
				boneAnim.transform = new AffineTrans(tmpTrans);
				break;
			case 1:
				//Identity transform
				break;
			case 2: {
				boneAnim.translate = readFrames3D(loader);
				boneAnim.scale = readFrames3D(loader);
				boneAnim.rotate = readFrames3D(loader);
				boneAnim.roll = readFrames1D(loader);
				break;
			}
			case 3: {
				boneAnim.translate = readFrames3DConst(loader);
				boneAnim.rotate = readFrames3D(loader);
				boneAnim.roll = readFrames1DConst(loader);
				break;
			}
			case 4: {
				boneAnim.rotate = readFrames3D(loader);
				boneAnim.roll = readFrames1D(loader);
				break;
			}
			case 5: {
				boneAnim.rotate = readFrames3D(loader);
				break;
			}
			case 6: {
				boneAnim.translate = readFrames3D(loader);
				boneAnim.rotate = readFrames3D(loader);
				boneAnim.roll = readFrames1D(loader);
				break;
			}
			default:
				throw new RuntimeException("Invalid animation type: " + type);
		}
		
		return boneAnim;
	}
	
	private final short[] readFrames3D(Loader loader) {
		int count = loader.readUShort();
		if(count==0)throw new RuntimeException("Empty MTRA track");loader.count(count,8);
		short[] frames = new short[count * 4];
		
		//Keyframe, X, Y, Z
		for (int j = 0; j < count * 4; j += 4) {
			frames[j] = loader.readShort();
			frames[j + 1] = loader.readShort();
			frames[j + 2] = loader.readShort();
			frames[j + 3] = loader.readShort();
			if(j>0&&(frames[j]&65535)<=(frames[j-4]&65535))throw new RuntimeException("Unsorted MTRA track");
		}
		
		return frames;
	}
	
	private final short[] readFrames3DConst(Loader loader) {
		return new short[] {loader.readShort(), loader.readShort(), loader.readShort()};
	}
	
	private final short[] readFrames1D(Loader loader) {
		int count = loader.readUShort();
		if(count==0)throw new RuntimeException("Empty MTRA track");loader.count(count,4);
		short[] frames = new short[count * 2];
		
		//Keyframe, X
		for (int j = 0; j < count * 2; j += 2) {
			frames[j] = loader.readShort();
			frames[j + 1] = loader.readShort();
			if(j>0&&(frames[j]&65535)<=(frames[j-2]&65535))throw new RuntimeException("Unsorted MTRA track");
		}
		
		return frames;
	}
	
	private final short[] readFrames1DConst(Loader loader) {
		return new short[] {loader.readShort()};
	}

	public final void dispose() {
		actions = null;
	}

	public final int getNumAction() {
		return actions.length;
	}

	public final int getNumActions() {
		return actions.length;
	}

	public final int getNumFrame(int idx) {
		return getNumFrames(idx);
	}

	public final int getNumFrames(int idx) {
		if (idx < 0 || idx >= actions.length) {
			throw new IllegalArgumentException();
		}
		
		return actions[idx].keyFrames << 16;
	}

	static class Action {
		int keyFrames;
		BoneAnim[] boneAnims;
		int[] dynamicPolys;
		
		Action(int keyFrames, int bones) { 
			this.keyFrames = keyFrames; 
			boneAnims = new BoneAnim[bones];
		}
		
		final void updateBoneAnim(int boneIdx, int frame, AffineTrans localTrans, AffineTrans resultTrans) {
			if (boneIdx >= boneAnims.length) {
				if(localTrans==null)resultTrans.setIdentity();else resultTrans.set(localTrans);
				return;
			}
			boneAnims[boneIdx].setFrame(frame, localTrans, resultTrans);
		}
	
		final int getPattern(int frame, int defValue) {
			int[] dynamic = dynamicPolys;
			if (dynamic == null) return defValue;

			int iFrame = frame < 0 ? 0 : frame >> 16;
			for (int i = dynamic.length - 2; i >= 0; i -= 2) {
				if (dynamic[i] <= iFrame) return dynamic[i + 1];
			}

			return defValue;
		}
	}

	static class BoneAnim {
		Vector3D tmp = new Vector3D();
		AffineTrans tmpTrans = new AffineTrans();
		
		int type;
		AffineTrans transform;
		short[] translate, scale, rotate, roll;
		
		BoneAnim(int type) { this.type = type; }
		
		// PiJava: full 16.16 frames, endpoint clamping and long intermediates.
		private static int interpolate(int a,int b,long offset,long span) {
			long n=(long)(b-a)*offset;
			return a+(int)(n>=0?n/span:-((-n+span-1)/span));
		}
		final void interp3D(int frame,short[] values,Vector3D out) {
			int last=values.length-4,index=0;
			if((long)frame>=((long)(values[last]&65535)<<16))index=last;
			else if((long)frame>((long)(values[0]&65535)<<16)) {
				for(index=last-4;index>0&&((long)(values[index]&65535)<<16)>frame;index-=4) {}
				long offset=(long)frame-((long)(values[index]&65535)<<16);
				long span=(long)((values[index+4]&65535)-(values[index]&65535))<<16;
				out.set(interpolate(values[index+1],values[index+5],offset,span),interpolate(values[index+2],values[index+6],offset,span),interpolate(values[index+3],values[index+7],offset,span));
				return;
			}
			out.set(values[index+1],values[index+2],values[index+3]);
		}
		final int interp1D(int frame,short[] values) {
			int last=values.length-2;
			if((long)frame>=((long)(values[last]&65535)<<16))return values[last+1];
			if((long)frame<=((long)(values[0]&65535)<<16))return values[1];
			int index;
			for(index=last-2;index>0&&((long)(values[index]&65535)<<16)>frame;index-=2) {}
			long offset=(long)frame-((long)(values[index]&65535)<<16);
			long span=(long)((values[index+2]&65535)-(values[index]&65535))<<16;
			return interpolate(values[index+1],values[index+3],offset,span);
		}
		
		final void setFrame(int frame, AffineTrans localTrans, AffineTrans resultTrans) {
			Vector3D tmp = this.tmp;
			AffineTrans tmpTrans = this.tmpTrans;
			if (localTrans == null) tmpTrans = resultTrans;
			
			switch (type) {
				case 0: {
					if (localTrans != null) resultTrans.mulA2(localTrans, transform);
					else resultTrans.set(transform);
					return;
				}
				case 1: {
					if (localTrans != null) resultTrans.set(localTrans);
					else resultTrans.setIdentity();
					return;
				}
				case 2: {
					//Translate
					interp3D(frame, translate, tmp);
					tmpTrans.m03 = tmp.x;
					tmpTrans.m13 = tmp.y;
					tmpTrans.m23 = tmp.z;

					//Rotate
					interp3D(frame, rotate, tmp);
					rotate(tmpTrans, tmp);

					//Roll
					roll(tmpTrans, interp1D(frame, roll));

					//Scale
					interp3D(frame, scale, tmp);
					tmpTrans.scale(tmp);
					break;
				}
				case 3: {
					//Translate (for all frames)
					tmpTrans.m03 = translate[0];
					tmpTrans.m13 = translate[1];
					tmpTrans.m23 = translate[2];

					//Rotate
					interp3D(frame, rotate, tmp);
					rotate(tmpTrans, tmp);

					//Roll (for all frames)
					roll(tmpTrans, roll[0]);
					break;
				}
				case 4: {
					//Translate
					tmpTrans.m03 = tmpTrans.m13 = tmpTrans.m23 = 0;
					
					//Rotate
					interp3D(frame, rotate, tmp);
					rotate(tmpTrans, tmp);

					//Roll
					roll(tmpTrans, interp1D(frame, roll));
					break;
				}
				case 5: {
					//Translate
					tmpTrans.m03 = tmpTrans.m13 = tmpTrans.m23 = 0;
					
					//Rotate
					interp3D(frame, rotate, tmp);
					rotate(tmpTrans, tmp);
					break;
				}
				case 6: {
					//Translate
					interp3D(frame, translate, tmp);
					tmpTrans.m03 = tmp.x;
					tmpTrans.m13 = tmp.y;
					tmpTrans.m23 = tmp.z;

					//Rotate
					interp3D(frame, rotate, tmp);
					rotate(tmpTrans, tmp);

					//Roll
					roll(tmpTrans, interp1D(frame, roll));
					break;
				}
			}
			
			if (localTrans != null) resultTrans.mulA2(localTrans, tmpTrans);
		}

		private final void rotate(AffineTrans trans, Vector3D v) {
			//Normalize direction vector
			if((v.x|v.y|v.z)==0)v.z=4096;
			v.unit();
			int x = v.x, y = v.y, z = v.z;

			int xx = (x * x + 2048) >> 12;
			int yy = (y * y + 2048) >> 12;
			
			if (xx > 0 || yy > 0) {
				int a = ((4096 - z) << 12) / (yy + xx);
				int b = (a * -((x * y + 2048) >> 12)) >> 12;
				
				trans.m00 = z + ((yy * a + 2048) >> 12);
				trans.m01 = b;
				trans.m02 = x;
				trans.m10 = b;
				trans.m11 = z + ((xx * a + 2048) >> 12);
				trans.m12 = y;
				trans.m20 = -x;
				trans.m21 = -y;
			} else {
				trans.m00 = 4096;
				trans.m01 = 0;
				trans.m02 = 0;
				trans.m10 = 0;
				trans.m11 = z;
				trans.m12 = 0;
				trans.m20 = 0;
				trans.m21 = 0;
			}
			
			trans.m22 = z;
		}

		private final void roll(AffineTrans trans, int angle) {
			int s = Util3D.sine(angle,4096);
			int c = Util3D.sine(angle+1024,4096);

			int m00 = trans.m00;
			int m01 = trans.m01;
			int m10 = trans.m10;
			int m11 = trans.m11;
			int m20 = trans.m20;
			int m21 = trans.m21;

			trans.m00 = (m00 * c + m01 * s + 2048) >> 12;
			trans.m01 = (m01 * c - m00 * s + 2048) >> 12;
			trans.m10 = (m10 * c + m11 * s + 2048) >> 12;
			trans.m11 = (m11 * c - m10 * s + 2048) >> 12;
			trans.m20 = (m20 * c + m21 * s + 2048) >> 12;
			trans.m21 = (m21 * c - m20 * s + 2048) >> 12;
		}
	}
}
