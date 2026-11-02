/*
 * MIT License
 * Copyright (c) 2026 Yury Kharchenko
 * Adapted for PiJava from MascotME 7659769711c8d93232b3364a00fe9c2c1e581772.
 * Checked little-endian and bit readers plus bounded-buffer resource stream reads.
 * Full MIT license: licenses/mascotme.txt.
 */

package com.mascotcapsule.micro3d.v3;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

class Loader {
	final byte[] data;
	int pos;
	int cached;
	int cache;

	Loader(byte[] bytes) { if(bytes==null)throw new NullPointerException();data = bytes; }
	// PiJava: reject malformed lengths before reads/allocations, without native handles.
	final int remaining() { return data.length-pos; }
	final void require(int bytes) { if(bytes<0 || bytes>remaining())throw new RuntimeException("Truncated Micro3D data"); }
	final int count(int count,int bytesEach) {
		if(count<0 || bytesEach<1 || count>remaining()/bytesEach)throw new RuntimeException("Invalid Micro3D count");
		return count;
	}
	static byte[] resource(Class owner,String name) throws IOException {
		if(name==null)throw new NullPointerException();
		InputStream in=owner.getResourceAsStream(name);
		if(in==null)throw new IOException("Resource not found: "+name);
		ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] buffer=new byte[1024];
		try { int n;while((n=in.read(buffer))!=-1) { if(n==0) { int b=in.read();if(b==-1)break;out.write(b); }else out.write(buffer,0,n); } }
		finally { in.close(); }
		return out.toByteArray();
	}
	
	final void skip(int bytes) {
		require(bytes);
		pos += bytes;
	}

	final int readUByte() {
		require(1);
		return data[pos++] & 0xff;
	}

	final byte readByte() {
		require(1);
		return data[pos++];
	}

	final short readShort() {
		require(2);
		return (short) ((data[pos++] & 0xff) | (data[pos++] << 8));
	}

	final int readUShort() {
		require(2);
		return (data[pos++] & 0xff) | ((data[pos++] & 0xff) << 8);
	}

	final int readInt() { 
		return readUShort() | (readUShort() << 16); 
	}

	final int readUBits(int size) {
		if (size < 0 || size > 25) {
			throw new IllegalArgumentException("Invalid bit size=" + size);
		}
		
		while (size > cached) {
			cache |= readUByte() << cached;
			cached += 8;
		}
		
		int mask = ~(0xffffffff << size);
		int result = cache & mask;
		cached -= size;
		cache >>>= size;
		return result;
	}

	final int readBits(int size) {
		int lzb = 32 - size;
		return (readUBits(size) << lzb) >> lzb;
	}

	final void clearCache() {
		cache = 0;
		cached = 0;
	}
}
