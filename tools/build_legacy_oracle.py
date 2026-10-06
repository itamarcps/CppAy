#!/usr/bin/env python3
"""Assemble PT2/STC diagnostics using unchanged AY_Emul routines and shared oracle synth."""
import argparse, pathlib, re, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,default=ROOT/'build-legacy/oracle-legacy.pas');p.add_argument('--compiler',default='fpc');a=p.parse_args()
s=(ROOT/'reference/oracle/oracle-observed.pas').read_text();original=(ROOT/'docs/Ay_Emul/Players.pas').read_text()
def between(start,end):return original[original.index(start):original.index(end,original.index(start))]
def routine(name):
 start=original.rindex('procedure '+name+'(')
 end=original.find('\nprocedure ',start+10)
 return original[start:end]
s=s.replace('type\n ModTypes', 'type\n'+between(' PPT2_Channel_Parameters =',' PSTP_Channel_Parameters =')+'\n ModTypes',1)
s=s.replace('case Boolean of','case integer of',1).replace('False:(Index:','0:(Index:',1).replace('True:(PT3_MusicName:','1:(PT3_MusicName:',1)
s=s.replace('PT3_PositionList:array[0..65535-201]of byte);','''PT3_PositionList:array[0..65535-201]of byte);
 2:(PT2_Delay,PT2_NumberOfPositions,PT2_LoopPosition:byte;PT2_SamplesPointers:array[0..31]of word;PT2_OrnamentsPointers:array[0..15]of word;PT2_PatternsPointer:word;PT2_MusicName:array[0..29]of char;PT2_PositionList:array[0..65535-131]of byte);
 3:(ST_Delay:byte;ST_PositionsPointer,ST_OrnamentsPointer,ST_PatternsPointer:word);''',1)
s=s.replace('PT3_C:PT3_Channel_Parameters;end;','PT3_C:PT3_Channel_Parameters;PT2:PT2_Parameters;PT2_A,PT2_B,PT2_C:PT2_Channel_Parameters;STC:STC_Parameters;STC_A,STC_B,STC_C:STC_Channel_Parameters;end;',1)
s=s.replace('procedure Init;var n,i,b:integer;', 'const\n'+between(' ST_Table: array[0..95] of word =',' {SQ-Tracker}')+'\n'+routine('PT2_Get_Registers')+'\n'+routine('STC_Get_Registers')+'\n'+routine('GetTimePT2')+'\n'+routine('GetTimeSTC')+'\nprocedure Init;var n,i,b:integer;',1)
# Observe direct register assignments at the same sites as the established PT3 observer.
for chan in ('A','B','C'):
 s=s.replace('SoundChip[CNum].RegisterAY.Ton'+chan+' := PlParams[CNum].PT2_'+chan+'.Ton;', 'SoundChip[CNum].RegisterAY.Ton'+chan+' := PlParams[CNum].PT2_'+chan+'.Ton; Observe('+str(2*('ABC'.index(chan)))+',PlParams[CNum].PT2_'+chan+'.Ton); Observe('+str(2*('ABC'.index(chan))+1)+',PlParams[CNum].PT2_'+chan+'.Ton shr 8);')
 s=s.replace('SoundChip[CNum].RegisterAY.Ton'+chan+' := PlParams[CNum].STC_'+chan+'.Ton;', 'SoundChip[CNum].RegisterAY.Ton'+chan+' := PlParams[CNum].STC_'+chan+'.Ton; Observe('+str(2*('ABC'.index(chan)))+',PlParams[CNum].STC_'+chan+'.Ton); Observe('+str(2*('ABC'.index(chan))+1)+',PlParams[CNum].STC_'+chan+'.Ton shr 8);')
for name in ('PT2','STC'):
 start=s.index('procedure '+name+'_Get_Registers(',s.index('const\n ST_Table'));end=s.index('\nprocedure ',start+10);body=s[start:end]
 body=body.replace('   Inc(PlConsts[CNum].Global_Tick_Counter);','   Observe(6,SoundChip[CNum].RegisterAY.Noise);Observe(11,SoundChip[CNum].RegisterAY.Index[11]);Observe(12,SoundChip[CNum].RegisterAY.Index[12]);\n   Inc(PlConsts[CNum].Global_Tick_Counter);') if name=='PT2' else body.replace(' Inc(PlConsts[CNum].Global_Tick_Counter);',' Observe(6,SoundChip[CNum].RegisterAY.Noise);Observe(11,SoundChip[CNum].RegisterAY.Index[11]);Observe(12,SoundChip[CNum].RegisterAY.Index[12]);\n Inc(PlConsts[CNum].Global_Tick_Counter);')
 s=s[:start]+body+s[end:]
start=original.rfind('   else if ',0,original.index('     with PlParams[n].STC, PLConsts[n].RAM^ do'));init_stc=original[start:original.index('   else if FType = FT.FLS then',start)];init_stc=init_stc[init_stc.index('    begin'):].rstrip()
start=original.rfind('   else if ',0,original.index('     with PlParams[n].PT2, PLConsts[n].RAM^ do'));init_pt2=original[start:original.index('   else if FType = FT.PT3 then',start)];init_pt2=init_pt2[init_pt2.index('    begin'):].rstrip()
s=s.replace('var module:ModTypes;', 'procedure InitLegacy(stc:Boolean);var n,i:integer;begin n:=0;if stc then '+init_stc+' else '+init_pt2+';end;\nvar module:ModTypes;')
s=s.replace('Tm:=0;Lp:=0;GetTimePT3(@module,Tm,Lp);', "Tm:=0;Lp:=0;if LowerCase(ExtractFileExt(ParamStr(1)))='.stc' then GetTimeSTC(@module,Tm) else GetTimePT2(@module,Tm,Lp);")
s=s.replace(' Init;SoundChip[0]', " InitLegacy(LowerCase(ExtractFileExt(ParamStr(1)))='.stc');SoundChip[0]")
s=s.replace('PT3_Get_Registers(0);if TSMode then PT3_Get_Registers(1);',"if LowerCase(ExtractFileExt(ParamStr(1)))='.stc' then STC_Get_Registers(0) else PT2_Get_Registers(0);")
# Legacy modules do not contain the PT3 TurboSound/version header.
start=s.index(' PLConsts[0].RAM:=@module;PLConsts[0].Version:=6;');end=s.index('\n Tm:=',start);s=s[:start]+' PLConsts[0].RAM:=@module;'+s[end:]
# Size the capture for the actual integer IRQ/PCM clocks; nominal seconds
# underallocate long songs. This changes buffer capacity, not synthesis.
s=s.replace('BufferLength:=round(Tm*SampleRate/InterruptHz)+1000;', 'BufferLength:=round(Extended(Tm)*round(AY_Freq/InterruptHz/8)*65536/round(AY_Freq/SampleRate/8*65536))+1000;')
s=s.replace('TraceEnabled:=True;', "TraceEnabled:=GetEnvironmentVariable('ORACLE_TRACE_OFF')<>'1';")
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(s)
subprocess.run([a.compiler,'-Fu'+str(ROOT/'reference/toolchain/usr/lib/fpc/3.2.2/units/x86_64-linux/rtl'),str(a.output)],check=True)
