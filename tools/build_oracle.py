#!/usr/bin/env python3
"""Assemble a minimal FPC diagnostic oracle from immutable source text.
No player, chip, mixer, FIR or quantizer algorithm is translated or regenerated.
Only GUI/CPU hooks absent on native PT3 path are replaced with no-op stubs.
"""
import pathlib,re,hashlib,json
root=pathlib.Path(__file__).resolve().parents[1]
a=(root/'docs/Ay_Emul/AY.pas').read_text();p=(root/'docs/Ay_Emul/Players.pas').read_text();m=(root/'docs/Ay_Emul/MainWin.pas').read_text()
def between(s,start,end):return s[s.index(start):s.index(end,s.index(start))]
head='''program Oracle;
{$mode objfpc}{$H+}{$ASMMODE intel}
uses SysUtils, Math;
'''
head+=between(a,'const\n//Amplitude tables','//Available soundchips')
head+=' ChTypes = (No_Chip, AY_Chip, YM_Chip);\n'
obj=between(a,' TSoundChip = object',' TFilt_K =')
obj=obj.replace('   procedure Synthesizer_Mixer_Q_Mono;','')
head+='type\n'+obj+' TFilt_K = array of integer;\n'
head+='type\n'+between(p,' PPT3_Channel_Parameters =',' PPT2_Channel_Parameters =')
head+='''type
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
'''
# Active tables are copied verbatim, including original comments.
head+='type PT3ToneTable=array[0..95]of word;PT3VolTable=array[0..15,0..15]of byte;\nconst\n'+between(p,' PT3NoteTable_PT_33_34r:',' st1nts:')
# Source methods: unchanged, no instrumentation required for PCM.
head+=between(a,'procedure TSoundChip.Case_EnvType_0_3__9;\nbegin','procedure FillVis;')
head+=between(a,'function Interpolator16','function Interpolator8')
head+=between(a,'function Averager16','function Averager8')
head+=between(a,'procedure Synthesizer_Stereo16(Buf:pointer);\nvar','procedure Synthesizer_Stereo8(Buf:pointer);\nvar')
head+=between(a,'procedure Calculate_Level_Tables;\nvar','procedure Get_Max_of_Level_Tables(out Max:integer);\nvar')
head+=between(p,'procedure PT3_Get_Registers(CNum: integer);\n\n function','procedure MakeBufferVTX(Buf: pointer);\nbegin')
head+=between(p,'procedure GetTimePT3(Module: PModTypes; var Tm, Lp: integer);\nvar','procedure GetTimePSC(Module:')
filter_code=between(m,'procedure TFrmMain.CalcFiltKoefs;\nconst','procedure TFrmMain.SetFilter(FQ:')
filter_code=filter_code.replace('procedure TFrmMain.CalcFiltKoefs;','procedure CalcFiltKoefs;')
filter_code=filter_code.replace(' s := Mes_FIR + \' (\' + IntToStr(Filt_M) + \' \' + Mes_PTS + \')\';',' s := \'\';').replace(" if IsFilt = 0 then s := s + ' + ' + LowerCase(Mes_Averager);",'').replace(' FrmMixer.Label13.Caption := s;','')
head+=filter_code
init=between(p,'     with PlParams[n].PT3, PLConsts[n].RAM^ do','    end\n   else if FType = FT.SQT then')
head+='procedure Init;var n,i,b:integer;begin for n:=0 to Ord(TSMode) do begin\n'+init+'end;end;\n'
head+='''var module:ModTypes;input,output:file;Tm,Lp,i,size:integer;buf:pointer;
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
'''
folder=root/'reference/oracle';folder.mkdir(exist_ok=True)
(folder/'oracle.pas').write_text(head)
manifest={'kind':'diagnostic minimal source adapter','source_files':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [root/'docs/Ay_Emul/AY.pas',root/'docs/Ay_Emul/Players.pas',root/'docs/Ay_Emul/MainWin.pas']},'changes':'Selected native PT3 routines copied verbatim. GUI/CPU/visualizer hooks stubbed only where inactive; source CalcFiltKoefs caption statements removed. Initialization and raw output in adapter. No original source modified. Not the unknown original AY_Emul executable.'}
(folder/'provenance.json').write_text(json.dumps(manifest,indent=2))
print(folder/'oracle.pas')
# Separate observer-only copy. Instrument setters and direct packed register writes.
instrumented=head
observer='''var trace:Text;TraceEnabled:Boolean=False;SourceTick,Ordinal:integer;
procedure Observe(reg,value:integer;chip:integer=0);
begin
 if not TraceEnabled then Exit;
 WriteLn(trace,'{"tick":',Int64(SourceTick)*round(AY_Freq/InterruptHz/8),',"ordinal":',Ordinal,',"chip":',chip,',"register":',reg,',"value":',value and 255,'}');Inc(Ordinal);
end;
'''
instrumented=instrumented.replace('procedure Atari_MixDMASnd',observer+'procedure Atari_MixDMASnd',1)
for method,reg in [('SetEnvelopeRegister',13),('SetMixerRegister',7),('SetAmplA',8),('SetAmplB',9),('SetAmplC',10)]:
 marker='procedure TSoundChip.'+method+'(Value:byte);\nbegin'
 instrumented=instrumented.replace(marker,marker+'\nObserve('+str(reg)+',Value,Ord(@Self=@SoundChip[1]));')
