program Oracle;
{$mode objfpc}{$H+}{$ASMMODE intel}
uses SysUtils, Math;
const
//Amplitude tables of sound chips
{ (c)Hacker KAY }
 Amplitudes_AY:array[0..15]of Word=
    (0, 836, 1212, 1773, 2619, 3875, 5397, 8823, 10392, 16706, 23339,
    29292, 36969, 46421, 55195, 65535);
{ (c)V_Soft
 Amplitudes_AY:array[0..15]of Word=
    (0, 513, 828, 1239, 1923, 3238, 4926, 9110, 10344, 17876, 24682,
    30442, 38844, 47270, 56402, 65535);}
{ (c)Lion17
 Amplitudes_YM:array[0..31]of Word=
    (0,  30,  190,  286, 375, 470, 560, 664, 866, 1130, 1515, 1803, 2253,
    2848, 3351, 3862, 4844, 6058, 7290, 8559, 10474, 12878, 15297, 17787,
    21500, 26172, 30866, 35676, 42664, 50986, 58842, 65535);}
{ (c)Hacker KAY }
 Amplitudes_YM:array[0..31]of Word=
    (0, 0, $F8, $1C2, $29E, $33A, $3F2, $4D7, $610, $77F, $90A, $A42,
    $C3B, $EC2, $1137, $13A7, $1750, $1BF9, $20DF, $2596, $2C9D, $3579,
    $3E55, $4768, $54FF, $6624, $773B, $883F, $A1DA, $C0FC, $E094, $FFFF);

 PreAmpDef = 127;

type

 TRegisterAY = packed record
 case Integer of
  0:(Index:array[0..15]of byte);
  1:(TonA,TonB,TonC: word;
     Noise:byte;
     Mixer:byte;
     AmplitudeA,AmplitudeB,AmplitudeC:byte;
     Envelope:word;
     EnvType:byte);
 end;

 ChTypes = (No_Chip, AY_Chip, YM_Chip);
type
 TSoundChip = object
   RegisterAY:TRegisterAY;
   First_Period:boolean;
   Ampl:integer;
   Ton_Counter_A, Ton_Counter_B, Ton_Counter_C, Noise_Counter:packed record
    case integer of
     0:(Lo:word;
        Hi:word);
     1:(Re:longword);
    end;
   Envelope_Counter:packed record
    case integer of
    0:(Lo:dword;
       Hi:dword);
    1:(Re:int64);
    end;
   Ton_A,Ton_B,Ton_C:integer;
   Noise:packed record
    case boolean of
    True: (Seed:longword);
    False:(Low:word;
           Val:dword);
    end;
   Case_EnvType:procedure of object;
   Ton_EnA,Ton_EnB,Ton_EnC,Noise_EnA, Noise_EnB,Noise_EnC:boolean;
   Envelope_EnA,Envelope_EnB,Envelope_EnC:boolean;
   Current_RegisterAY:byte;
   procedure Case_EnvType_0_3__9;
   procedure Case_EnvType_4_7__15;
   procedure Case_EnvType_8;
   procedure Case_EnvType_10;
   procedure Case_EnvType_11;
   procedure Case_EnvType_12;
   procedure Case_EnvType_13;
   procedure Case_EnvType_14;
   procedure Synthesizer_Logic_Q;
   procedure SetMixerRegister(Value:byte);
   procedure SetEnvelopeRegister(Value:byte);
   procedure SetAmplA(Value:byte);
   procedure SetAmplB(Value:byte);
   procedure SetAmplC(Value:byte);
   procedure SetAYRegister(Num:integer;Value:byte);
   procedure SetAYRegisterFast(Num:integer;Value:byte);
   procedure Synthesizer_Mixer_Q;

  end;
 TFilt_K = array of integer;
type
 PPT3_Channel_Parameters = ^PT3_Channel_Parameters;

 PT3_Channel_Parameters = record
   Address_In_Pattern,
   OrnamentPointer,
   SamplePointer,
   Ton: word;
   Loop_Ornament_Position,
   Ornament_Length,
   Position_In_Ornament,
   Loop_Sample_Position,
   Sample_Length,
   Position_In_Sample,
   Volume,
   Number_Of_Notes_To_Skip,
   Note,
   Slide_To_Note,
   Amplitude: byte;
   Envelope_Enabled,
   Enabled,
   SimpleGliss: boolean;
   Current_Amplitude_Sliding,
   Ton_Slide_Count,
   Current_OnOff,
   OnOff_Delay,
   OffOn_Delay,
   Ton_Slide_Delay,
   Current_Ton_Sliding,
   Ton_Accumulator,
   Ton_Slide_Step,
   Ton_Delta: smallint;
   Note_Skip_Counter: shortint;
   Current_Noise_Sliding,
   Current_Envelope_Sliding: byte;
 end;

 PPT3_Parameters = ^PT3_Parameters;

 PT3_Parameters = record
   Env_Base: packed record
     case boolean of
       True: (wrd: smallint);
       False: (lo: byte;
         hi: byte);
     end;
   Cur_Env_Slide,
   Env_Slide_Add: smallint;
   Cur_Env_Delay,
   Env_Delay: shortint;
   Noise_Base,
   Delay,
   AddToNoise,
   DelayCounter,
   CurrentPosition: byte;
 end;

type
 ModTypes = packed record case Boolean of
 False:(Index:array[0..65535] of byte);
 True:(PT3_MusicName:array[0..98] of char;PT3_TonTableId,PT3_Delay,PT3_NumberOfPositions,PT3_LoopPosition:byte;PT3_PatternsPointer:word;PT3_SamplesPointers:array[0..31]of word;PT3_OrnamentsPointers:array[0..15]of word;PT3_PositionList:array[0..65535-201]of byte);
 end;
 PModTypes=^ModTypes;
var
 PLConsts:array[0..1]of record RAM:PModTypes;Version,TS,Global_Tick_Counter,Global_Tick_Max:integer;end;
 PlParams:array[0..1]of record PT3:PT3_Parameters;PT3_A,PT3_B,PT3_C:PT3_Channel_Parameters;end;
 SoundChip:array[0..1]of TSoundChip;
 TSMode:Boolean=False;
 ChType:ChTypes=YM_Chip;
 Index_AL:byte=255;Index_AR:byte=13;Index_BL:byte=170;Index_BR:byte=170;Index_CL:byte=13;Index_CR:byte=255;
 PreAmp:byte=127;PreAmpMax:byte=255;Atari_DMAMax:byte=146;BeeperMax:byte=146;
 InterruptHz:double=50;SampleRate:integer=48000;AY_Freq:integer=1773400;SampleBit:integer=16;NumberOfChannels:integer=2;
 Beeper,BeeperLevel,Atari_DMALevel:integer;
 Filt_M,IsFilt,Filt_I:integer;
 Filt_K,Filt_XL,Filt_XR:TFilt_K;
 IntFlag:Boolean;
 Tik,Tick_Counter:packed record case Boolean of False:(Lo:word;Hi:word);True:(Re:integer);end;
 Number_Of_Tiks:packed record case Boolean of False:(lo:longword;hi:longword);True:(re:int64);end;
 Current_Tik,Delay_In_Tiks:longword;
 BufferLength,BuffLen,NOfTicks,VisPoint:integer;
 Level_AR,Level_AL,Level_BR,Level_BL,Level_CR,Level_CL:array[0..31]of Integer;
 PrevLeft,PrevRight,LevelL,LevelR,Left_Chan,Right_Chan,Left_Chan1,Right_Chan1:integer;
type
 TS16=packed array[0..0]of record Left,Right:smallint;end;PS16=^TS16;
