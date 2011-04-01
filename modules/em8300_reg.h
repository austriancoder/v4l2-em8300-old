/*
 * em8300_reg.h
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2001-2002 Rick Haines <rick@kuroyi.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
  Register access macros
*/
#define register_ptr(reg) &em->mem[reg]
#define ucregister_ptr(reg) &em->mem[em->ucode_regs[reg]]
#define ucregister(reg) em->ucode_regs[reg]

#define write_register(reg, v) writel(v, &em->mem[reg])
#define read_register(reg) readl(&em->mem[reg])
#define write_ucregister(reg,v) writel(v, &em->mem[em->ucode_regs[reg]])
#define read_ucregister(reg) readl(&em->mem[em->ucode_regs[reg]])

/*
  EM8300 fixed registers
*/

#define DRAM_C0_CONTROL                       0x1c10
#define DRAM_C0_ADD_LO                        0x1c11
#define DRAM_C0_ADD_HI                        0x1c12
#define DRAM_C0_XSIZE                         0x1c13
#define DRAM_C0_YSIZE                         0x1c16

#define DRAM_C3_CONTROL                       0x1c40
#define DRAM_C3_ADD_LO                        0x1c41
#define DRAM_C3_ADD_HI                        0x1c42
#define DRAM_C3_XSIZE                         0x1c43
#define DRAM_C3_YSIZE                         0x1c46

#define I2C_OE                                0x1f4e
#define I2C_PIN                               0x1f4d
#define AUDIO_RATE                            0x1fb0
#define INTERRUPT_ACK                         0x1ffa
#define VIDEO_HSYNC_LO                        0x1f42
#define VIDEO_HSYNC_HI                        0x1f43
#define VIDEO_VSYNC_LO                        0x1f44
#define VIDEO_VSYNC_HI                        0x1f45

/*
  EM8300 microcode dependent registers
*/

#define MV_Command 0
#define MV_Status 1
#define MV_BuffStart_Lo 2
#define MV_BuffStart_Hi 3
#define MV_BuffSize_Lo 4
#define MV_BuffSize_Hi 5
#define MV_RdPtr_Lo 6
#define MV_RdPtr_Hi 7
#define MV_Threshold 8
#define MV_Wrptr_Lo 9
#define MV_Wrptr_Hi 10
#define MV_PCIRdPtr 11
#define MV_PCIWrPtr 12
#define MV_PCISize 13
#define MV_PCIStart 14
#define MV_PTSRdPtr 15
#define MV_PTSSize 16
#define MV_PTSFifo 17
#define MV_SCRSpeed 18
#define MV_SCRlo 19
#define MV_SCRhi 20
#define MV_FrameCntLo 21
#define MV_FrameCntHi 22
#define MV_FrameEventLo 23
#define MV_FrameEventHi 24
#define MV_AccSpeed 25
#define Width_Buf3 26
#define MA_Command 27
#define MA_Status 28
#define MA_BuffStart_Lo 29
#define MA_BuffStart_Hi 30
#define MA_BuffSize 31
#define MA_BuffSize_Hi 32
#define MA_Rdptr 33
#define MA_Rdptr_Hi 34
#define MA_Threshold 35
#define MA_Wrptr 36
#define MA_Wrptr_Hi 37
#define Q_IrqMask 38
#define Q_IrqStatus 39
#define Q_IntCnt 40
#define MA_PCIRdPtr 41
#define MA_PCIWrPtr 42
#define MA_PCISize 43
#define MA_PCIStart 44
#define SP_Command 45
#define SP_Status 46
#define SP_BuffStart_Lo 47
#define SP_BuffStart_Hi 48
#define SP_BuffSize_Lo 49
#define SP_BuffSize_Hi 50
#define SP_RdPtr_Lo 51
#define SP_RdPtr_Hi 52
#define SP_Wrptr_Lo 53
#define SP_Wrptr_Hi 54
#define SP_PCIRdPtr 55
#define SP_PCIWrPtr 56
#define SP_PCISize 57
#define SP_PCIStart 58
#define SP_PTSRdPtr 59
#define SP_PTSSize 60
#define SP_PTSFifo 61
#define DICOM_DisplayBuffer 62
#define Vsync_DBuf 63
#define DICOM_TvOut 64
#define DICOM_UpdateFlag 65
#define DICOM_VSyncLo1 66
#define DICOM_VSyncLo2 67
#define DICOM_VSyncDelay1 68
#define DICOM_VSyncDelay2 69
#define DICOM_Display_Data 70
#define PicPTSLo 71
#define PicPTSHi 72
#define Error_Code 73
#define DisplayHorSize 74
#define Line21Buf1_Cnt 75
#define Line21Buf2_Cnt 76
#define TimeCodeHi 77
#define TimeCodeLo 78
#define AUTH_Challenge 79
#define AUTH_Response 80
#define AUTH_Command 81
#define Timer_Cnt 82
#define Ovl_Addr 83
#define Button_Color 84
#define Button_Contrast 85
#define Button_Top 86
#define Button_Bottom 87
#define Button_Left 88
#define Button_Right 89
#define SP_Palette 90
#define DICOM_FrameTop 91
#define DICOM_FrameBottom 92
#define DICOM_FrameLeft 93
#define DICOM_FrameRight 94
#define DICOM_VisibleTop 95
#define DICOM_VisibleBottom 96
#define DICOM_VisibleLeft 97
#define DICOM_VisibleRight 98
#define DICOM_BCSLuma 99
#define DICOM_BCSChroma 100
#define DICOM_Control 101
#define DICOM_Controlx 102
#define MV_CryptKey 103
#define DICOM_Kmin 104
#define MicroCodeVersion 105
#define ForcedLeftParity 106
#define L21_Buf1 107
#define L21_Buf2 108
#define Mute_Pattern 109
#define Mute_Patternrityhtm 110

