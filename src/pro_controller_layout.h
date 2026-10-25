#pragma once
#include "controller_text_fit.h"
namespace host_pro {
constexpr int image_x=748,image_y=151,image_w=460,image_h=318;
inline constexpr std::array<host_controller::Anchor,16> anchors={{
    {host_keymap::ZL,831,188,42,22,889,199,"ZL"},
    {host_keymap::L,823,216,42,22,881,227,"L"},
    {host_keymap::Minus,903,229,34,23,920,259,"-"},
    {host_controller::Menu,831,276,40,24,851,307,"L3"},
    {host_keymap::DUp,887,324,26,23,900,354,""},
    {host_keymap::DLeft,861,350,26,23,874,380,""},
    {host_keymap::DRight,913,350,26,23,926,380,""},
    {host_keymap::DDown,887,376,26,23,900,406,""},
    {host_keymap::ZR,1080,188,42,22,1064,199,"ZR"},
    {host_keymap::R,1092,216,42,22,1076,227,"R"},
    {host_keymap::Plus,1018,229,34,23,1035,259,"+"},
    {host_keymap::X,1087,249,34,23,1104,279,"X"},
    {host_keymap::Y,1050,280,34,23,1067,310,"Y"},
    {host_keymap::A,1120,280,34,23,1137,310,"A"},
    {host_keymap::B,1087,312,34,23,1104,342,"B"},
    {host_keymap::R3,1025,357,40,24,1045,388,"R3"},
}};
inline constexpr std::array<host_controller::ButtonFace,16> button_faces={{
    {host_keymap::ZL,866,198,39,9,{851,192,31,13},{885,191,17,11},"ZL",true},
    {host_keymap::L,855,226,43,9.5,{840,220,31,13},{878,217,17,11},"L",true},
    {host_keymap::Minus,920,244,10,11,{912,235,17,19},{912,258,17,13},"-",false},
    {host_controller::Menu,851,293,23,24,{830,284,43,14},{844,300,15,9},"L3",false},
    {host_keymap::DUp,900,340,10.5,10.5,{892,332,17,17},{892,336,17,13},"",false},
    {host_keymap::DLeft,874,366,10.5,11,{866,357,17,19},{866,362,17,14},"",false},
    {host_keymap::DRight,926,366,10.5,11,{918,357,17,19},{918,362,17,14},"",false},
    {host_keymap::DDown,900,392,10.5,10.5,{892,384,17,17},{892,388,17,13},"",false},
    {host_keymap::ZR,1088,198,39,9,{1073,192,31,13},{1053,191,17,11},"ZR",true},
    {host_keymap::R,1100,226,43,9.5,{1085,220,31,13},{1061,217,17,11},"R",true},
    {host_keymap::Plus,1034,244,10,11,{1026,235,17,19},{1026,258,17,13},"+",false},
    {host_keymap::X,1103,264,16,17,{1089,252,29,14},{1099,268,9,9},"X",false},
    {host_keymap::Y,1067,296,16,17,{1053,284,29,14},{1063,300,9,9},"Y",false},
    {host_keymap::A,1138,296,16,17,{1124,284,29,13},{1134,299,9,9},"A",false},
    {host_keymap::B,1103,328,16,17,{1089,316,29,14},{1099,332,9,9},"B",false},
    {host_keymap::R3,1054,372,24,25,{1041,363,27,19},{1046,380,17,13},"R3",false},
}};
}