procedure Atari_MixDMASnd(var L,R:integer);begin end;
procedure FillVis;begin end;
procedure RaiseBadFileStructure;begin raise Exception.Create('Bad structure');end;
procedure incr(var n:longword);begin Inc(n);if n>=65536 then RaiseBadFileStructure;end;
function CheckLoopAndStop(CNum:integer):boolean;
begin Result:=PLConsts[CNum].Global_Tick_Counter>=PLConsts[CNum].Global_Tick_Max;end;
type PT3ToneTable=array[0..95]of word;PT3VolTable=array[0..15,0..15]of byte;
const
 PT3NoteTable_PT_33_34r: PT3ToneTable = (
   $0C21, $0B73, $0ACE, $0A33, $09A0, $0916, $0893, $0818, $07A4, $0736, $06CE, $066D,
   $0610, $05B9, $0567, $0519, $04D0, $048B, $0449, $040C, $03D2, $039B, $0367, $0336,
   $0308, $02DC, $02B3, $028C, $0268, $0245, $0224, $0206, $01E9, $01CD, $01B3, $019B,
   $0184, $016E, $0159, $0146, $0134, $0122, $0112, $0103, $00F4, $00E6, $00D9, $00CD,
   $00C2, $00B7, $00AC, $00A3, $009A, $0091, $0089, $0081, $007A, $0073, $006C, $0066,
   $0061, $005B, $0056, $0051, $004D, $0048, $0044, $0040, $003D, $0039, $0036, $0033,
   $0030, $002D, $002B, $0028, $0026, $0024, $0022, $0020, $001E, $001C, $001B, $0019,
   $0018, $0016, $0015, $0014, $0013, $0012, $0011, $0010, $000F, $000E, $000D, $000C);

 {Table #0 of Pro Tracker 3.4x - 3.5x}
 PT3NoteTable_PT_34_35: PT3ToneTable = (
   $0C22, $0B73, $0ACF, $0A33, $09A1, $0917, $0894, $0819, $07A4, $0737, $06CF, $066D,
   $0611, $05BA, $0567, $051A, $04D0, $048B, $044A, $040C, $03D2, $039B, $0367, $0337,
   $0308, $02DD, $02B4, $028D, $0268, $0246, $0225, $0206, $01E9, $01CE, $01B4, $019B,
   $0184, $016E, $015A, $0146, $0134, $0123, $0112, $0103, $00F5, $00E7, $00DA, $00CE,
   $00C2, $00B7, $00AD, $00A3, $009A, $0091, $0089, $0082, $007A, $0073, $006D, $0067,
   $0061, $005C, $0056, $0052, $004D, $0049, $0045, $0041, $003D, $003A, $0036, $0033,
   $0031, $002E, $002B, $0029, $0027, $0024, $0022, $0020, $001F, $001D, $001B, $001A,
   $0018, $0017, $0016, $0014, $0013, $0012, $0011, $0010, $000F, $000E, $000D, $000C);

 {Table #1 of Pro Tracker 3.3x - 3.5x)}
 PT3NoteTable_ST: PT3ToneTable = (
   $0EF8, $0E10, $0D60, $0C80, $0BD8, $0B28, $0A88, $09F0, $0960, $08E0, $0858, $07E0,
   $077C, $0708, $06B0, $0640, $05EC, $0594, $0544, $04F8, $04B0, $0470, $042C, $03FD,
   $03BE, $0384, $0358, $0320, $02F6, $02CA, $02A2, $027C, $0258, $0238, $0216, $01F8,
   $01DF, $01C2, $01AC, $0190, $017B, $0165, $0151, $013E, $012C, $011C, $010A, $00FC,
   $00EF, $00E1, $00D6, $00C8, $00BD, $00B2, $00A8, $009F, $0096, $008E, $0085, $007E,
   $0077, $0070, $006B, $0064, $005E, $0059, $0054, $004F, $004B, $0047, $0042, $003F,
   $003B, $0038, $0035, $0032, $002F, $002C, $002A, $0027, $0025, $0023, $0021, $001F,
   $001D, $001C, $001A, $0019, $0017, $0016, $0015, $0013, $0012, $0011, $0010, $000F);

 {Table #2 of Pro Tracker 3.4r}
 PT3NoteTable_ASM_34r: PT3ToneTable = (
   $0D3E, $0C80, $0BCC, $0B22, $0A82, $09EC, $095C, $08D6, $0858, $07E0, $076E, $0704,
   $069F, $0640, $05E6, $0591, $0541, $04F6, $04AE, $046B, $042C, $03F0, $03B7, $0382,
   $034F, $0320, $02F3, $02C8, $02A1, $027B, $0257, $0236, $0216, $01F8, $01DC, $01C1,
   $01A8, $0190, $0179, $0164, $0150, $013D, $012C, $011B, $010B, $00FC, $00EE, $00E0,
   $00D4, $00C8, $00BD, $00B2, $00A8, $009F, $0096, $008D, $0085, $007E, $0077, $0070,
   $006A, $0064, $005E, $0059, $0054, $0050, $004B, $0047, $0043, $003F, $003C, $0038,
   $0035, $0032, $002F, $002D, $002A, $0028, $0026, $0024, $0022, $0020, $001E, $001D,
   $001B, $001A, $0019, $0018, $0015, $0014, $0013, $0012, $0011, $0010, $000F, $000E);

 {Table #2 of Pro Tracker 3.4x - 3.5x}
 PT3NoteTable_ASM_34_35: PT3ToneTable = (
   $0D10, $0C55, $0BA4, $0AFC, $0A5F, $09CA, $093D, $08B8, $083B, $07C5, $0755, $06EC,
   $0688, $062A, $05D2, $057E, $052F, $04E5, $049E, $045C, $041D, $03E2, $03AB, $0376,
   $0344, $0315, $02E9, $02BF, $0298, $0272, $024F, $022E, $020F, $01F1, $01D5, $01BB,
   $01A2, $018B, $0174, $0160, $014C, $0139, $0128, $0117, $0107, $00F9, $00EB, $00DD,
   $00D1, $00C5, $00BA, $00B0, $00A6, $009D, $0094, $008C, $0084, $007C, $0075, $006F,
   $0069, $0063, $005D, $0058, $0053, $004E, $004A, $0046, $0042, $003E, $003B, $0037,
   $0034, $0031, $002F, $002C, $0029, $0027, $0025, $0023, $0021, $001F, $001D, $001C,
   $001A, $0019, $0017, $0016, $0015, $0014, $0012, $0011, $0010, $000F, $000E, $000D);

 {Table #3 of Pro Tracker 3.4r}
 PT3NoteTable_REAL_34r: PT3ToneTable = (
   $0CDA, $0C22, $0B73, $0ACF, $0A33, $09A1, $0917, $0894, $0819, $07A4, $0737, $06CF,
   $066D, $0611, $05BA, $0567, $051A, $04D0, $048B, $044A, $040C, $03D2, $039B, $0367,
   $0337, $0308, $02DD, $02B4, $028D, $0268, $0246, $0225, $0206, $01E9, $01CE, $01B4,
   $019B, $0184, $016E, $015A, $0146, $0134, $0123, $0113, $0103, $00F5, $00E7, $00DA,
   $00CE, $00C2, $00B7, $00AD, $00A3, $009A, $0091, $0089, $0082, $007A, $0073, $006D,
   $0067, $0061, $005C, $0056, $0052, $004D, $0049, $0045, $0041, $003D, $003A, $0036,
   $0033, $0031, $002E, $002B, $0029, $0027, $0024, $0022, $0020, $001F, $001D, $001B,
   $001A, $0018, $0017, $0016, $0014, $0013, $0012, $0011, $0010, $000F, $000E, $000D);

 {Table #3 of Pro Tracker 3.4x - 3.5x}
 PT3NoteTable_REAL_34_35: PT3ToneTable = (
   $0CDA, $0C22, $0B73, $0ACF, $0A33, $09A1, $0917, $0894, $0819, $07A4, $0737, $06CF,
   $066D, $0611, $05BA, $0567, $051A, $04D0, $048B, $044A, $040C, $03D2, $039B, $0367,
   $0337, $0308, $02DD, $02B4, $028D, $0268, $0246, $0225, $0206, $01E9, $01CE, $01B4,
   $019B, $0184, $016E, $015A, $0146, $0134, $0123, $0112, $0103, $00F5, $00E7, $00DA,
   $00CE, $00C2, $00B7, $00AD, $00A3, $009A, $0091, $0089, $0082, $007A, $0073, $006D,
   $0067, $0061, $005C, $0056, $0052, $004D, $0049, $0045, $0041, $003D, $003A, $0036,
   $0033, $0031, $002E, $002B, $0029, $0027, $0024, $0022, $0020, $001F, $001D, $001B,
   $001A, $0018, $0017, $0016, $0014, $0013, $0012, $0011, $0010, $000F, $000E, $000D);

 {Volume table of Pro Tracker 3.3x - 3.4x}
 PT3VolumeTable_33_34: PT3VolTable = (
   ($00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00),
   ($00, $00, $00, $00, $00, $00, $00, $00, $01, $01, $01, $01, $01, $01, $01, $01),
   ($00, $00, $00, $00, $00, $00, $01, $01, $01, $01, $01, $02, $02, $02, $02, $02),
   ($00, $00, $00, $00, $01, $01, $01, $01, $02, $02, $02, $02, $03, $03, $03, $03),
   ($00, $00, $00, $00, $01, $01, $01, $02, $02, $02, $03, $03, $03, $04, $04, $04),
   ($00, $00, $00, $01, $01, $01, $02, $02, $03, $03, $03, $04, $04, $04, $05, $05),
   ($00, $00, $00, $01, $01, $02, $02, $03, $03, $03, $04, $04, $05, $05, $06, $06),
   ($00, $00, $01, $01, $02, $02, $03, $03, $04, $04, $05, $05, $06, $06, $07, $07),
   ($00, $00, $01, $01, $02, $02, $03, $03, $04, $05, $05, $06, $06, $07, $07, $08),
   ($00, $00, $01, $01, $02, $03, $03, $04, $05, $05, $06, $06, $07, $08, $08, $09),
   ($00, $00, $01, $02, $02, $03, $04, $04, $05, $06, $06, $07, $08, $08, $09, $0A),
   ($00, $00, $01, $02, $03, $03, $04, $05, $06, $06, $07, $08, $09, $09, $0A, $0B),
   ($00, $00, $01, $02, $03, $04, $04, $05, $06, $07, $08, $08, $09, $0A, $0B, $0C),
   ($00, $00, $01, $02, $03, $04, $05, $06, $07, $07, $08, $09, $0A, $0B, $0C, $0D),
   ($00, $00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A, $0B, $0C, $0D, $0E),
   ($00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A, $0B, $0C, $0D, $0E, $0F));

 {Volume table of Pro Tracker 3.5x}
 PT3VolumeTable_35: PT3VolTable =
   ( //Amplitude := round((Volume * 17 + byte(Volume > 7)) * Sample_Amplitude / 256);
   ($00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00),
   ($00, $00, $00, $00, $00, $00, $00, $00, $01, $01, $01, $01, $01, $01, $01, $01),
   ($00, $00, $00, $00, $01, $01, $01, $01, $01, $01, $01, $01, $02, $02, $02, $02),
   ($00, $00, $00, $01, $01, $01, $01, $01, $02, $02, $02, $02, $02, $03, $03, $03),
   ($00, $00, $01, $01, $01, $01, $02, $02, $02, $02, $03, $03, $03, $03, $04, $04),
   ($00, $00, $01, $01, $01, $02, $02, $02, $03, $03, $03, $04, $04, $04, $05, $05),
   ($00, $00, $01, $01, $02, $02, $02, $03, $03, $04, $04, $04, $05, $05, $06, $06),
   ($00, $00, $01, $01, $02, $02, $03, $03, $04, $04, $05, $05, $06, $06, $07, $07),
   ($00, $01, $01, $02, $02, $03, $03, $04, $04, $05, $05, $06, $06, $07, $07, $08),
   ($00, $01, $01, $02, $02, $03, $04, $04, $05, $05, $06, $07, $07, $08, $08, $09),
   ($00, $01, $01, $02, $03, $03, $04, $05, $05, $06, $07, $07, $08, $09, $09, $0A),
   ($00, $01, $01, $02, $03, $04, $04, $05, $06, $07, $07, $08, $09, $0A, $0A, $0B),
   ($00, $01, $02, $02, $03, $04, $05, $06, $06, $07, $08, $09, $0A, $0A, $0B, $0C),
   ($00, $01, $02, $03, $03, $04, $05, $06, $07, $08, $09, $0A, $0A, $0B, $0C, $0D),
   ($00, $01, $02, $03, $04, $05, $06, $07, $07, $08, $09, $0A, $0B, $0C, $0D, $0E),
   ($00, $01, $02, $03, $04, $05, $06, $07, $08, $09, $0A, $0B, $0C, $0D, $0E, $0F));

procedure TSoundChip.Case_EnvType_0_3__9;
begin
if First_Period then
 begin
  dec(Ampl);
  if Ampl = 0 then First_Period := False
 end
end;

procedure TSoundChip.Case_EnvType_4_7__15;
begin
if First_Period then
 begin
  Inc(Ampl);
  if Ampl = 32 then
   begin
    First_Period := False;
    Ampl := 0
   end
 end
end;

procedure TSoundChip.Case_EnvType_8;
begin
Ampl := (Ampl - 1) and 31
end;

procedure TSoundChip.Case_EnvType_10;
begin
if First_Period then
 begin
  dec(Ampl);
  if Ampl < 0 then
   begin
    First_Period := False;
    Ampl := 0
   end
 end
else
 begin
  inc(Ampl);
  if Ampl = 32 then
   begin
    First_Period := True;
    Ampl := 31
   end
 end
end;

procedure TSoundChip.Case_EnvType_11;
begin
if First_Period then
 begin
  dec(Ampl);
  if Ampl < 0 then
   begin
    First_Period := False;
    Ampl := 31
   end
 end
end;

procedure TSoundChip.Case_EnvType_12;
begin
Ampl := (Ampl + 1) and 31
end;

procedure TSoundChip.Case_EnvType_13;
begin
if First_Period then
 begin
  inc(Ampl);
  if Ampl = 32 then
   begin
    First_Period := False;
    Ampl := 31
   end
 end
end;

procedure TSoundChip.Case_EnvType_14;
begin
if not First_Period then
 begin
  dec(Ampl);
  if Ampl < 0 then
   begin
    First_Period := True;
    Ampl := 0
   end
 end
else
 begin
  inc(Ampl);
  if Ampl = 32 then
   begin
    First_Period := False;
    Ampl := 31
   end
 end
end;

function NoiseGenerator(Seed:integer):integer;
begin
{$ifdef cpu32}
asm
 shld edx,eax,16
 shld ecx,eax,19
 xor ecx,edx
 and ecx,1
 add eax,eax
 and eax,$1ffff
 inc eax
 xor eax,ecx
end;
{$else}
Result := (((Seed shl 1) or 1) xor ((Seed shr 16) xor (Seed shr 13) and 1)) and $1ffff;
{$endif}
end;

procedure TSoundChip.Synthesizer_Logic_Q;
begin
inc(Ton_Counter_A.Hi);
if Ton_Counter_A.Hi >= RegisterAY.TonA then
 begin
  Ton_Counter_A.Hi := 0;
  Ton_A := Ton_A xor 1
 end;
inc(Ton_Counter_B.Hi);
if Ton_Counter_B.Hi >= RegisterAY.TonB then
 begin
  Ton_Counter_B.Hi := 0;
  Ton_B := Ton_B xor 1
 end;
inc(Ton_Counter_C.Hi);
if Ton_Counter_C.Hi >= RegisterAY.TonC then
 begin
  Ton_Counter_C.Hi := 0;
  Ton_C := Ton_C xor 1
 end;
inc(Noise_Counter.Hi);
if (Noise_Counter.Hi and 1 = 0) and
   (Noise_Counter.Hi >= RegisterAY.Noise shl 1) then
 begin
  Noise_Counter.Hi := 0;
  Noise.Seed := NoiseGenerator(Noise.Seed);
 end;
if Envelope_Counter.Hi = 0 then Case_EnvType;
inc(Envelope_Counter.Hi);
if Envelope_Counter.Hi >= RegisterAY.Envelope then
 Envelope_Counter.Hi := 0;
end;

procedure TSoundChip.SetMixerRegister(Value:byte);
begin
RegisterAY.Mixer := Value;
Ton_EnA := (Value and 1) = 0;
Noise_EnA := (Value and 8) = 0;
Ton_EnB := (Value and 2) = 0;
Noise_EnB := (Value and 16) = 0;
Ton_EnC := (Value and 4) = 0;
Noise_EnC := (Value and 32) = 0;
end;

procedure TSoundChip.SetEnvelopeRegister(Value:byte);
begin
Envelope_Counter.Hi := 0;
First_Period := True;
if (Value and 4) = 0 then
 ampl := 32
else
 ampl := -1;
RegisterAY.EnvType := Value;
Case Value of
0..3,9: Case_EnvType := @Case_EnvType_0_3__9;
4..7,15:Case_EnvType := @Case_EnvType_4_7__15;
8:      Case_EnvType := @Case_EnvType_8;
10:     Case_EnvType := @Case_EnvType_10;
11:     Case_EnvType := @Case_EnvType_11;
12:     Case_EnvType := @Case_EnvType_12;
13:     Case_EnvType := @Case_EnvType_13;
14:     Case_EnvType := @Case_EnvType_14;
end;
end;

procedure TSoundChip.SetAmplA(Value:byte);
begin
RegisterAY.AmplitudeA := Value;
Envelope_EnA := (Value and 16) = 0;
end;

procedure TSoundChip.SetAmplB(Value:byte);
begin
RegisterAY.AmplitudeB := Value;
Envelope_EnB := (Value and 16) = 0;
end;

procedure TSoundChip.SetAmplC(Value:byte);
begin
RegisterAY.AmplitudeC := Value;
Envelope_EnC := (Value and 16) = 0;
end;

procedure TSoundChip.SetAYRegister(Num:integer;Value:byte);
begin
case Num of
13:
 SetEnvelopeRegister(Value and 15);
1,3,5:
 RegisterAY.Index[Num] := Value and 15;
6:
 RegisterAY.Noise := Value and 31;
7: SetMixerRegister(Value and 63);
8: SetAmplA(Value and 31);
9: SetAmplB(Value and 31);
10:SetAmplC(Value and 31);
0,2,4,11,12:
 RegisterAY.Index[Num] := Value;
end
end;

procedure TSoundChip.SetAYRegisterFast(Num:integer;Value:byte);
begin
case Num of
13:
 SetEnvelopeRegister(Value);
1,3,5:
 RegisterAY.Index[Num] := Value;
6:
 RegisterAY.Noise := Value;
7: SetMixerRegister(Value);
8: SetAmplA(Value);
9: SetAmplB(Value);
10:SetAmplC(Value);
0,2,4,11,12:
 RegisterAY.Index[Num] := Value;
end;
end;

//sorry for assembler, I can't make effective qword procedure on pascal...
function ApplyFilter(Lev:integer;var Filt_X:TFilt_K):integer;
{$ifdef cpu32}
begin
asm
        push    ebx
        push    esi
        push    edi
        add     esp,-8
        mov     ecx,Filt_M
        mov     edi,Filt_K
//        lea     esi,edi+ecx*4
        lea     esi,edi[ecx*4] //FPC
        mov     ebx,[edx]
        mov     ecx,Filt_I
        mov     [ebx+ecx*4],eax
        imul    dword ptr [edi]
        mov     [esp],eax
        mov     [esp+4],edx
@lp:    dec     ecx
        jns     @gz
        mov     ecx,Filt_M
//        sets    al
//        dec     eax


@gz:    mov     eax,[ebx+ecx*4]
        add     edi,4
        imul    dword ptr [edi]
        add     [esp],eax
        adc     [esp+4],edx
        cmp     edi,esi
        jnz     @lp
        mov     Filt_I,ecx
        pop     eax
        pop     edx
        pop     edi
        pop     esi
        pop     ebx
        test    edx,edx
        jns     @nm
        add     eax,0FFFFFFh
        adc     edx,0
@nm:    shrd    eax,edx,24
end;
{$else}
var
 Res:int64;
 j:integer;
begin
Filt_X[Filt_I] := Lev;
Res := Lev*Filt_K[0];
for j := 1 to Filt_M do
 begin
  if Filt_I > 0 then
   Dec(Filt_I)
  else
   Filt_I := Filt_M;
  Inc(Res,Filt_X[Filt_I]*Filt_K[j]);
 end;
Result := Res div $1000000;
{$endif}
end;

procedure TSoundChip.Synthesizer_Mixer_Q;
var
 LevL,LevR,k:integer;
begin
LevL := Beeper;
LevR := LevL;

Atari_MixDMASnd(LevL,LevR);

k := 1;
if Ton_EnA then k := Ton_A;
if Noise_EnA then k := k and Noise.Val;
if k <> 0 then
 begin
  if Envelope_EnA then
   begin
    inc(LevL,Level_AL[RegisterAY.AmplitudeA * 2 + 1]);
    inc(LevR,Level_AR[RegisterAY.AmplitudeA * 2 + 1])
   end
  else
   begin
    inc(LevL,Level_AL[Ampl]);
    inc(LevR,Level_AR[Ampl])
   end
 end;

k := 1;
if Ton_EnB then k := Ton_B;
if Noise_EnB then k := k and Noise.Val;
if k <> 0 then
 if Envelope_EnB then
  begin
   inc(LevL,Level_BL[RegisterAY.AmplitudeB * 2 + 1]);
   inc(LevR,Level_BR[RegisterAY.AmplitudeB * 2 + 1])
  end
 else
  begin
   inc(LevL,Level_BL[Ampl]);
   inc(LevR,Level_BR[Ampl])
  end;

k := 1;
if Ton_EnC then k := Ton_C;
if Noise_EnC then k := k and Noise.Val;
if k <> 0 then
 if Envelope_EnC then
  begin
   inc(LevL,Level_CL[RegisterAY.AmplitudeC * 2 + 1]);
   inc(LevR,Level_CR[RegisterAY.AmplitudeC * 2 + 1])
  end
 else
  begin
   inc(LevL,Level_CL[Ampl]);
   inc(LevR,Level_CR[Ampl])
  end;

inc(LevelL,LevL);
inc(LevelR,LevR);
end;

function Interpolator16(l1,l0,ofs:integer):integer;
begin
Result := (l1 - l0) * ofs div 65536 + l0;
if Result > 32767 then
 Result := 32767
else if Result < -32768 then
 Result := -32768;
end;

function Averager16(l:integer):integer;
begin
Result := l div Tick_Counter.Hi;
if Result > 32767 then
 Result := 32767
else if Result < -32768 then
 Result := -32768;
end;

procedure Synthesizer_Stereo16(Buf:pointer);
var
 Tmp:integer;
begin
repeat
Tmp := 0; LevelL := Tmp; LevelR := Tmp;
if Tick_Counter.Re >= Tik.Re then
 begin
  repeat
   if IsFilt > 0 then
    begin
     Tmp := Tik.Re - Tick_Counter.Re + 65536;
     PS16(Buf)^[BuffLen].Left := Interpolator16(Left_Chan,PrevLeft,Tmp);
     PS16(Buf)^[BuffLen].Right := Interpolator16(Right_Chan,PrevRight,Tmp);
    end
   else
    begin
     PS16(Buf)^[BuffLen].Left := Averager16(Left_Chan1);
     PS16(Buf)^[BuffLen].Right := Averager16(Right_Chan1);
    end;
   inc(Tik.Re,integer(Delay_In_Tiks));
   if NOfTicks = VisPoint then FillVis;
   Inc(NOfTicks);
   Inc(BuffLen);
   if BuffLen = BufferLength then
    begin
     if Current_Tik < Number_Of_Tiks.Hi then
      IntFlag := True;
     exit
    end
  until Tick_Counter.Re < Tik.Re; //simple upsampler
  dec(Tik.Re,Tick_Counter.Re);
  Tmp := 0; Left_Chan1 := Tmp; Right_Chan1 := Tmp; Tick_Counter.Re := Tmp;
 end;
SoundChip[0].Synthesizer_Logic_Q;
SoundChip[0].Synthesizer_Mixer_Q;
if TSMode then
 begin
  SoundChip[1].Synthesizer_Logic_Q;
  SoundChip[1].Synthesizer_Mixer_Q;
 end;
if IsFilt >= 0 then
 begin
  Tmp := Filt_I;
  LevelL := ApplyFilter(LevelL,Filt_XL);
  Filt_I := Tmp;
  LevelR := ApplyFilter(LevelR,Filt_XR)
 end;
PrevLeft := Left_Chan;
Left_Chan := LevelL;
inc(Left_Chan1,LevelL);
PrevRight := Right_Chan;
Right_Chan := LevelR;
inc(Right_Chan1,LevelR);

inc(Current_Tik);
Inc(Tick_Counter.Hi);
until Current_Tik >= Number_Of_Tiks.Hi;
Tmp := 0; Number_Of_Tiks.Hi := Tmp; Current_Tik := Tmp
end;

procedure Calculate_Level_Tables;
var
 i,b,l,r:integer;
 Index_A,Index_B,Index_C:integer;
 k:real;
begin
if NumberOfChannels = 2 then
 begin
  Index_A := Index_AL; Index_B := Index_BL; Index_C := Index_CL;
  //DMA or TS - search bigger
  l := Index_AL + Index_BL + Index_CL + Atari_DMAMax;
  r := Index_AR + Index_BR + Index_CR + Atari_DMAMax;
  if l < r then
   l := r;
  r := (Index_AL + Index_BL + Index_CL) * 2;
  if l < r then
   l := r;
  r := (Index_AR + Index_BR + Index_CR) * 2;
  if l < r then
   l := r;
 end
else
 begin
  Index_A := Index_AL + Index_AR; Index_B := Index_BL + Index_BR; Index_C := Index_CL + Index_CR;
  l := Index_A + Index_B + Index_C + Atari_DMAMax;
  r := (Index_A + Index_B + Index_C) * 2;
  if l < r then
   l := r;
 end;
if l = 0 then
 inc(l);
if SampleBit = 8 then
 r := 127
else
 r := 32767;
k := PreAmp / PreAmpMax * 2;
case ChType of
AY_Chip:
 for i := 0 to 15 do
  begin
   b := trunc(Index_A/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_AL[i*2] := b; Level_AL[i*2 + 1] := b;
   b := trunc(Index_AR/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_AR[i*2] := b; Level_AR[i*2 + 1] := b;
   b := trunc(Index_B/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_BL[i*2] := b; Level_BL[i*2 + 1] := b;
   b := trunc(Index_BR/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_BR[i*2] := b; Level_BR[i*2 + 1] := b;
   b := trunc(Index_C/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_CL[i*2] := b; Level_CL[i*2 + 1] := b;
   b := trunc(Index_CR/l*Amplitudes_AY[i]/65535*r*k+0.5);
   Level_CR[i*2] := b; Level_CR[i*2 + 1] := b;
  end;
YM_Chip:
 for i := 0 to 31 do
  begin
   Level_AL[i] := trunc(Index_A/l*Amplitudes_YM[i]/65535*r*k+0.5);
   Level_AR[i] := trunc(Index_AR/l*Amplitudes_YM[i]/65535*r*k+0.5);
   Level_BL[i] := trunc(Index_B/l*Amplitudes_YM[i]/65535*r*k+0.5);
   Level_BR[i] := trunc(Index_BR/l*Amplitudes_YM[i]/65535*r*k+0.5);
   Level_CL[i] := trunc(Index_C/l*Amplitudes_YM[i]/65535*r*k+0.5);
   Level_CR[i] := trunc(Index_CR/l*Amplitudes_YM[i]/65535*r*k+0.5);
  end;
end;
BeeperLevel := -trunc(BeeperMax/l*r*k+0.5);
Atari_DMALevel := trunc(Atari_DMAMax/l*r*k+0.5);
end;

procedure PT3_Get_Registers(CNum: integer);

 function GetNoteFreq(j: integer): integer;
 begin
   case PLConsts[CNum].RAM^.PT3_TonTableId of
     0: if PlConsts[CNum].Version <= 3 then
         Result := PT3NoteTable_PT_33_34r[j]
       else
         Result := PT3NoteTable_PT_34_35[j];
     1: Result := PT3NoteTable_ST[j];
     2: if PlConsts[CNum].Version <= 3 then
         Result := PT3NoteTable_ASM_34r[j]
       else
         Result := PT3NoteTable_ASM_34_35[j];
   else
     if PlConsts[CNum].Version <= 3 then
       Result := PT3NoteTable_REAL_34r[j]
     else
       Result := PT3NoteTable_REAL_34_35[j]
    end;
 end;

 procedure PatternInterpreter(var Chan: PT3_Channel_Parameters);
 var
   quit: boolean;
   Flag9, Flag8, Flag5, Flag4, Flag3, Flag2, Flag1: byte;
   counter, b: byte;
   PrNote, PrSliding: integer;
 begin
   PrNote := Chan.Note;
   PrSliding := Chan.Current_Ton_Sliding;
   quit := False;
   counter := 0;
   Flag9 := 0;
   Flag8 := 0;
   Flag5 := 0;
   Flag4 := 0;
   Flag3 := 0;
   Flag2 := 0;
   Flag1 := 0;
   with Chan, PLConsts[CNum].RAM^ do
    begin
     repeat
       case Index[Address_In_Pattern] of
         $f0..$ff:
          begin
           OrnamentPointer :=
             PT3_OrnamentsPointers[Index[Address_In_Pattern] - $f0];
           Loop_Ornament_Position := Index[OrnamentPointer];
           Inc(OrnamentPointer);
           Ornament_Length := Index[OrnamentPointer];
           Inc(OrnamentPointer);
           Inc(Address_In_Pattern);
           SamplePointer := PT3_SamplesPointers[Index[Address_In_Pattern] div 2];
           Loop_Sample_Position := Index[SamplePointer];
           Inc(SamplePointer);
           Sample_Length := Index[SamplePointer];
           Inc(SamplePointer);
           Envelope_Enabled := False;
           Position_In_Ornament := 0;
          end;
         $d1..$ef:
          begin
           SamplePointer := PT3_SamplesPointers[Index[Address_In_Pattern] - $d0];
           Loop_Sample_Position := Index[SamplePointer];
           Inc(SamplePointer);
           Sample_Length := Index[SamplePointer];
           Inc(SamplePointer);
          end;
         $d0:
           quit := True;
         $c1..$cf:
           Volume := Index[Address_In_Pattern] - $c0;
         $c0:
          begin
           Position_In_Sample := 0;
           Current_Amplitude_Sliding := 0;
           Current_Noise_Sliding := 0;
           Current_Envelope_Sliding := 0;
           Position_In_Ornament := 0;
           Ton_Slide_Count := 0;
           Current_Ton_Sliding := 0;
           Ton_Accumulator := 0;
           Current_OnOff := 0;
           Enabled := False;
           quit := True;
          end;
         $b2..$bf:
          begin
           Envelope_Enabled := True;
           SoundChip[CNum].SetEnvelopeRegister(Index[Address_In_Pattern] - $b1);
           Inc(Address_In_Pattern);
           with PlParams[CNum].PT3 do
            begin
             Env_Base.hi := Index[Address_In_Pattern];
             Inc(Address_In_Pattern);
             Env_Base.lo := Index[Address_In_Pattern];
             Position_In_Ornament := 0;
             Cur_Env_Slide := 0;
             Cur_Env_Delay := 0;
            end;
          end;
         $b1:
          begin
           Inc(Address_In_Pattern);
           Number_Of_Notes_To_Skip := Index[Address_In_Pattern];
          end;
         $b0:
          begin
           Envelope_Enabled := False;
           Position_In_Ornament := 0;
          end;
         $50..$af:
          begin
           Note := Index[Address_In_Pattern] - $50;
           Position_In_Sample := 0;
           Current_Amplitude_Sliding := 0;
           Current_Noise_Sliding := 0;
           Current_Envelope_Sliding := 0;
           Position_In_Ornament := 0;
           Ton_Slide_Count := 0;
           Current_Ton_Sliding := 0;
           Ton_Accumulator := 0;
           Current_OnOff := 0;
           Enabled := True;
           quit := True;
          end;
         $40..$4f:
          begin
           OrnamentPointer :=
             PT3_OrnamentsPointers[Index[Address_In_Pattern] - $40];
           Loop_Ornament_Position := Index[Chan.OrnamentPointer];
           Inc(OrnamentPointer);
           Ornament_Length := Index[OrnamentPointer];
           Inc(OrnamentPointer);
           Position_In_Ornament := 0;
          end;
         $20..$3f:
           PlParams[CNum].PT3.Noise_Base := Index[Address_In_Pattern] - $20;
         $10..$1f:
          begin
           if Index[Address_In_Pattern] = $10 then
             Envelope_Enabled := False
           else
            begin
             SoundChip[CNum].SetEnvelopeRegister(Index[Address_In_Pattern] - $10);
             Inc(Address_In_Pattern);
             with PlParams[CNum].PT3 do
              begin
               Env_Base.hi := Index[Address_In_Pattern];
               Inc(Address_In_Pattern);
               Env_Base.lo := Index[Address_In_Pattern];
               Envelope_Enabled := True;
               Cur_Env_Slide := 0;
               Cur_Env_Delay := 0;
              end;
            end;
           Inc(Address_In_Pattern);
           SamplePointer := PT3_SamplesPointers[Index[Address_In_Pattern] div 2];
           Loop_Sample_Position := Index[SamplePointer];
           Inc(SamplePointer);
           Sample_Length := Index[SamplePointer];
           Inc(SamplePointer);
           Position_In_Ornament := 0;
          end;
         $9:
          begin
           Inc(counter);
           Flag9 := counter;
          end;
         $8:
          begin
           Inc(counter);
           Flag8 := counter;
          end;
         $5:
          begin
           Inc(counter);
           Flag5 := counter;
          end;
         $4:
          begin
           Inc(counter);
           Flag4 := counter;
          end;
         $3:
          begin
           Inc(counter);
           Flag3 := counter;
          end;
         $2:
          begin
           Inc(counter);
           Flag2 := counter;
          end;
         $1:
          begin
           Inc(counter);
           Flag1 := counter;
          end
        end;
       Inc(Address_In_Pattern)
     until quit;
     while counter > 0 do
      begin
       if (counter = Flag1) then
        begin
         Ton_Slide_Delay := Index[Address_In_Pattern];
         Ton_Slide_Count := Ton_Slide_Delay;
         Inc(Address_In_Pattern);
         Ton_Slide_Step := PWord(@Index[Address_In_Pattern])^;
         Inc(Address_In_Pattern, 2);
         SimpleGliss := True;
         Current_OnOff := 0;
         if (Ton_Slide_Count = 0) and (PlConsts[CNum].Version >= 7) then
           Inc(Ton_Slide_Count);
        end
       else if (counter = Flag2) then
        begin
         SimpleGliss := False;
         Current_OnOff := 0;
         Ton_Slide_Delay := Index[Address_In_Pattern];
         Ton_Slide_Count := Ton_Slide_Delay;
         Inc(Address_In_Pattern, 3);
         Ton_Slide_Step := Abs(smallint(PWord(@Index[Address_In_Pattern])^));
         Inc(Address_In_Pattern, 2);
         Ton_Delta := GetNoteFreq(Note) - GetNoteFreq(PrNote);
         Slide_To_Note := Note;
         Note := PrNote;
         if PlConsts[CNum].Version >= 6 then
           Current_Ton_Sliding := PrSliding;
         if Ton_Delta - Current_Ton_Sliding < 0 then
           Ton_Slide_Step := -Ton_Slide_Step;
        end
       else if counter = Flag3 then
        begin
         Position_in_Sample := Index[Address_In_Pattern];
         Inc(Address_In_Pattern);
        end
       else if counter = Flag4 then
        begin
         Position_in_Ornament := Index[Address_In_Pattern];
         Inc(Address_In_Pattern);
        end
       else if counter = Flag5 then
        begin
         OnOff_Delay := Index[Address_In_Pattern];
         Inc(Address_In_Pattern);
         OffOn_Delay := Index[Address_In_Pattern];
         Current_OnOff := OnOff_Delay;
         Inc(Address_In_Pattern);
         Ton_Slide_Count := 0;
         Current_Ton_Sliding := 0;
        end
       else if counter = Flag8 then
        begin
         with PlParams[CNum].PT3 do
          begin
           Env_Delay := Index[Address_In_Pattern];
           Cur_Env_Delay := Env_Delay;
           Inc(Address_In_Pattern);
           Env_Slide_Add := PWord(@Index[Address_In_Pattern])^;
          end;
         Inc(Address_In_Pattern, 2);
        end
       else if counter = Flag9 then
        begin
         b := Index[Address_In_Pattern];
         PlParams[CNum].PT3.Delay := b;
         if TSMode and (PLConsts[1].TS <> $20) then
          begin
           PlParams[0].PT3.Delay := b;
           PlParams[0].PT3.DelayCounter := b;
           PlParams[1].PT3.Delay := b;
          end;
         Inc(Address_In_Pattern);
        end;
       Dec(counter);
      end;
     Note_Skip_Counter := Number_Of_Notes_To_Skip;
    end;
 end;

var
 TempMixer: byte;
 AddToEnv: shortint;

 procedure ChangeRegisters(var Chan: PT3_Channel_Parameters);
 var
   j, b1, b0: byte;
   w: word;
 begin
   with Chan, PLConsts[CNum].RAM^ do
    begin
     if Enabled then
      begin
       Ton := PWord(@Index[SamplePointer + Position_In_Sample * 4 + 2])^;
       Inc(Ton, Ton_Accumulator);
       b0 := Index[SamplePointer + Position_In_Sample * 4];
       b1 := Index[SamplePointer + Position_In_Sample * 4 + 1];
       if b1 and $40 <> 0 then
         Ton_Accumulator := Ton;
       j := Note + Index[OrnamentPointer + Position_In_Ornament];
       if shortint(j) < 0 then j := 0
       else if j > 95 then j := 95;
       w := GetNoteFreq(j);
       Ton := (Ton + Current_Ton_Sliding + w) and $fff;
       if Ton_Slide_Count > 0 then
        begin
         Dec(Ton_Slide_Count);
         if Ton_Slide_Count = 0 then
          begin
           Inc(Current_Ton_Sliding, Ton_Slide_Step);
           Ton_Slide_Count := Ton_Slide_Delay;
           if not SimpleGliss then
             if ((Ton_Slide_Step < 0) and (Current_Ton_Sliding <= Ton_Delta)) or
               ((Ton_Slide_Step >= 0) and (Current_Ton_Sliding >= Ton_Delta)) then
              begin
               Note := Slide_To_Note;
               Ton_Slide_Count := 0;
               Current_Ton_Sliding := 0;
              end;
          end;
        end;
       Amplitude := b1 and $f;
       if b0 and $80 <> 0 then
         if b0 and $40 <> 0 then
          begin
           if Current_Amplitude_Sliding < 15 then
             Inc(Current_Amplitude_Sliding);
          end
         else if Current_Amplitude_Sliding > -15 then
           Dec(Current_Amplitude_Sliding);
       Inc(Amplitude, Current_Amplitude_Sliding);
       if shortint(Amplitude) < 0 then Amplitude := 0
       else if Amplitude > 15 then Amplitude := 15;
       if PlConsts[CNum].Version <= 4 then
         Amplitude := PT3VolumeTable_33_34[Volume, Amplitude]
       else
         Amplitude := PT3VolumeTable_35[Volume, Amplitude];
       if (b0 and 1 = 0) and Envelope_Enabled then
         Amplitude := Amplitude or 16;
       if b1 and $80 <> 0 then
        begin
         if b0 and $20 <> 0 then
           j := (b0 shr 1) or $F0 + Current_Envelope_Sliding
         else
           j := (b0 shr 1) and $F + Current_Envelope_Sliding;
         if b1 and $20 <> 0 then Current_Envelope_Sliding := j;
         Inc(AddToEnv, j);
        end
       else
        begin
         PlParams[CNum].PT3.AddToNoise := b0 shr 1 + Current_Noise_Sliding;
         if b1 and $20 <> 0 then
           Current_Noise_Sliding := PlParams[CNum].PT3.AddToNoise;
        end;
       TempMixer := b1 shr 1 and $48 or TempMixer;
       Inc(Position_In_Sample);
       if Position_In_Sample >= Sample_Length then
         Position_In_Sample := Loop_Sample_Position;
       Inc(Position_In_Ornament);
       if Position_In_Ornament >= Ornament_Length then
         Position_In_Ornament := Loop_Ornament_Position;
      end
     else
       Amplitude := 0;
     TempMixer := TempMixer shr 1;
     if Current_OnOff > 0 then
      begin
       Dec(Current_OnOff);
       if Current_OnOff = 0 then
        begin
         Enabled := not Enabled;
         if Enabled then Current_OnOff := OnOff_Delay
         else
           Current_OnOff := OffOn_Delay;
        end;
      end;
    end;
 end;

var
 i, b: integer;
begin
 if CheckLoopAndStop(CNum) then
   Exit;
 with PlParams[CNum].PT3 do
  begin
   Dec(DelayCounter);
   if DelayCounter = 0 then
    begin
     with PlParams[CNum].PT3_A do
      begin
       Dec(Note_Skip_Counter);
       if Note_Skip_Counter = 0 then
         with PLConsts[CNum].RAM^ do
          begin
           if (Index[Address_In_Pattern] = 0) then
            begin
             Inc(CurrentPosition);
             if CurrentPosition = PT3_NumberOfPositions then
               CurrentPosition := PT3_LoopPosition;
             i := PT3_PositionList[CurrentPosition];
             b := PLConsts[CNum].TS;
             if b <> $20 then i := b * 3 - 3 - i;
             Address_In_Pattern :=
               PWord(@Index[PT3_PatternsPointer + i * 2])^;
             PlParams[CNum].PT3_B.Address_In_Pattern :=
               PWord(@Index[PT3_PatternsPointer + i * 2 + 2])^;
             PlParams[CNum].PT3_C.Address_In_Pattern :=
               PWord(@Index[PT3_PatternsPointer + i * 2 + 4])^;
             Noise_Base := 0;
            end;
           PatternInterpreter(PlParams[CNum].PT3_A);
          end;
      end;
     with PlParams[CNum].PT3_B do
      begin
       Dec(Note_Skip_Counter);
       if Note_Skip_Counter = 0 then
         PatternInterpreter(PlParams[CNum].PT3_B);
      end;
     with PlParams[CNum].PT3_C do
      begin
       Dec(Note_Skip_Counter);
       if Note_Skip_Counter = 0 then
         PatternInterpreter(PlParams[CNum].PT3_C);
      end;
     DelayCounter := Delay;
    end;

   AddToEnv := 0;
   TempMixer := 0;
   ChangeRegisters(PlParams[CNum].PT3_A);
   ChangeRegisters(PlParams[CNum].PT3_B);
   ChangeRegisters(PlParams[CNum].PT3_C);

   SoundChip[CNum].SetMixerRegister(TempMixer);

   SoundChip[CNum].RegisterAY.TonA := PlParams[CNum].PT3_A.Ton;
   SoundChip[CNum].RegisterAY.TonB := PlParams[CNum].PT3_B.Ton;
   SoundChip[CNum].RegisterAY.TonC := PlParams[CNum].PT3_C.Ton;

   SoundChip[CNum].SetAmplA(PlParams[CNum].PT3_A.Amplitude);
   SoundChip[CNum].SetAmplB(PlParams[CNum].PT3_B.Amplitude);
   SoundChip[CNum].SetAmplC(PlParams[CNum].PT3_C.Amplitude);

   SoundChip[CNum].RegisterAY.Noise := (Noise_Base + AddToNoise) and 31;

   SoundChip[CNum].RegisterAY.Envelope := Env_Base.wrd + AddToEnv + Cur_Env_Slide;

   if Cur_Env_Delay > 0 then
    begin
     Dec(Cur_Env_Delay);
     if Cur_Env_Delay = 0 then
      begin
       Cur_Env_Delay := Env_Delay;
       Inc(Cur_Env_Slide, Env_Slide_Add);
      end;
    end;
  end;

 Inc(PlConsts[CNum].Global_Tick_Counter);
end;

procedure GetTimePT3(Module: PModTypes; var Tm, Lp: integer);
var
 b: byte;
 TS: integer;
 vars: array[0..1] of record
   a1, a2, a3, a11, a22, a33: shortint;
   j1, j2, j3: longword;
    end;

 procedure GetPatPtrs(n, i: integer);
 begin
   if (i < 0) or (i > 84 * 3) or (i mod 3 <> 0) then RaiseBadFileStructure;
   with vars[n], Module^ do
    begin
     j1 := PWord(@Index[PT3_PatternsPointer + i * 2])^;
     j2 := PWord(@Index[PT3_PatternsPointer + i * 2 + 2])^;
     j3 := PWord(@Index[PT3_PatternsPointer + i * 2 + 4])^;
    end;
 end;

 function PatInt(n: integer): boolean;
 var
   j, c1, c2, c3, c4, c5, c8: integer;
 begin
   Result := False;
   with vars[n], Module^ do
    begin
     Dec(a1);
     if a1 = 0 then
      begin
       if Index[j1] = 0 then exit;
       j := 0;
       c1 := 0;
       c2 := 0;
       c3 := 0;
       c4 := 0;
       c5 := 0;
       c8 := 0;
       repeat
         case Index[j1] of
           $d0, $c0, $50..$af:
            begin
             a1 := a11;
             incr(j1);
             break;
            end;
           $10, $f0..$ff:
             Inc(j1);
           $b2..$bf:
             Inc(j1, 2);
           $b1:
            begin
             incr(j1);
             a11 := Index[j1];
            end;
           $11..$1f:
             Inc(j1, 3);
           1:
            begin
             Inc(j);
             c1 := j;
            end;
           2:
            begin
             Inc(j);
             c2 := j;
            end;
           3:
            begin
             Inc(j);
             c3 := j;
            end;
           4:
            begin
             Inc(j);
             c4 := j;
            end;
           5:
            begin
             Inc(j);
             c5 := j;
            end;
           8:
            begin
             Inc(j);
             c8 := j;
            end;
           9:
             Inc(j)
          end;
         incr(j1)
       until False;
       while j > 0 do
        begin
         if (j = c1) or (j = c8) then
           Inc(j1, 3)
         else if (j = c2) then
           Inc(j1, 5)
         else if (j = c3) or (j = c4) then
           Inc(j1)
         else if (j = c5) then
           Inc(j1, 2)
         else
          begin
           b := Index[j1];
           Inc(j1);
          end;
         if j1 >= 65536 then RaiseBadFileStructure;
         Dec(j);
        end;
      end;
     Dec(a2);
     if a2 = 0 then
      begin
       j := 0;
       c1 := 0;
       c2 := 0;
       c3 := 0;
       c4 := 0;
       c5 := 0;
       c8 := 0;
       repeat
         case Index[j2] of
           $d0, $c0, $50..$af:
            begin
             a2 := a22;
             incr(j2);
             break;
            end;
           $10, $f0..$ff:
             Inc(j2);
           $b2..$bf:
             Inc(j2, 2);
           $b1:
            begin
             incr(j2);
             a22 := Index[j2];
            end;
           $11..$1f:
             Inc(j2, 3);
           1:
            begin
             Inc(j);
             c1 := j;
            end;
           2:
            begin
             Inc(j);
             c2 := j;
            end;
           3:
            begin
             Inc(j);
             c3 := j;
            end;
           4:
            begin
             Inc(j);
             c4 := j;
            end;
           5:
            begin
             Inc(j);
             c5 := j;
            end;
           8:
            begin
             Inc(j);
             c8 := j;
            end;
           9:
             Inc(j)
          end;
         incr(j2)
       until False;
       while j > 0 do
        begin
         if (j = c1) or (j = c8) then
           Inc(j2, 3)
         else if (j = c2) then
           Inc(j2, 5)
         else if (j = c3) or (j = c4) then
           Inc(j2)
         else if (j = c5) then
           Inc(j2, 2)
         else
          begin
           b := Index[j2];
           Inc(j2);
          end;
         if j2 >= 65536 then RaiseBadFileStructure;
         Dec(j);
        end;
      end;
     Dec(a3);
     if a3 = 0 then
      begin
       j := 0;
       c1 := 0;
       c2 := 0;
       c3 := 0;
       c4 := 0;
       c5 := 0;
       c8 := 0;
       repeat
         case Module^.Index[j3] of
           $d0, $c0, $50..$af:
            begin
             a3 := a33;
             incr(j3);
             break;
            end;
           $10, $f0..$ff:
             Inc(j3);
           $b2..$bf:
             Inc(j3, 2);
           $b1:
            begin
             incr(j3);
             a33 := Index[j3];
            end;
           $11..$1f:
             Inc(j3, 3);
           1:
            begin
             Inc(j);
             c1 := j;
            end;
           2:
            begin
             Inc(j);
             c2 := j;
            end;
           3:
            begin
             Inc(j);
             c3 := j;
            end;
           4:
            begin
             Inc(j);
             c4 := j;
            end;
           5:
            begin
             Inc(j);
             c5 := j;
            end;
           8:
            begin
             Inc(j);
             c8 := j;
            end;
           9:
             Inc(j)
          end;
         incr(j3)
       until False;
       while j > 0 do
        begin
         if (j = c1) or (j = c8) then
           Inc(j3, 3)
         else if (j = c2) then
           Inc(j3, 5)
         else if (j = c3) or (j = c4) then
           Inc(j3)
         else if (j = c5) then
           Inc(j3, 2)
         else
          begin
           b := Index[j3];
           Inc(j3);
          end;
         if j3 >= 65536 then RaiseBadFileStructure;
         Dec(j);
        end;
      end;
    end;
   Result := True;
 end;

var
 i: integer;
 DLCatcher: integer;
begin
 with Module^ do
  begin
   b := PT3_Delay;
   TS := $20;
   if PT3_MusicName[13] in ['7'..'9'] then
     TS := Ord(PT3_MusicName[98]);
   for i := 0 to 1 do
     with vars[i] do
      begin
       a11 := 1;
       a22 := 1;
       a33 := 1;
       DLCatcher := 256 * 256; //max 256 patterns 256 lines per pattern
      end;
   for i := 0 to PT3_NumberOfPositions - 1 do
    begin
     if i = PT3_LoopPosition then Lp := tm;
     GetPatPtrs(0, PT3_PositionList[i]);
     with vars[0] do
      begin
       a1 := 1;
       a2 := 1;
       a3 := 1;
      end;
     if TS <> $20 then
      begin
       GetPatPtrs(1, TS * 3 - 3 - PT3_PositionList[i]);
       with vars[1] do
        begin
         a1 := 1;
         a2 := 1;
         a3 := 1;
        end;
      end;
     repeat
       if not PatInt(0) then break;
       if TS <> $20 then
         if not PatInt(1) then break;
       Inc(tm, b);
       Dec(DLCatcher);
       if DLCatcher < 0 then RaiseBadFileStructure;
     until False;
    end;
  end;
end;

procedure CalcFiltKoefs;
const
 MaxF = 9200;
var
 i: integer;
 K, F, C, i2, Filt_M2: double;
 FKt: array of double;
 s: string;
begin
 //Work range [0..MaxF)
 //Range [MaxF..SampleRate / 2) is easy cut-off from 0 to -53 dB
 //Cut-off range is [SampleRate / 2.. AY_Freq div 8 / 2] (-53 dB)
 //for Ay_Freq = 1773400 Hz:
(*
Полезная область - 0..11083,75 Гц (10)
221675->44100 - 67 (коэффициентов)
221675->48000 - 57
221675->96000 - 20
221675->110000 - 17

Полезная область - 0..10076,14 (11)
221675->22050 - 771

Полезная область - 0..9236,46 (12)
221675->22050 - 409

Полезная область - 0..8525,96 (13)
221675->22050 - 293
*)
 IsFilt := 0;
 C := 22050;
 if SampleRate >= 44100 then
  begin
   C := SampleRate / 2;
   Inc(IsFilt);
  end;
 Filt_M := round(3.3 / (C - MaxF) * (AY_Freq div 8));
 if AY_Freq * Filt_M > 3500000 * 50 then //90% of usage for my Celeron 850 MHz
  begin
   Filt_M := round(3500000 * 50 / AY_Freq);
   IsFilt := 0;
  end;
 C := Pi * (MaxF + C) / (AY_Freq div 8);
 SetLength(FKt, Filt_M);
 Filt_M2 := (Filt_M - 1) / 2;
 K := 0;
 for i := 0 to Filt_M - 1 do
  begin
   i2 := i - Filt_M2;
   if i2 = 0 then
     F := C
   else
     F := sin(C * i2) / i2 * (0.54 + 0.46 * cos(2 * Pi / Filt_M * i2));
   FKt[i] := F;
   K := K + F;
  end;
 SetLength(Filt_K, Filt_M);
 for i := 0 to Filt_M - 1 do
   Filt_K[i] := round(FKt[i] / K * $1000000);
 s := '';


 Dec(Filt_M);
end;

procedure Init;var n,i,b:integer;begin for n:=0 to Ord(TSMode) do begin
     with PlParams[n].PT3, PLConsts[n].RAM^ do
      begin
       DelayCounter := 1;
       Delay := PT3_Delay;
{"FillChared"    CurrentPosition := 0;
    Noise_Base := 0;
    AddToNoise := 0;
    Cur_Env_Slide := 0;
    Cur_Env_Delay := 0;
    Env_Base.wrd := 0}
      end;

     with PLConsts[n].RAM^ do
      begin
       i := PT3_PositionList[0];
       b := PLConsts[n].TS;
       if b <> $20 then i := b * 3 - 3 - i;
       PlParams[n].PT3_A.Address_In_Pattern :=
         PWord(@Index[PT3_PatternsPointer + i * 2])^;
       PlParams[n].PT3_B.Address_In_Pattern :=
         PWord(@Index[PT3_PatternsPointer + i * 2 + 2])^;
       PlParams[n].PT3_C.Address_In_Pattern :=
         PWord(@Index[PT3_PatternsPointer + i * 2 + 4])^;
      end;

     with PlParams[n].PT3_A, PLConsts[n].RAM^ do
      begin
       OrnamentPointer := PT3_OrnamentsPointers[0];
       Loop_Ornament_Position := Index[OrnamentPointer];
       Inc(OrnamentPointer);
       Ornament_Length := Index[OrnamentPointer];
       Inc(OrnamentPointer);
       //extra code begin (pt3 has no default sample anyway)
       SamplePointer := PT3_SamplesPointers[1];
       Loop_Sample_Position := Index[SamplePointer];
       Inc(SamplePointer);
       Sample_Length := Index[SamplePointer];
       Inc(SamplePointer);
       //extra code end
       Volume := 15;
       //"FillChared"     Current_Ton_Sliding := 0;
       Note_Skip_Counter := 1;
{"FillChared"     Current_OnOff := 0;
     Enabled := False;
     Envelope_Enabled := False;
     Note := 0;
     Ton := 0}
      end;

     with PlParams[n].PT3_B do
      begin
       OrnamentPointer := PlParams[n].PT3_A.OrnamentPointer;
       Loop_Ornament_Position := PlParams[n].PT3_A.Loop_Ornament_Position;
       Ornament_Length := PlParams[n].PT3_A.Ornament_Length;
       //extra code begin (pt3 has no default sample anyway)
       SamplePointer := PlParams[n].PT3_A.SamplePointer;
       Loop_Sample_Position := PlParams[n].PT3_A.Loop_Sample_Position;
       Sample_Length := PlParams[n].PT3_A.Sample_Length;
       //extra code end
       Volume := 15;
       //"FillChared"     Current_Ton_Sliding := 0;
       Note_Skip_Counter := 1;
{"FillChared"     Current_OnOff := 0;
     Enabled := False;
     Envelope_Enabled := False;
     Note := 0;
     Ton := 0}
      end;

     with PlParams[n].PT3_C do
      begin
       OrnamentPointer := PlParams[n].PT3_A.OrnamentPointer;
       Loop_Ornament_Position := PlParams[n].PT3_A.Loop_Ornament_Position;
       Ornament_Length := PlParams[n].PT3_A.Ornament_Length;
       //extra code begin (pt3 has no default sample anyway)
       SamplePointer := PlParams[n].PT3_A.SamplePointer;
       Loop_Sample_Position := PlParams[n].PT3_A.Loop_Sample_Position;
       Sample_Length := PlParams[n].PT3_A.Sample_Length;
       //extra code end
       Volume := 15;
       //"FillChared"     Current_Ton_Sliding := 0;
       Note_Skip_Counter := 1;
{"FillChared"     Current_OnOff := 0;
     Enabled := False;
     Envelope_Enabled := False;
     Note := 0;
     Ton := 0}
      end;

end;end;
var module:ModTypes;input,output:file;Tm,Lp,i,size:integer;buf:pointer;
begin
 if ParamCount<>2 then Halt(2);
 SampleRate:=StrToIntDef(GetEnvironmentVariable('ORACLE_RATE'),48000);AY_Freq:=StrToIntDef(GetEnvironmentVariable('ORACLE_CLOCK'),1773400);PreAmp:=StrToIntDef(GetEnvironmentVariable('ORACLE_PREAMP'),127);InterruptHz:=StrToFloatDef(GetEnvironmentVariable('ORACLE_INTERRUPT'),50);
 if GetEnvironmentVariable('ORACLE_AY')='1' then ChType:=AY_Chip;
 FillChar(module,SizeOf(module),0);AssignFile(input,ParamStr(1));Reset(input,1);size:=FileSize(input);if size>65536 then Halt(3);BlockRead(input,module,size);CloseFile(input);
 PLConsts[0].RAM:=@module;PLConsts[0].Version:=6;if module.PT3_MusicName[13] in ['0'..'9'] then PLConsts[0].Version:=Ord(module.PT3_MusicName[13])-$30;PLConsts[0].TS:=$20;if (PLConsts[0].Version>=7) and (Ord(module.PT3_MusicName[98])<>$20) then begin TSMode:=True;PLConsts[1]:=PLConsts[0];PLConsts[1].TS:=Ord(module.PT3_MusicName[98]);end;
 Tm:=0;Lp:=0;GetTimePT3(@module,Tm,Lp);PLConsts[0].Global_Tick_Max:=Tm;
 Init;SoundChip[0].SetEnvelopeRegister(0);SoundChip[0].First_Period:=False;SoundChip[0].Ampl:=0;SoundChip[0].Noise.Seed:=$ffff;SoundChip[0].Noise.Val:=0;
 if TSMode then begin SoundChip[1].SetEnvelopeRegister(0);SoundChip[1].First_Period:=False;SoundChip[1].Ampl:=0;SoundChip[1].Noise.Seed:=$ffff;SoundChip[1].Noise.Val:=0;PLConsts[1].Global_Tick_Max:=Tm;end;
 Calculate_Level_Tables;if (GetEnvironmentVariable('ORACLE_NO_FILTER')='1') or (SampleRate>=AY_Freq div 8) then begin IsFilt:=-1;Filt_M:=0;end else CalcFiltKoefs;SetLength(Filt_XL,Filt_M+1);SetLength(Filt_XR,Filt_M+1);Filt_I:=0;
 Delay_In_Tiks:=round(8192/SampleRate*AY_Freq);Tik.Re:=Delay_In_Tiks;
 BufferLength:=round(Tm*SampleRate/InterruptHz)+1000;GetMem(buf,BufferLength*4);BuffLen:=0;
 for i:=0 to Tm-1 do begin PT3_Get_Registers(0);if TSMode then PT3_Get_Registers(1);Number_Of_Tiks.hi:=round(AY_Freq/InterruptHz/8);Synthesizer_Stereo16(buf);end;
 AssignFile(output,ParamStr(2));Rewrite(output,1);BlockWrite(output,buf^,BuffLen*4);CloseFile(output);FreeMem(buf);
 WriteLn(Tm,' interrupts, ',BuffLen,' frames');
end.
