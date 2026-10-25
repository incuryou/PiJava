#pragma once
#include <cstdint>
#include <string>
// A class name is immutable after loading. IDs are runtime-only, never saved.
enum class NativeClassId : int16_t { Unresolved=-1, Other=0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12, C13, C14, C15, C16, C17, C18, C19, C20, C21, C22, C23, C24, C25, C26, C27, Mesh242 };
inline NativeClassId native_class_id(const std::string& name) {
    if(name=="javax/microedition/m3g/Transform")return NativeClassId::C1;
    if(name=="javax/microedition/m3g/Graphics3D")return NativeClassId::C2;
    if(name=="javax/microedition/m3g/Triangle")return NativeClassId::C3;
    if(name=="javax/microedition/m3g/Loader")return NativeClassId::C4;
    if(name=="javax/microedition/m3g/M3GArrays")return NativeClassId::C5;
    if(name=="java/lang/Object")return NativeClassId::C6;
    if(name=="java/lang/Throwable")return NativeClassId::C7;
    if(name=="java/io/ConsoleOutputStream")return NativeClassId::C8;
    if(name=="javax/microedition/lcdui/Graphics")return NativeClassId::C9;
    if(name=="com/nokia/mid/ui/DirectGraphics")return NativeClassId::C10;
    if(name=="javax/microedition/media/NativePlayer")return NativeClassId::C11;
    if(name=="javax/microedition/lcdui/Displayable")return NativeClassId::C12;
    if(name=="javax/microedition/lcdui/Display")return NativeClassId::C13;
    if(name=="javax/microedition/lcdui/Image")return NativeClassId::C14;
    if(name=="javax/microedition/lcdui/game/GameCanvas")return NativeClassId::C15;
    if(name=="javax/microedition/lcdui/Canvas")return NativeClassId::C16;
    if(name=="java/lang/Thread")return NativeClassId::C17;
    if(name=="java/lang/String")return NativeClassId::C18;
    if(name=="java/lang/StringBuilder")return NativeClassId::C19;
    if(name=="java/lang/Math")return NativeClassId::C20;
    if(name=="java/lang/Float")return NativeClassId::C21;
    if(name=="java/lang/Double")return NativeClassId::C22;
    if(name=="java/lang/System")return NativeClassId::C23;
    if(name=="java/lang/Class")return NativeClassId::C24;
    if(name=="javax/microedition/io/Connector")return NativeClassId::C25;
    if(name=="javax/microedition/midlet/MIDlet")return NativeClassId::C26;
    if(name=="javax/microedition/rms/RecordStore")return NativeClassId::C27;
    if(name=="javax/microedition/m3g/M3GMesh242")return NativeClassId::Mesh242;
    return NativeClassId::Other;
}