for name,reg in [('TonA',0),('TonB',2),('TonC',4)]:
 marker=f'   SoundChip[CNum].RegisterAY.{name} := PlParams[CNum].PT3_'+chr(65+reg//2)+'.Ton;'
 instrumented=instrumented.replace(marker,marker+f'\n   Observe({reg},SoundChip[CNum].RegisterAY.{name},CNum);Observe({reg+1},SoundChip[CNum].RegisterAY.{name} shr 8,CNum);')
for field,reg in [('Noise',6),('Envelope',11)]:
 pattern=r'(   SoundChip\[CNum\]\.RegisterAY\.'+field+r' := [^;]+;)'
 append=f'\n   Observe({reg},SoundChip[CNum].RegisterAY.{field},CNum);'
 if field=='Envelope':append+='Observe(12,SoundChip[CNum].RegisterAY.Envelope shr 8,CNum);'
 instrumented=re.sub(pattern,lambda match:match[0]+append,instrumented)
instrumented=instrumented.replace('if ParamCount<>2 then Halt(2);','if ParamCount<>3 then Halt(2);')
instrumented=instrumented.replace('for i:=0 to Tm-1 do begin','AssignFile(trace,ParamStr(3));Rewrite(trace);TraceEnabled:=True;\n for i:=0 to Tm-1 do begin SourceTick:=i;Ordinal:=0;')
instrumented=instrumented.replace('AssignFile(output,ParamStr(2));','CloseFile(trace);\n AssignFile(output,ParamStr(2));')
(folder/'oracle-observed.pas').write_text(instrumented)
# Register-log oracle retains the original PSG_Get_Registers and YM3 reader.
log_support="""var
 UniReadersData:array[0..0]of ^TReader;
 Reader:TReader;
 FileHandle:integer=0;PSG_Skip:integer=0;
 Do_Loop:Boolean=False;Real_End_All:Boolean=False;
 PVTXYMUnpackedData:PByte;Position_In_VTX,VTX_Offset,NumberOfVBLs:integer;
procedure UniRead(handle:integer;target:pointer;count:integer);
begin
 if Reader.UniFilePos+count>Reader.UniFileSize then raise Exception.Create('Read bounds');
 Move(PLConsts[0].RAM^.Index[Reader.UniFilePos],target^,count);Inc(Reader.UniFilePos,count);
end;
procedure InitForAllTypes(reset:Boolean);begin Reader.UniFilePos:=16;PSG_Skip:=0;end;
procedure ResetAYChipEmulation(chip:integer;zeroregs:Boolean);begin raise Exception.Create('Unexpected restart');end;
"""
log=instrumented.replace('procedure Atari_MixDMASnd','type TReader=record UniFileSize,UniFilePos:integer;end;\n'+log_support+'procedure Atari_MixDMASnd',1)
log_routines=between(p,'procedure PSG_Get_Registers(CNum: integer);\nvar','procedure STC_Get_Registers(CNum: integer);\n') if 'procedure STC_Get_Registers(CNum: integer);\n' in p[p.index('procedure PSG_Get_Registers(CNum: integer);\nvar'):] else ''
# Bound exact routine by the next top-level procedure rather than copying unrelated players.
start=p.index('procedure PSG_Get_Registers(CNum: integer);\nvar');end=p.index('\nprocedure ',start+10)
log_routines=p[start:end]
log_routines+=between(p,'procedure VTX_YM3_YM3b_Get_Registers(CNum: integer);\nvar','procedure MakeBufferYM2(Buf: pointer);\nvar')
log=log.replace('var module:ModTypes;',log_routines+'\nvar module:ModTypes;',1)
old="Tm:=0;Lp:=0;GetTimePT3(@module,Tm,Lp);PLConsts[0].Global_Tick_Max:=Tm;\n Init;"
new="""Reader.UniFileSize:=size;Reader.UniFilePos:=16;UniReadersData[0]:=@Reader;
 Tm:=0;Lp:=0;
 if module.Index[0]=Ord('P') then begin
  i:=16;while i<size do begin case module.Index[i] of
   255:Inc(Tm);254:begin Inc(i);Inc(Tm,module.Index[i]*4);end;253:break;
   else Inc(i);end;Inc(i);end;
  if not (module.Index[i-1] in [254,255]) then Inc(Tm);
 end else begin if module.Index[3]=Ord('b') then NumberOfVBLs:=(size-8)div 14 else NumberOfVBLs:=(size-4)div 14;Tm:=NumberOfVBLs;VTX_Offset:=4;PVTXYMUnpackedData:=@module.Index;end;
 PLConsts[0].Global_Tick_Max:=Tm;
 """
log=log.replace(old,new)
log=log.replace('PT3_Get_Registers(0);if TSMode then PT3_Get_Registers(1);Number_Of_Tiks.hi',"if module.Index[0]=Ord('P') then PSG_Get_Registers(0) else VTX_YM3_YM3b_Get_Registers(0);Number_Of_Tiks.hi")
log=log.replace("if not (module.Index[i-1] in [254,255]) then Inc(Tm);","if (i>=size) then begin if not (module.Index[size-1] in [254,255]) then Inc(Tm);end else Inc(Tm);")
# Original YM reader assigns some packed registers directly: observe those writes.
# Emit YM writes in original loop order at its source location.
log=log.replace('   Inc(k, NumberOfVBLs);','   if not (i in [7,8,9,10]) then Observe(i,SoundChip[CNum].RegisterAY.Index[i]);\n   Inc(k, NumberOfVBLs);')
# PSG direct assignments must be observed; setters already emit the other registers.
log=log.replace('       end;\n      end;\n    end;\n until UniReadersData', '       end;\n       if (b<14) and not (b in [7,8,9,10,13]) then Observe(b,SoundChip[0].RegisterAY.Index[b]);\n      end;\n    end;\n until UniReadersData')
(folder/'oracle-logs.pas').write_text(log)

import difflib
(folder/'observer.patch').write_text(''.join(difflib.unified_diff(head.splitlines(True),instrumented.splitlines(True),fromfile='oracle.pas',tofile='oracle-observed.pas')))
