<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="9.6.2">
<drawing>
<settings><setting alwaysvectorfont="no"/><setting verticaltext="up"/></settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="yes" active="yes"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="yes" active="yes"/>
<layer number="17" name="Pads" color="2" fill="1" visible="yes" active="yes"/>
<layer number="18" name="Vias" color="2" fill="1" visible="yes" active="yes"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="yes" active="yes"/>
<layer number="20" name="Dimension" color="15" fill="1" visible="yes" active="yes"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="yes" active="yes"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="yes" active="yes"/>
<layer number="25" name="tNames" color="7" fill="1" visible="yes" active="yes"/>
<layer number="27" name="tValues" color="7" fill="1" visible="yes" active="yes"/>
<layer number="29" name="tStop" color="7" fill="1" visible="yes" active="yes"/>
<layer number="31" name="tCream" color="7" fill="1" visible="yes" active="yes"/>
<layer number="39" name="tKeepout" color="4" fill="1" visible="yes" active="yes"/>
<layer number="41" name="tRestrict" color="4" fill="1" visible="yes" active="yes"/>
<layer number="43" name="vRestrict" color="2" fill="1" visible="yes" active="yes"/>
<layer number="44" name="Drills" color="7" fill="1" visible="yes" active="yes"/>
<layer number="45" name="Holes" color="7" fill="1" visible="yes" active="yes"/>
<layer number="46" name="Milling" color="3" fill="1" visible="yes" active="yes"/>
<layer number="47" name="Measures" color="7" fill="1" visible="yes" active="yes"/>
<layer number="48" name="Document" color="7" fill="1" visible="yes" active="yes"/>
<layer number="49" name="Reference" color="7" fill="1" visible="yes" active="yes"/>
<layer number="51" name="tDocu" color="7" fill="1" visible="yes" active="yes"/>
<layer number="52" name="bDocu" color="7" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="yes" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="nibbles_power">
<description>Nibbles pole-base boards: symbol-only parts (the TI chips are Ultra Librarian parts with footprints; the rest are still symbol-only)</description>
<packages>
<package name="VSSOP10_DGS_TEX">
<smd name="1" x="-2.2161" y="1" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="2" x="-2.2161" y="0.5" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="3" x="-2.2161" y="0" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="4" x="-2.2161" y="-0.5" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="5" x="-2.2161" y="-1" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="6" x="2.2161" y="-1" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="7" x="2.2161" y="-0.5" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="8" x="2.2161" y="0" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="9" x="2.2161" y="0.5" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<smd name="10" x="2.2161" y="1" dx="1.3335" dy="0.2794" layer="1" roundness="0" rot="R0"/>
<wire x1="-1.5494" y1="0.8636" x2="-1.5494" y2="1.143" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="1.143" x2="-2.54" y2="1.143" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="1.143" x2="-2.54" y2="0.8636" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="0.8636" x2="-1.5494" y2="0.8636" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="0.3556" x2="-1.5494" y2="0.635" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="0.635" x2="-2.54" y2="0.635" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="0.635" x2="-2.54" y2="0.3556" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="0.3556" x2="-1.5494" y2="0.3556" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-0.1524" x2="-1.5494" y2="0.1524" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="0.1524" x2="-2.54" y2="0.127" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="0.127" x2="-2.54" y2="-0.1524" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="-0.1524" x2="-1.5494" y2="-0.1524" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-0.635" x2="-1.5494" y2="-0.3556" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-0.3556" x2="-2.54" y2="-0.3556" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="-0.3556" x2="-2.54" y2="-0.635" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="-0.635" x2="-1.5494" y2="-0.635" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-1.143" x2="-1.5494" y2="-0.8636" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-0.8636" x2="-2.54" y2="-0.8636" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="-0.8636" x2="-2.54" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-2.54" y1="-1.143" x2="-1.5494" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-0.8636" x2="1.5494" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-1.143" x2="2.54" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="2.54" y1="-1.143" x2="2.54" y2="-0.8636" width="0.1524" layer="51"/>
<wire x1="2.54" y1="-0.8636" x2="1.5494" y2="-0.8636" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-0.3556" x2="1.5494" y2="-0.635" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-0.635" x2="2.54" y2="-0.635" width="0.1524" layer="51"/>
<wire x1="2.54" y1="-0.635" x2="2.54" y2="-0.3556" width="0.1524" layer="51"/>
<wire x1="2.54" y1="-0.3556" x2="1.5494" y2="-0.3556" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="0.1524" x2="1.5494" y2="-0.1524" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-0.1524" x2="2.54" y2="-0.127" width="0.1524" layer="51"/>
<wire x1="2.54" y1="-0.127" x2="2.54" y2="0.1524" width="0.1524" layer="51"/>
<wire x1="2.54" y1="0.1524" x2="1.5494" y2="0.1524" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="0.635" x2="1.5494" y2="0.3556" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="0.3556" x2="2.54" y2="0.3556" width="0.1524" layer="51"/>
<wire x1="2.54" y1="0.3556" x2="2.54" y2="0.635" width="0.1524" layer="51"/>
<wire x1="2.54" y1="0.635" x2="1.5494" y2="0.635" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="1.143" x2="1.5494" y2="0.8636" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="0.8636" x2="2.54" y2="0.8636" width="0.1524" layer="51"/>
<wire x1="2.54" y1="0.8636" x2="2.54" y2="1.143" width="0.1524" layer="51"/>
<wire x1="2.54" y1="1.143" x2="1.5494" y2="1.143" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="-1.5494" x2="1.5494" y2="-1.5494" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="-1.5494" x2="1.5494" y2="1.5494" width="0.1524" layer="51"/>
<wire x1="1.5494" y1="1.5494" x2="-1.5494" y2="1.5494" width="0.1524" layer="51"/>
<wire x1="-1.5494" y1="1.5494" x2="-1.5494" y2="-1.5494" width="0.1524" layer="51"/>
<wire x1="0.3048" y1="1.5494" x2="-0.3048" y2="1.5494" width="0.1524" layer="51" curve="-180"/>
<wire x1="-1.6764" y1="-1.6764" x2="1.6764" y2="-1.6764" width="0.1524" layer="21"/>
<wire x1="1.6764" y1="-1.6764" x2="1.6764" y2="-1.4732" width="0.1524" layer="21"/>
<wire x1="1.6764" y1="1.6764" x2="-1.6764" y2="1.6764" width="0.1524" layer="21"/>
<wire x1="-1.6764" y1="1.6764" x2="-1.6764" y2="1.4732" width="0.1524" layer="21"/>
<wire x1="-1.6764" y1="-1.4732" x2="-1.6764" y2="-1.6764" width="0.1524" layer="21"/>
<wire x1="1.6764" y1="1.4732" x2="1.6764" y2="1.6764" width="0.1524" layer="21"/>
<polygon width="0.0254" layer="21"><vertex x="3.3909" y="1.1905"/><vertex x="3.3909" y="0.8095"/><vertex x="3.1369" y="0.8095"/><vertex x="3.1369" y="1.1905"/></polygon>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
</package>
<package name="DBV0006A_N">
<smd name="1" x="-1.375" y="0.95" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<smd name="2" x="-1.375" y="0" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<smd name="3" x="-1.375" y="-0.95" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<smd name="4" x="1.375" y="-0.95" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<smd name="5" x="1.375" y="0" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<smd name="6" x="1.375" y="0.95" dx="0.6" dy="1" layer="1" roundness="0" rot="R90"/>
<wire x1="0.7874" y1="-1.397" x2="0.7874" y2="1.3716" width="0.1524" layer="51"/>
<wire x1="-0.7366" y1="1.4478" x2="-0.7366" y2="1.4478" width="0.1524" layer="51"/>
<wire x1="-0.8128" y1="-1.397" x2="-0.8128" y2="1.3716" width="0.1524" layer="51"/>
<wire x1="-0.7366" y1="1.4478" x2="0.7112" y2="1.4478" width="0.1524" layer="51"/>
<wire x1="0.7112" y1="-1.4732" x2="0.7112" y2="-1.4732" width="0.1524" layer="51"/>
<wire x1="0.7112" y1="1.4478" x2="0.7112" y2="1.4478" width="0.1524" layer="51"/>
<wire x1="-0.7366" y1="-1.4732" x2="-0.7366" y2="-1.4732" width="0.1524" layer="51"/>
<wire x1="-0.7366" y1="-1.4732" x2="0.7112" y2="-1.4732" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="-0.7874" x2="-0.8382" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="-0.7874" x2="-1.1176" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="-1.143" x2="-0.8382" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="-1.143" x2="-1.1938" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="-0.7874" x2="-1.1938" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="-0.7874" x2="-0.8128" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="-0.7874" x2="-1.0922" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-0.8128" y1="-1.143" x2="-0.8128" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="-1.143" x2="-1.4224" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="-1.143" x2="-0.8128" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="-1.143" x2="-1.0922" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="-1.143" x2="-1.1176" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="0.1778" x2="-0.8382" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="0.1778" x2="-1.1176" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="-0.2032" x2="-0.8382" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="-0.2032" x2="-1.1938" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="0.1778" x2="-1.1938" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="0.1778" x2="-0.8128" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="0.1778" x2="-1.0922" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-0.8128" y1="-0.2032" x2="-0.8128" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="-0.2032" x2="-1.4224" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="-0.2032" x2="-0.8128" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="-0.2032" x2="-1.0922" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="-0.2032" x2="-1.1176" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="1.1176" x2="-0.8382" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="1.1176" x2="-1.1176" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-1.0922" y1="0.762" x2="-0.8382" y2="0.762" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="0.762" x2="-1.1938" y2="0.762" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="1.1176" x2="-1.1938" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="1.1176" x2="-0.8128" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="1.1176" x2="-1.0922" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-0.8128" y1="0.762" x2="-0.8128" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-1.4224" y1="0.762" x2="-1.4224" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-0.8382" y1="0.762" x2="-0.8128" y2="0.762" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="0.762" x2="-1.0922" y2="0.762" width="0.1524" layer="51"/>
<wire x1="-1.1938" y1="0.762" x2="-1.1176" y2="0.762" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="-1.143" x2="1.0922" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="-1.143" x2="1.1938" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="-0.7874" x2="1.0922" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="-0.7874" x2="1.4224" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="-1.143" x2="1.4224" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="-1.143" x2="0.8382" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="-1.143" x2="1.1176" y2="-1.143" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="-1.143" x2="0.7874" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="1.4224" y1="-1.143" x2="1.4224" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="-0.7874" x2="0.8382" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="-0.7874" x2="1.1176" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="-0.7874" x2="1.1938" y2="-0.7874" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="-0.2032" x2="1.0922" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="-0.2032" x2="1.1938" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="0.1778" x2="1.0922" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="0.1778" x2="1.4224" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="-0.2032" x2="1.4224" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="-0.2032" x2="0.8382" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="-0.2032" x2="1.1176" y2="-0.2032" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="-0.2032" x2="0.7874" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="1.4224" y1="-0.2032" x2="1.4224" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="0.1778" x2="0.8382" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="0.1778" x2="1.1176" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="0.1778" x2="1.1938" y2="0.1778" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="0.762" x2="1.0922" y2="0.762" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="0.762" x2="1.1938" y2="0.762" width="0.1524" layer="51"/>
<wire x1="0.8382" y1="1.1176" x2="1.0922" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="1.1176" x2="1.4224" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="1.1938" y1="0.762" x2="1.4224" y2="0.762" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="0.762" x2="0.8382" y2="0.762" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="0.762" x2="1.1176" y2="0.762" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="0.762" x2="0.7874" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="1.4224" y1="0.762" x2="1.4224" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="0.7874" y1="1.1176" x2="0.8382" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="1.0922" y1="1.1176" x2="1.1176" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="1.1176" x2="1.1938" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-0.8128" y1="1.3716" x2="-0.7366" y2="1.4478" width="0.1524" layer="51" curve="-90"/>
<wire x1="0.7112" y1="1.4478" x2="0.7874" y2="1.3716" width="0.1524" layer="51" curve="-90"/>
<wire x1="0.7874" y1="-1.397" x2="0.7112" y2="-1.4732" width="0.1524" layer="51" curve="-90"/>
<wire x1="-0.5842" y1="0.9144" x2="-0.3302" y2="0.9144" width="0.1524" layer="51" curve="-180"/>
<wire x1="-0.3302" y1="0.9144" x2="-0.5842" y2="0.9144" width="0.1524" layer="51" curve="-180"/>
<wire x1="-0.7366" y1="-1.4732" x2="-0.8128" y2="-1.397" width="0.1524" layer="51" curve="-90"/>
<wire x1="-1.6764" y1="1.5494" x2="0.508" y2="1.5494" width="0.2032" layer="21"/>
<wire x1="-0.4826" y1="-1.5494" x2="0.508" y2="-1.5494" width="0.2032" layer="21"/>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
<text x="-3.2766" y="-0.635" size="1.27" layer="27" ratio="6">&gt;NAME</text>
</package>
<package name="PW0016A-MFG">
<smd name="1" x="-2.9" y="2.275" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="2" x="-2.9" y="1.625" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="3" x="-2.9" y="0.975" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="4" x="-2.9" y="0.325" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="5" x="-2.9" y="-0.325" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="6" x="-2.9" y="-0.975" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="7" x="-2.9" y="-1.625" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="8" x="-2.9" y="-2.275" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="9" x="2.9" y="-2.275" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="10" x="2.9" y="-1.625" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="11" x="2.9" y="-0.975" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="12" x="2.9" y="-0.325" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="13" x="2.9" y="0.325" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="14" x="2.9" y="0.975" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="15" x="2.9" y="1.625" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<smd name="16" x="2.9" y="2.275" dx="1.5" dy="0.45" layer="1" roundness="0" rot="R0"/>
<wire x1="-2.2606" y1="-2.54" x2="-2.2606" y2="2.54" width="0.1524" layer="51"/>
<wire x1="-2.2606" y1="-2.54" x2="2.2606" y2="-2.54" width="0.1524" layer="51"/>
<wire x1="2.2606" y1="-2.54" x2="2.2606" y2="2.54" width="0.1524" layer="51"/>
<wire x1="-2.2606" y1="2.54" x2="2.2606" y2="2.54" width="0.1524" layer="51"/>
<wire x1="-1.778" y1="-2.54" x2="1.778" y2="-2.54" width="0.1524" layer="21"/>
<wire x1="-1.778" y1="2.54" x2="1.778" y2="2.54" width="0.1524" layer="21"/>
<wire x1="-4.1148" y1="2.286" x2="-4.3688" y2="2.286" width="0.254" layer="21" curve="-180"/>
<wire x1="-4.3688" y1="2.286" x2="-4.1148" y2="2.286" width="0.254" layer="21" curve="-180"/>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
<text x="-3.2766" y="-0.635" size="1.27" layer="27" ratio="6">&gt;NAME</text>
</package>
<package name="RGE24_2P5XP85X1P45_TEX">
<smd name="1" x="-1.9558" y="1.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="2" x="-1.9558" y="0.75" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="3" x="-1.9558" y="0.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="4" x="-1.9558" y="-0.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="5" x="-1.9558" y="-0.75" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="6" x="-1.9558" y="-1.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="7" x="-1.25" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="8" x="-0.75" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="9" x="-0.25" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="10" x="0.25" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="11" x="0.75" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="12" x="1.25" y="-1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="13" x="1.9558" y="-1.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="14" x="1.9558" y="-0.75" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="15" x="1.9558" y="-0.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="16" x="1.9558" y="0.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="17" x="1.9558" y="0.75" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="18" x="1.9558" y="1.25" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R270"/>
<smd name="19" x="1.25" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="20" x="0.75" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="21" x="0.25" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="22" x="-0.25" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="23" x="-0.75" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="24" x="-1.25" y="1.9558" dx="0.2286" dy="0.7874" layer="1" roundness="0" rot="R180"/>
<smd name="25" x="0" y="0.625" dx="2.794" dy="1.5494" layer="1" roundness="0" rot="R0"/>
<smd name="26" x="0" y="-0.925" dx="2.794" dy="0.9398" layer="1" roundness="0" rot="R0"/>
<wire x1="-2.1844" y1="-2.1844" x2="-1.7018" y2="-2.1844" width="0.1524" layer="21"/>
<wire x1="2.1844" y1="-2.1844" x2="2.1844" y2="-1.7018" width="0.1524" layer="21"/>
<wire x1="2.1844" y1="2.1844" x2="1.7018" y2="2.1844" width="0.1524" layer="21"/>
<wire x1="-2.1844" y1="2.1844" x2="-2.1844" y2="1.7018" width="0.1524" layer="21"/>
<wire x1="-2.1844" y1="-1.7018" x2="-2.1844" y2="-2.1844" width="0.1524" layer="21"/>
<wire x1="1.7018" y1="-2.1844" x2="2.1844" y2="-2.1844" width="0.1524" layer="21"/>
<wire x1="2.1844" y1="1.7018" x2="2.1844" y2="2.1844" width="0.1524" layer="21"/>
<wire x1="-1.7018" y1="2.1844" x2="-2.1844" y2="2.1844" width="0.1524" layer="21"/>
<polygon width="0.0254" layer="21"><vertex x="0.0595" y="-2.6035"/><vertex x="0.0595" y="-2.8575"/><vertex x="0.4405" y="-2.8575"/><vertex x="0.4405" y="-2.6035"/></polygon>
<polygon width="0.0254" layer="21"><vertex x="0.5595" y="2.6035"/><vertex x="0.5595" y="2.8575"/><vertex x="0.9405" y="2.8575"/><vertex x="0.9405" y="2.6035"/></polygon>
<wire x1="-2.032" y1="0.762" x2="-0.762" y2="2.032" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="2.032" x2="1.397" y2="2.032" width="0.1524" layer="51"/>
<wire x1="1.397" y1="2.032" x2="1.397" y2="2.032" width="0.1524" layer="51"/>
<wire x1="1.397" y1="2.032" x2="1.1176" y2="2.032" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="2.032" x2="1.1176" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.6096" y1="2.032" x2="0.889" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.889" y1="2.032" x2="0.889" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.889" y1="2.032" x2="0.6096" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.6096" y1="2.032" x2="0.6096" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.1016" y1="2.032" x2="0.381" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.381" y1="2.032" x2="0.381" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.381" y1="2.032" x2="0.1016" y2="2.032" width="0.1524" layer="51"/>
<wire x1="0.1016" y1="2.032" x2="0.1016" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.381" y1="2.032" x2="-0.1016" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.1016" y1="2.032" x2="-0.1016" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.1016" y1="2.032" x2="-0.381" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.381" y1="2.032" x2="-0.381" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.889" y1="2.032" x2="-0.6096" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.6096" y1="2.032" x2="-0.6096" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.6096" y1="2.032" x2="-0.889" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-0.889" y1="2.032" x2="-0.889" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-1.397" y1="2.032" x2="-1.1176" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="2.032" x2="-1.1176" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="2.032" x2="-1.397" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-1.397" y1="2.032" x2="-1.397" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="1.1176" x2="-2.032" y2="1.397" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="1.397" x2="-2.032" y2="1.397" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="1.397" x2="-2.032" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="1.1176" x2="-2.032" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.6096" x2="-2.032" y2="0.889" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.889" x2="-2.032" y2="0.889" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.889" x2="-2.032" y2="0.6096" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.6096" x2="-2.032" y2="0.6096" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.1016" x2="-2.032" y2="0.381" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.381" x2="-2.032" y2="0.381" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.381" x2="-2.032" y2="0.1016" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="0.1016" x2="-2.032" y2="0.1016" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.381" x2="-2.032" y2="-0.1016" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.1016" x2="-2.032" y2="-0.1016" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.1016" x2="-2.032" y2="-0.381" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.381" x2="-2.032" y2="-0.381" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.889" x2="-2.032" y2="-0.6096" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.6096" x2="-2.032" y2="-0.6096" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.6096" x2="-2.032" y2="-0.889" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-0.889" x2="-2.032" y2="-0.889" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-1.397" x2="-2.032" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-1.1176" x2="-2.032" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-1.1176" x2="-2.032" y2="-1.397" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-1.397" x2="-2.032" y2="-1.397" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="-2.032" x2="-1.397" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-1.397" y1="-2.032" x2="-1.397" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-1.397" y1="-2.032" x2="-1.1176" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-1.1176" y1="-2.032" x2="-1.1176" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.6096" y1="-2.032" x2="-0.889" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.889" y1="-2.032" x2="-0.889" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.889" y1="-2.032" x2="-0.6096" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.6096" y1="-2.032" x2="-0.6096" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.1016" y1="-2.032" x2="-0.381" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.381" y1="-2.032" x2="-0.381" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.381" y1="-2.032" x2="-0.1016" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="-0.1016" y1="-2.032" x2="-0.1016" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.381" y1="-2.032" x2="0.1016" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.1016" y1="-2.032" x2="0.1016" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.1016" y1="-2.032" x2="0.381" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.381" y1="-2.032" x2="0.381" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.889" y1="-2.032" x2="0.6096" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.6096" y1="-2.032" x2="0.6096" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.6096" y1="-2.032" x2="0.889" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="0.889" y1="-2.032" x2="0.889" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="1.397" y1="-2.032" x2="1.1176" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="-2.032" x2="1.1176" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="1.1176" y1="-2.032" x2="1.397" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="1.397" y1="-2.032" x2="1.397" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-1.1176" x2="2.032" y2="-1.397" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-1.397" x2="2.032" y2="-1.397" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-1.397" x2="2.032" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-1.1176" x2="2.032" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.6096" x2="2.032" y2="-0.889" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.889" x2="2.032" y2="-0.889" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.889" x2="2.032" y2="-0.6096" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.6096" x2="2.032" y2="-0.6096" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.1016" x2="2.032" y2="-0.381" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.381" x2="2.032" y2="-0.381" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.381" x2="2.032" y2="-0.1016" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-0.1016" x2="2.032" y2="-0.1016" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.381" x2="2.032" y2="0.1016" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.1016" x2="2.032" y2="0.1016" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.1016" x2="2.032" y2="0.381" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.381" x2="2.032" y2="0.381" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.889" x2="2.032" y2="0.6096" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.6096" x2="2.032" y2="0.6096" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.6096" x2="2.032" y2="0.889" width="0.1524" layer="51"/>
<wire x1="2.032" y1="0.889" x2="2.032" y2="0.889" width="0.1524" layer="51"/>
<wire x1="2.032" y1="1.397" x2="2.032" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="2.032" y1="1.1176" x2="2.032" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="2.032" y1="1.1176" x2="2.032" y2="1.397" width="0.1524" layer="51"/>
<wire x1="2.032" y1="1.397" x2="2.032" y2="1.397" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="-2.032" x2="2.032" y2="-2.032" width="0.1524" layer="51"/>
<wire x1="2.032" y1="-2.032" x2="2.032" y2="2.032" width="0.1524" layer="51"/>
<wire x1="2.032" y1="2.032" x2="-2.032" y2="2.032" width="0.1524" layer="51"/>
<wire x1="-2.032" y1="2.032" x2="-2.032" y2="-2.032" width="0.1524" layer="51"/>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
</package>
<package name="SOT_33DRLR_TEX">
<smd name="1" x="-0.8001" y="0.7502" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="2" x="-0.8001" y="0.2501" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="3" x="-0.8001" y="-0.2501" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="4" x="-0.8001" y="-0.7502" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="5" x="0.8001" y="-0.7502" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="6" x="0.8001" y="-0.2501" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="7" x="0.8001" y="0.2501" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<smd name="8" x="0.8001" y="0.7502" dx="0.7112" dy="0.2286" layer="1" roundness="0" rot="R0"/>
<wire x1="-0.762" y1="-1.2192" x2="0.762" y2="-1.2192" width="0.1524" layer="21"/>
<wire x1="0.762" y1="1.2192" x2="-0.762" y2="1.2192" width="0.1524" layer="21"/>
<wire x1="-1.6002" y1="0.762" x2="-1.8034" y2="0.762" width="0.1524" layer="21" curve="-180"/>
<wire x1="-1.8034" y1="0.762" x2="-1.6002" y2="0.762" width="0.1524" layer="21" curve="-180"/>
<wire x1="-0.6604" y1="-1.1176" x2="0.6604" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="0.6604" y1="-1.1176" x2="0.6604" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="0.6604" y1="1.1176" x2="-0.6604" y2="1.1176" width="0.1524" layer="51"/>
<wire x1="-0.6604" y1="1.1176" x2="-0.6604" y2="-1.1176" width="0.1524" layer="51"/>
<wire x1="-0.3556" y1="0.762" x2="-0.508" y2="0.762" width="0" layer="51" curve="-180"/>
<wire x1="-0.508" y1="0.762" x2="-0.3556" y2="0.762" width="0" layer="51" curve="-180"/>
<wire x1="0.3048" y1="1.1176" x2="-0.3048" y2="1.0922" width="0.1524" layer="51" curve="-180"/>
<text x="-3.2766" y="-0.635" size="1.27" layer="27" ratio="6">&gt;NAME</text>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
</package>
<package name="AMASS_XT30PW-M_1x02_P2.50mm_Horizontal">
<pad name="M1" x="-8" y="10" drill="1" diameter="1.4" shape="round" rot="R0"/>
<pad name="M2" x="3" y="10" drill="1" diameter="1.4" shape="round" rot="R0"/>
<pad name="1" x="0" y="-0" drill="1.9" diameter="3.5" shape="square" rot="R0"/>
<pad name="2" x="-5" y="-0" drill="1.9" diameter="3.5" shape="round" rot="R0"/>
<wire x1="-8.15" y1="11.11" x2="-7.56" y2="11.11" width="0.12" layer="21"/>
<wire x1="-8.15" y1="8.89" x2="-7.56" y2="8.89" width="0.12" layer="21"/>
<wire x1="-7.56" y1="13.21" x2="-7.56" y2="11.11" width="0.12" layer="21"/>
<wire x1="-7.56" y1="13.21" x2="2.56" y2="13.21" width="0.12" layer="21"/>
<wire x1="-7.56" y1="8.89" x2="-7.56" y2="3.99" width="0.12" layer="21"/>
<wire x1="-7.56" y1="3.99" x2="2.56" y2="3.99" width="0.12" layer="21"/>
<wire x1="2.56" y1="13.21" x2="2.56" y2="11.11" width="0.12" layer="21"/>
<wire x1="2.56" y1="11.11" x2="3.15" y2="11.11" width="0.12" layer="21"/>
<wire x1="2.56" y1="8.89" x2="2.56" y2="3.99" width="0.12" layer="21"/>
<wire x1="2.56" y1="8.89" x2="3.15" y2="8.89" width="0.12" layer="21"/>
<wire x1="-8.15" y1="11" x2="-7.45" y2="11" width="0.1" layer="51"/>
<wire x1="-8.15" y1="9" x2="-7.45" y2="9" width="0.1" layer="51"/>
<wire x1="-7.45" y1="13.1" x2="-7.45" y2="11" width="0.1" layer="51"/>
<wire x1="-7.45" y1="13.1" x2="2.45" y2="13.1" width="0.1" layer="51"/>
<wire x1="-7.45" y1="9" x2="-7.45" y2="4.1" width="0.1" layer="51"/>
<wire x1="-7.45" y1="4.1" x2="2.45" y2="4.1" width="0.1" layer="51"/>
<wire x1="2.45" y1="13.1" x2="2.45" y2="11" width="0.1" layer="51"/>
<wire x1="2.45" y1="11" x2="3.15" y2="11" width="0.1" layer="51"/>
<wire x1="2.45" y1="9" x2="2.45" y2="4.1" width="0.1" layer="51"/>
<wire x1="2.45" y1="9" x2="3.15" y2="9" width="0.1" layer="51"/>
<wire x1="-8.15" y1="8.89" x2="-8.15" y2="11.11" width="0.12" layer="21" curve="-180"/>
<wire x1="3.15" y1="11.11" x2="3.15" y2="8.89" width="0.12" layer="21" curve="-180"/>
<wire x1="-8.15" y1="9" x2="-8.15" y2="11" width="0.1" layer="51" curve="-180"/>
<wire x1="3.15" y1="11" x2="3.15" y2="9" width="0.1" layer="51" curve="-180"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal">
<pad name="1" x="0" y="-0" drill="0.75" diameter="1.2" shape="square" rot="R0"/>
<pad name="2" x="2" y="-0" drill="0.75" diameter="1.2" shape="long" rot="R90"/>
<wire x1="-2.06" y1="1.46" x2="-2.06" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-2.06" y1="-0.14" x2="-1.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="-2.06" y1="-6.36" x2="4.06" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-1.14" y1="1.46" x2="-2.06" y2="1.46" width="0.12" layer="21"/>
<wire x1="-1.14" y1="-0.14" x2="-1.14" y2="1.46" width="0.12" layer="21"/>
<wire x1="-0.86" y1="-0.14" x2="-1.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="-0.86" y1="-0.14" x2="-0.86" y2="1.075" width="0.12" layer="21"/>
<wire x1="-0.8" y1="-4.1" x2="-0.8" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-4.1" x2="-0.3" y2="-6.36" width="0.12" layer="21"/>
<wire x1="0.5" y1="-2" x2="1.5" y2="-2" width="0.12" layer="21"/>
<wire x1="0.5" y1="-6.36" x2="0.5" y2="-2" width="0.12" layer="21"/>
<wire x1="1.5" y1="-2" x2="1.5" y2="-6.36" width="0.12" layer="21"/>
<wire x1="3.14" y1="1.46" x2="3.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="3.14" y1="-0.14" x2="2.86" y2="-0.14" width="0.12" layer="21"/>
<wire x1="4.06" y1="1.46" x2="3.14" y2="1.46" width="0.12" layer="21"/>
<wire x1="4.06" y1="-0.14" x2="3.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="4.06" y1="-6.36" x2="4.06" y2="1.46" width="0.12" layer="21"/>
<wire x1="-1.95" y1="1.35" x2="-1.95" y2="-6.25" width="0.1" layer="51"/>
<wire x1="-1.95" y1="-6.25" x2="3.95" y2="-6.25" width="0.1" layer="51"/>
<wire x1="-1.25" y1="1.35" x2="-1.95" y2="1.35" width="0.1" layer="51"/>
<wire x1="-1.25" y1="-0.25" x2="-1.25" y2="1.35" width="0.1" layer="51"/>
<wire x1="-0.5" y1="-1.375" x2="0.5" y2="-1.375" width="0.1" layer="51"/>
<wire x1="0" y1="-0.875" x2="-0.5" y2="-1.375" width="0.1" layer="51"/>
<wire x1="0.5" y1="-1.375" x2="0" y2="-0.875" width="0.1" layer="51"/>
<wire x1="3.25" y1="1.35" x2="3.25" y2="-0.25" width="0.1" layer="51"/>
<wire x1="3.25" y1="-0.25" x2="-1.25" y2="-0.25" width="0.1" layer="51"/>
<wire x1="3.95" y1="1.35" x2="3.25" y2="1.35" width="0.1" layer="51"/>
<wire x1="3.95" y1="-6.25" x2="3.95" y2="1.35" width="0.1" layer="51"/>
<wire x1="-1.3" y1="-2.5" x2="-0.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-2.5" x2="-0.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-4.1" x2="-1.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="-1.3" y1="-4.1" x2="-1.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="2.3" y1="-2.5" x2="3.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="3.3" y1="-2.5" x2="3.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="3.3" y1="-4.1" x2="2.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="2.3" y1="-4.1" x2="2.3" y2="-2.5" width="0.12" layer="21"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="JST_PH_S3B-PH-K_1x03_P2.00mm_Horizontal">
<pad name="1" x="0" y="-0" drill="0.75" diameter="1.2" shape="square" rot="R0"/>
<pad name="2" x="2" y="-0" drill="0.75" diameter="1.2" shape="long" rot="R90"/>
<pad name="3" x="4" y="-0" drill="0.75" diameter="1.2" shape="long" rot="R90"/>
<wire x1="-2.06" y1="1.46" x2="-2.06" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-2.06" y1="-0.14" x2="-1.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="-2.06" y1="-6.36" x2="6.06" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-1.14" y1="1.46" x2="-2.06" y2="1.46" width="0.12" layer="21"/>
<wire x1="-1.14" y1="-0.14" x2="-1.14" y2="1.46" width="0.12" layer="21"/>
<wire x1="-0.86" y1="-0.14" x2="-1.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="-0.86" y1="-0.14" x2="-0.86" y2="1.075" width="0.12" layer="21"/>
<wire x1="-0.8" y1="-4.1" x2="-0.8" y2="-6.36" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-4.1" x2="-0.3" y2="-6.36" width="0.12" layer="21"/>
<wire x1="0.5" y1="-2" x2="3.5" y2="-2" width="0.12" layer="21"/>
<wire x1="0.5" y1="-6.36" x2="0.5" y2="-2" width="0.12" layer="21"/>
<wire x1="3.5" y1="-2" x2="3.5" y2="-6.36" width="0.12" layer="21"/>
<wire x1="5.14" y1="1.46" x2="5.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="5.14" y1="-0.14" x2="4.86" y2="-0.14" width="0.12" layer="21"/>
<wire x1="6.06" y1="1.46" x2="5.14" y2="1.46" width="0.12" layer="21"/>
<wire x1="6.06" y1="-0.14" x2="5.14" y2="-0.14" width="0.12" layer="21"/>
<wire x1="6.06" y1="-6.36" x2="6.06" y2="1.46" width="0.12" layer="21"/>
<wire x1="-1.95" y1="1.35" x2="-1.95" y2="-6.25" width="0.1" layer="51"/>
<wire x1="-1.95" y1="-6.25" x2="5.95" y2="-6.25" width="0.1" layer="51"/>
<wire x1="-1.25" y1="1.35" x2="-1.95" y2="1.35" width="0.1" layer="51"/>
<wire x1="-1.25" y1="-0.25" x2="-1.25" y2="1.35" width="0.1" layer="51"/>
<wire x1="-0.5" y1="-1.375" x2="0.5" y2="-1.375" width="0.1" layer="51"/>
<wire x1="0" y1="-0.875" x2="-0.5" y2="-1.375" width="0.1" layer="51"/>
<wire x1="0.5" y1="-1.375" x2="0" y2="-0.875" width="0.1" layer="51"/>
<wire x1="5.25" y1="1.35" x2="5.25" y2="-0.25" width="0.1" layer="51"/>
<wire x1="5.25" y1="-0.25" x2="-1.25" y2="-0.25" width="0.1" layer="51"/>
<wire x1="5.95" y1="1.35" x2="5.25" y2="1.35" width="0.1" layer="51"/>
<wire x1="5.95" y1="-6.25" x2="5.95" y2="1.35" width="0.1" layer="51"/>
<wire x1="-1.3" y1="-2.5" x2="-0.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-2.5" x2="-0.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="-0.3" y1="-4.1" x2="-1.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="-1.3" y1="-4.1" x2="-1.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="4.3" y1="-2.5" x2="5.3" y2="-2.5" width="0.12" layer="21"/>
<wire x1="5.3" y1="-2.5" x2="5.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="5.3" y1="-4.1" x2="4.3" y2="-4.1" width="0.12" layer="21"/>
<wire x1="4.3" y1="-4.1" x2="4.3" y2="-2.5" width="0.12" layer="21"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="D_0402_1005Metric">
<smd name="1" x="-0.485" y="-0" dx="0.59" dy="0.64" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.485" y="-0" dx="0.59" dy="0.64" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.4" y1="-0.25" x2="-0.4" y2="0.25" width="0.1" layer="51"/>
<wire x1="-0.3" y1="-0.25" x2="-0.3" y2="0.25" width="0.1" layer="51"/>
<wire x1="-0.5" y1="0.25" x2="0.5" y2="0.25" width="0.1" layer="51"/>
<wire x1="0.5" y1="0.25" x2="0.5" y2="-0.25" width="0.1" layer="51"/>
<wire x1="0.5" y1="-0.25" x2="-0.5" y2="-0.25" width="0.1" layer="51"/>
<wire x1="-0.5" y1="-0.25" x2="-0.5" y2="0.25" width="0.1" layer="51"/>
<circle x="-1.09" y="-0" radius="0.05" width="0.1" layer="21"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="D_SMB">
<smd name="1" x="-2.15" y="-0" dx="2.5" dy="2.3" layer="1" roundness="22" rot="R0"/>
<smd name="2" x="2.15" y="-0" dx="2.5" dy="2.3" layer="1" roundness="22" rot="R0"/>
<wire x1="-3.66" y1="2.15" x2="-3.66" y2="-2.15" width="0.12" layer="21"/>
<wire x1="-3.66" y1="2.15" x2="2.15" y2="2.15" width="0.12" layer="21"/>
<wire x1="-3.66" y1="-2.15" x2="2.15" y2="-2.15" width="0.12" layer="21"/>
<wire x1="-2.3" y1="-2" x2="-2.3" y2="2" width="0.1" layer="51"/>
<wire x1="-0.6494" y1="0.7991" x2="-0.6494" y2="-0.8011" width="0.1" layer="51"/>
<wire x1="-0.6494" y1="-0.001" x2="-1.5511" y2="-0.001" width="0.1" layer="51"/>
<wire x1="-0.6494" y1="-0.001" x2="0.5012" y2="0.7991" width="0.1" layer="51"/>
<wire x1="-0.6494" y1="-0.001" x2="0.5012" y2="-0.7503" width="0.1" layer="51"/>
<wire x1="0.5012" y1="-0.001" x2="1.4994" y2="-0.001" width="0.1" layer="51"/>
<wire x1="0.5012" y1="-0.7503" x2="0.5012" y2="0.7991" width="0.1" layer="51"/>
<wire x1="2.3" y1="2" x2="-2.3" y2="2" width="0.1" layer="51"/>
<wire x1="2.3" y1="2" x2="2.3" y2="-2" width="0.1" layer="51"/>
<wire x1="2.3" y1="-2" x2="-2.3" y2="-2" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="R_0603_1608Metric">
<smd name="1" x="-0.825" y="-0" dx="0.8" dy="0.95" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.825" y="-0" dx="0.8" dy="0.95" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.2373" y1="0.5225" x2="0.2373" y2="0.5225" width="0.12" layer="21"/>
<wire x1="-0.2373" y1="-0.5225" x2="0.2373" y2="-0.5225" width="0.12" layer="21"/>
<wire x1="-0.8" y1="0.4125" x2="0.8" y2="0.4125" width="0.1" layer="51"/>
<wire x1="0.8" y1="0.4125" x2="0.8" y2="-0.4125" width="0.1" layer="51"/>
<wire x1="0.8" y1="-0.4125" x2="-0.8" y2="-0.4125" width="0.1" layer="51"/>
<wire x1="-0.8" y1="-0.4125" x2="-0.8" y2="0.4125" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="R_2512_6332Metric">
<smd name="1" x="-2.9625" y="-0" dx="1.225" dy="3.35" layer="1" roundness="41" rot="R0"/>
<smd name="2" x="2.9625" y="-0" dx="1.225" dy="3.35" layer="1" roundness="41" rot="R0"/>
<wire x1="-2.1771" y1="1.71" x2="2.1771" y2="1.71" width="0.12" layer="21"/>
<wire x1="-2.1771" y1="-1.71" x2="2.1771" y2="-1.71" width="0.12" layer="21"/>
<wire x1="-3.15" y1="1.6" x2="3.15" y2="1.6" width="0.1" layer="51"/>
<wire x1="3.15" y1="1.6" x2="3.15" y2="-1.6" width="0.1" layer="51"/>
<wire x1="3.15" y1="-1.6" x2="-3.15" y2="-1.6" width="0.1" layer="51"/>
<wire x1="-3.15" y1="-1.6" x2="-3.15" y2="1.6" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="CP_Elec_6.3x9.9">
<smd name="1" x="-2.8" y="-0" dx="3.5" dy="1.6" layer="1" roundness="31" rot="R0"/>
<smd name="2" x="2.8" y="-0" dx="3.5" dy="1.6" layer="1" roundness="31" rot="R0"/>
<wire x1="-4.4375" y1="1.8475" x2="-3.65" y2="1.8475" width="0.12" layer="21"/>
<wire x1="-4.0438" y1="2.2412" x2="-4.0438" y2="1.4538" width="0.12" layer="21"/>
<wire x1="-3.41" y1="2.3456" x2="-3.41" y2="1.06" width="0.12" layer="21"/>
<wire x1="-3.41" y1="2.3456" x2="-2.3456" y2="3.41" width="0.12" layer="21"/>
<wire x1="-3.41" y1="-2.3456" x2="-3.41" y2="-1.06" width="0.12" layer="21"/>
<wire x1="-3.41" y1="-2.3456" x2="-2.3456" y2="-3.41" width="0.12" layer="21"/>
<wire x1="-2.3456" y1="3.41" x2="3.41" y2="3.41" width="0.12" layer="21"/>
<wire x1="-2.3456" y1="-3.41" x2="3.41" y2="-3.41" width="0.12" layer="21"/>
<wire x1="3.41" y1="3.41" x2="3.41" y2="1.06" width="0.12" layer="21"/>
<wire x1="3.41" y1="-3.41" x2="3.41" y2="-1.06" width="0.12" layer="21"/>
<wire x1="-3.3" y1="2.3" x2="-3.3" y2="-2.3" width="0.1" layer="51"/>
<wire x1="-3.3" y1="2.3" x2="-2.3" y2="3.3" width="0.1" layer="51"/>
<wire x1="-3.3" y1="-2.3" x2="-2.3" y2="-3.3" width="0.1" layer="51"/>
<wire x1="-2.7048" y1="1.33" x2="-2.0748" y2="1.33" width="0.1" layer="51"/>
<wire x1="-2.3898" y1="1.645" x2="-2.3898" y2="1.015" width="0.1" layer="51"/>
<wire x1="-2.3" y1="3.3" x2="3.3" y2="3.3" width="0.1" layer="51"/>
<wire x1="-2.3" y1="-3.3" x2="3.3" y2="-3.3" width="0.1" layer="51"/>
<wire x1="3.3" y1="3.3" x2="3.3" y2="-3.3" width="0.1" layer="51"/>
<circle x="0" y="-0" radius="3.15" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="C_1206_3216Metric">
<smd name="1" x="-1.475" y="-0" dx="1.15" dy="1.8" layer="1" roundness="43" rot="R0"/>
<smd name="2" x="1.475" y="-0" dx="1.15" dy="1.8" layer="1" roundness="43" rot="R0"/>
<wire x1="-0.7113" y1="0.91" x2="0.7113" y2="0.91" width="0.12" layer="21"/>
<wire x1="-0.7113" y1="-0.91" x2="0.7113" y2="-0.91" width="0.12" layer="21"/>
<wire x1="-1.6" y1="0.8" x2="1.6" y2="0.8" width="0.1" layer="51"/>
<wire x1="1.6" y1="0.8" x2="1.6" y2="-0.8" width="0.1" layer="51"/>
<wire x1="1.6" y1="-0.8" x2="-1.6" y2="-0.8" width="0.1" layer="51"/>
<wire x1="-1.6" y1="-0.8" x2="-1.6" y2="0.8" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="C_0603_1608Metric">
<smd name="1" x="-0.775" y="-0" dx="0.9" dy="0.95" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.775" y="-0" dx="0.9" dy="0.95" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.1406" y1="0.51" x2="0.1406" y2="0.51" width="0.12" layer="21"/>
<wire x1="-0.1406" y1="-0.51" x2="0.1406" y2="-0.51" width="0.12" layer="21"/>
<wire x1="-0.8" y1="0.4" x2="0.8" y2="0.4" width="0.1" layer="51"/>
<wire x1="0.8" y1="0.4" x2="0.8" y2="-0.4" width="0.1" layer="51"/>
<wire x1="0.8" y1="-0.4" x2="-0.8" y2="-0.4" width="0.1" layer="51"/>
<wire x1="-0.8" y1="-0.4" x2="-0.8" y2="0.4" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="C_0805_2012Metric">
<smd name="1" x="-0.95" y="-0" dx="1" dy="1.45" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.95" y="-0" dx="1" dy="1.45" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.2613" y1="0.735" x2="0.2613" y2="0.735" width="0.12" layer="21"/>
<wire x1="-0.2613" y1="-0.735" x2="0.2613" y2="-0.735" width="0.12" layer="21"/>
<wire x1="-1" y1="0.625" x2="1" y2="0.625" width="0.1" layer="51"/>
<wire x1="1" y1="0.625" x2="1" y2="-0.625" width="0.1" layer="51"/>
<wire x1="1" y1="-0.625" x2="-1" y2="-0.625" width="0.1" layer="51"/>
<wire x1="-1" y1="-0.625" x2="-1" y2="0.625" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="ONSemi_SO-8FL_488AA">
<smd name="1" x="-2.7473" y="1.905" dx="1" dy="0.75" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="-2.7473" y="0.635" dx="1" dy="0.75" layer="1" roundness="50" rot="R0"/>
<smd name="3" x="-2.7473" y="-0.635" dx="1" dy="0.75" layer="1" roundness="50" rot="R0"/>
<smd name="4" x="-2.7473" y="-1.905" dx="1" dy="0.75" layer="1" roundness="50" rot="R0"/>
<smd name="5" x="0.745" y="-0" dx="4.055" dy="4.56" layer="1" roundness="0" rot="R0"/>
<polygon width="0" layer="1"><vertex x="0.0475" y="2.28"/><vertex x="3.2475" y="2.28"/><vertex x="3.2475" y="1.53"/><vertex x="2.65" y="1.53"/><vertex x="2.65" y="-1.53"/><vertex x="3.2475" y="-1.53"/><vertex x="3.2475" y="-2.28"/><vertex x="0.0475" y="-2.28"/><vertex x="0.0475" y="-2.775"/><vertex x="-0.8575" y="-2.775"/><vertex x="-0.8575" y="2.775"/><vertex x="0.0475" y="2.775"/></polygon>
<polygon width="0" layer="29"><vertex x="0.0475" y="2.28"/><vertex x="3.2475" y="2.28"/><vertex x="3.2475" y="1.53"/><vertex x="2.65" y="1.53"/><vertex x="2.65" y="-1.53"/><vertex x="3.2475" y="-1.53"/><vertex x="3.2475" y="-2.28"/><vertex x="0.0475" y="-2.28"/><vertex x="0.0475" y="-2.775"/><vertex x="-0.8575" y="-2.775"/><vertex x="-0.8575" y="2.775"/><vertex x="0.0475" y="2.775"/></polygon>
<polygon width="0" layer="31"><vertex x="0.0475" y="2.28"/><vertex x="3.2475" y="2.28"/><vertex x="3.2475" y="1.53"/><vertex x="2.65" y="1.53"/><vertex x="2.65" y="-1.53"/><vertex x="3.2475" y="-1.53"/><vertex x="3.2475" y="-2.28"/><vertex x="0.0475" y="-2.28"/><vertex x="0.0475" y="-2.775"/><vertex x="-0.8575" y="-2.775"/><vertex x="-0.8575" y="2.775"/><vertex x="0.0475" y="2.775"/></polygon>
<wire x1="-1.1182" y1="2.56" x2="-2.1033" y2="2.56" width="0.12" layer="21"/>
<wire x1="-1.1182" y1="-2.56" x2="-2.1033" y2="-2.56" width="0.12" layer="21"/>
<wire x1="3.0777" y1="2.56" x2="0.3078" y2="2.56" width="0.12" layer="21"/>
<wire x1="3.0777" y1="-2.56" x2="0.3078" y2="-2.56" width="0.12" layer="21"/>
<wire x1="3.1077" y1="1.27" x2="3.1077" y2="-1.27" width="0.12" layer="21"/>
<wire x1="-2.9022" y1="1.275" x2="-2.9022" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-1.7272" y1="2.45" x2="-2.9022" y2="1.275" width="0.1" layer="51"/>
<wire x1="2.9977" y1="2.45" x2="-1.7272" y2="2.45" width="0.1" layer="51"/>
<wire x1="2.9977" y1="2.45" x2="2.9977" y2="-2.45" width="0.1" layer="51"/>
<wire x1="2.9977" y1="-2.45" x2="-2.9022" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-2.75" y1="2.55" x2="-2.51" y2="2.88" width="0.12" layer="21"/>
<wire x1="-2.51" y1="2.88" x2="-2.99" y2="2.88" width="0.12" layer="21"/>
<wire x1="-2.99" y1="2.88" x2="-2.75" y2="2.55" width="0.12" layer="21"/>
<wire x1="-2.75" y1="2.55" x2="-2.75" y2="2.55" width="0.12" layer="21"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="SOT-23">
<smd name="1" x="-0.9375" y="0.95" dx="1.475" dy="0.6" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="-0.9375" y="-0.95" dx="1.475" dy="0.6" layer="1" roundness="50" rot="R0"/>
<smd name="3" x="0.9375" y="-0" dx="1.475" dy="0.6" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.76" y1="1.56" x2="0.76" y2="1.56" width="0.12" layer="21"/>
<wire x1="-0.76" y1="1.51" x2="-0.76" y2="1.56" width="0.12" layer="21"/>
<wire x1="-0.76" y1="-0.39" x2="-0.76" y2="0.39" width="0.12" layer="21"/>
<wire x1="-0.76" y1="-1.56" x2="-0.76" y2="-1.51" width="0.12" layer="21"/>
<wire x1="0.76" y1="1.56" x2="0.76" y2="0.56" width="0.12" layer="21"/>
<wire x1="0.76" y1="-0.56" x2="0.76" y2="-1.56" width="0.12" layer="21"/>
<wire x1="0.76" y1="-1.56" x2="-0.76" y2="-1.56" width="0.12" layer="21"/>
<wire x1="-1.3" y1="0.38" x2="-1.06" y2="0.05" width="0.12" layer="21"/>
<wire x1="-1.06" y1="0.05" x2="-1.54" y2="0.05" width="0.12" layer="21"/>
<wire x1="-1.54" y1="0.05" x2="-1.3" y2="0.38" width="0.12" layer="21"/>
<wire x1="-0.325" y1="1.45" x2="0.65" y2="1.45" width="0.1" layer="51"/>
<wire x1="0.65" y1="1.45" x2="0.65" y2="-1.45" width="0.1" layer="51"/>
<wire x1="0.65" y1="-1.45" x2="-0.65" y2="-1.45" width="0.1" layer="51"/>
<wire x1="-0.65" y1="-1.45" x2="-0.65" y2="1.125" width="0.1" layer="51"/>
<wire x1="-0.65" y1="1.125" x2="-0.325" y2="1.45" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="Fuse_1206_3216Metric">
<smd name="1" x="-1.4" y="-0" dx="1.25" dy="1.75" layer="1" roundness="40" rot="R0"/>
<smd name="2" x="1.4" y="-0" dx="1.25" dy="1.75" layer="1" roundness="40" rot="R0"/>
<wire x1="-0.6021" y1="0.91" x2="0.6021" y2="0.91" width="0.12" layer="21"/>
<wire x1="-0.6021" y1="-0.91" x2="0.6021" y2="-0.91" width="0.12" layer="21"/>
<wire x1="-1.6" y1="0.8" x2="1.6" y2="0.8" width="0.1" layer="51"/>
<wire x1="1.6" y1="0.8" x2="1.6" y2="-0.8" width="0.1" layer="51"/>
<wire x1="1.6" y1="-0.8" x2="-1.6" y2="-0.8" width="0.1" layer="51"/>
<wire x1="-1.6" y1="-0.8" x2="-1.6" y2="0.8" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="L_Sunlord_MWSA0603S">
<smd name="1" x="-3.025" y="-0" dx="2.35" dy="3.5" layer="1" roundness="21" rot="R0"/>
<smd name="2" x="3.025" y="-0" dx="2.35" dy="3.5" layer="1" roundness="21" rot="R0"/>
<wire x1="-3.61" y1="3.41" x2="3.61" y2="3.41" width="0.12" layer="21"/>
<wire x1="-3.61" y1="2.01" x2="-3.61" y2="3.41" width="0.12" layer="21"/>
<wire x1="-3.61" y1="-3.41" x2="-3.61" y2="-2.01" width="0.12" layer="21"/>
<wire x1="3.61" y1="3.41" x2="3.61" y2="2.01" width="0.12" layer="21"/>
<wire x1="3.61" y1="-2.01" x2="3.61" y2="-3.41" width="0.12" layer="21"/>
<wire x1="3.61" y1="-3.41" x2="-3.61" y2="-3.41" width="0.12" layer="21"/>
<wire x1="-3.5" y1="3.3" x2="3.5" y2="3.3" width="0.1" layer="51"/>
<wire x1="3.5" y1="3.3" x2="3.5" y2="-3.3" width="0.1" layer="51"/>
<wire x1="3.5" y1="-3.3" x2="-3.5" y2="-3.3" width="0.1" layer="51"/>
<wire x1="-3.5" y1="-3.3" x2="-3.5" y2="3.3" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="JST_GH_SM08B-GHS-TB_1x08-1MP_P1.25mm_Horizontal">
<smd name="1" x="-4.375" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="-3.125" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="3" x="-1.875" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="4" x="-0.625" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="5" x="0.625" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="6" x="1.875" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="7" x="3.125" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="8" x="4.375" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="MP1" x="-6.225" y="-1.35" dx="1" dy="2.7" layer="1" roundness="50" rot="R0"/>
<smd name="MP2" x="6.225" y="-1.35" dx="1" dy="2.7" layer="1" roundness="50" rot="R0"/>
<wire x1="-6.735" y1="1.71" x2="-4.935" y2="1.71" width="0.12" layer="21"/>
<wire x1="-6.735" y1="0.26" x2="-6.735" y2="1.71" width="0.12" layer="21"/>
<wire x1="-5.465" y1="-2.56" x2="5.465" y2="-2.56" width="0.12" layer="21"/>
<wire x1="-4.935" y1="1.71" x2="-4.935" y2="2.7" width="0.12" layer="21"/>
<wire x1="6.735" y1="1.71" x2="4.935" y2="1.71" width="0.12" layer="21"/>
<wire x1="6.735" y1="0.26" x2="6.735" y2="1.71" width="0.12" layer="21"/>
<wire x1="-6.625" y1="1.6" x2="-6.625" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-6.625" y1="1.6" x2="6.625" y2="1.6" width="0.1" layer="51"/>
<wire x1="-6.625" y1="-2.45" x2="6.625" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-4.875" y1="1.6" x2="-4.375" y2="0.8929" width="0.1" layer="51"/>
<wire x1="-4.375" y1="0.8929" x2="-3.875" y2="1.6" width="0.1" layer="51"/>
<wire x1="6.625" y1="1.6" x2="6.625" y2="-2.45" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
<package name="JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal">
<smd name="1" x="-3.125" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="-1.875" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="3" x="-0.625" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="4" x="0.625" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="5" x="1.875" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="6" x="3.125" y="1.85" dx="0.6" dy="1.7" layer="1" roundness="50" rot="R0"/>
<smd name="MP1" x="-4.975" y="-1.35" dx="1" dy="2.7" layer="1" roundness="50" rot="R0"/>
<smd name="MP2" x="4.975" y="-1.35" dx="1" dy="2.7" layer="1" roundness="50" rot="R0"/>
<wire x1="-5.485" y1="1.71" x2="-3.685" y2="1.71" width="0.12" layer="21"/>
<wire x1="-5.485" y1="0.26" x2="-5.485" y2="1.71" width="0.12" layer="21"/>
<wire x1="-4.215" y1="-2.56" x2="4.215" y2="-2.56" width="0.12" layer="21"/>
<wire x1="-3.685" y1="1.71" x2="-3.685" y2="2.7" width="0.12" layer="21"/>
<wire x1="5.485" y1="1.71" x2="3.685" y2="1.71" width="0.12" layer="21"/>
<wire x1="5.485" y1="0.26" x2="5.485" y2="1.71" width="0.12" layer="21"/>
<wire x1="-5.375" y1="1.6" x2="-5.375" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-5.375" y1="1.6" x2="5.375" y2="1.6" width="0.1" layer="51"/>
<wire x1="-5.375" y1="-2.45" x2="5.375" y2="-2.45" width="0.1" layer="51"/>
<wire x1="-3.625" y1="1.6" x2="-3.125" y2="0.8929" width="0.1" layer="51"/>
<wire x1="-3.125" y1="0.8929" x2="-2.625" y2="1.6" width="0.1" layer="51"/>
<wire x1="5.375" y1="1.6" x2="5.375" y2="-2.45" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
</package>
</packages>
<symbols>
<symbol name="C">
<wire x1="-2.54" y1="0" x2="-0.508" y2="0" width="0.1524" layer="94"/>
<wire x1="0.508" y1="0" x2="2.54" y2="0" width="0.1524" layer="94"/>
<rectangle x1="-0.762" y1="-1.524" x2="-0.254" y2="1.524" layer="94"/>
<rectangle x1="0.254" y1="-1.524" x2="0.762" y2="1.524" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="2" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="CONN2">
<wire x1="-2.54" y1="-2.54" x2="2.54" y2="-2.54" width="0.254" layer="94"/>
<wire x1="2.54" y1="-2.54" x2="2.54" y2="2.54" width="0.254" layer="94"/>
<wire x1="2.54" y1="2.54" x2="-2.54" y2="2.54" width="0.254" layer="94"/>
<wire x1="-2.54" y1="2.54" x2="-2.54" y2="-2.54" width="0.254" layer="94"/>
<text x="-2.54" y="3.81" size="1.778" layer="95">&gt;NAME</text>
<text x="-2.54" y="-5.588" size="1.778" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="1.27" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="2" x="-5.08" y="-1.27" visible="pin" length="short" direction="pas" rot="R0"/>
</symbol>
<symbol name="CONN3">
<wire x1="-2.54" y1="-3.81" x2="2.54" y2="-3.81" width="0.254" layer="94"/>
<wire x1="2.54" y1="-3.81" x2="2.54" y2="3.81" width="0.254" layer="94"/>
<wire x1="2.54" y1="3.81" x2="-2.54" y2="3.81" width="0.254" layer="94"/>
<wire x1="-2.54" y1="3.81" x2="-2.54" y2="-3.81" width="0.254" layer="94"/>
<text x="-2.54" y="5.08" size="1.778" layer="95">&gt;NAME</text>
<text x="-2.54" y="-6.8580000000000005" size="1.778" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="2.54" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="2" x="-5.08" y="0" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="3" x="-5.08" y="-2.54" visible="pin" length="short" direction="pas" rot="R0"/>
</symbol>
<symbol name="CONN6">
<wire x1="-2.54" y1="-7.62" x2="2.54" y2="-7.62" width="0.254" layer="94"/>
<wire x1="2.54" y1="-7.62" x2="2.54" y2="7.62" width="0.254" layer="94"/>
<wire x1="2.54" y1="7.62" x2="-2.54" y2="7.62" width="0.254" layer="94"/>
<wire x1="-2.54" y1="7.62" x2="-2.54" y2="-7.62" width="0.254" layer="94"/>
<text x="-2.54" y="8.89" size="1.778" layer="95">&gt;NAME</text>
<text x="-2.54" y="-10.668" size="1.778" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="6.35" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="2" x="-5.08" y="3.81" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="3" x="-5.08" y="1.27" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="4" x="-5.08" y="-1.27" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="5" x="-5.08" y="-3.81" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="6" x="-5.08" y="-6.35" visible="pin" length="short" direction="pas" rot="R0"/>
</symbol>
<symbol name="CONN8">
<wire x1="-2.54" y1="-10.16" x2="2.54" y2="-10.16" width="0.254" layer="94"/>
<wire x1="2.54" y1="-10.16" x2="2.54" y2="10.16" width="0.254" layer="94"/>
<wire x1="2.54" y1="10.16" x2="-2.54" y2="10.16" width="0.254" layer="94"/>
<wire x1="-2.54" y1="10.16" x2="-2.54" y2="-10.16" width="0.254" layer="94"/>
<text x="-2.54" y="11.43" size="1.778" layer="95">&gt;NAME</text>
<text x="-2.54" y="-13.208" size="1.778" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="8.89" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="2" x="-5.08" y="6.35" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="3" x="-5.08" y="3.81" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="4" x="-5.08" y="1.27" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="5" x="-5.08" y="-1.27" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="6" x="-5.08" y="-3.81" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="7" x="-5.08" y="-6.35" visible="pin" length="short" direction="pas" rot="R0"/>
<pin name="8" x="-5.08" y="-8.89" visible="pin" length="short" direction="pas" rot="R0"/>
</symbol>
<symbol name="D">
<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.1524" layer="94"/>
<polygon width="0.1524" layer="94"><vertex x="-1.27" y="1.27"/><vertex x="1.27" y="0"/><vertex x="-1.27" y="-1.27"/></polygon>
<wire x1="1.27" y1="1.27" x2="1.27" y2="-1.27" width="0.254" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="A" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="C" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="FUSE">
<wire x1="-2.54" y1="-1.016" x2="2.54" y2="-1.016" width="0.254" layer="94"/>
<wire x1="2.54" y1="-1.016" x2="2.54" y2="1.016" width="0.254" layer="94"/>
<wire x1="2.54" y1="1.016" x2="-2.54" y2="1.016" width="0.254" layer="94"/>
<wire x1="-2.54" y1="1.016" x2="-2.54" y2="-1.016" width="0.254" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="2" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="INA228">
<wire x1="7.62" y1="5.08" x2="7.62" y2="-15.24" width="0.1524" layer="94"/>
<wire x1="7.62" y1="-15.24" x2="33.02" y2="-15.24" width="0.1524" layer="94"/>
<wire x1="33.02" y1="-15.24" x2="33.02" y2="5.08" width="0.1524" layer="94"/>
<wire x1="33.02" y1="5.08" x2="7.62" y2="5.08" width="0.1524" layer="94"/>
<text x="15.5956" y="9.1186" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="14.9606" y="6.5786" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="A1" x="0" y="0" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="A0" x="0" y="-2.54" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="ALERT" x="0" y="-5.08" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="SDA" x="0" y="-7.62" visible="both" length="long" direction="io" swaplevel="0" rot="R0"/>
<pin name="SCL" x="0" y="-10.16" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="VS" x="40.64" y="-10.16" visible="both" length="long" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="GND" x="40.64" y="-7.62" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
<pin name="VBUS" x="40.64" y="-5.08" visible="both" length="long" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="IN-" x="40.64" y="-2.54" visible="both" length="long" direction="in" swaplevel="0" rot="R180"/>
<pin name="IN+" x="40.64" y="0" visible="both" length="long" direction="in" swaplevel="0" rot="R180"/>
</symbol>
<symbol name="L">
<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.6096" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="2" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="LM74700">
<wire x1="-15.24" y1="-12.7" x2="15.24" y2="-12.7" width="0.2032" layer="94"/>
<wire x1="15.24" y1="-12.7" x2="15.24" y2="12.7" width="0.2032" layer="94"/>
<wire x1="15.24" y1="12.7" x2="-15.24" y2="12.7" width="0.2032" layer="94"/>
<wire x1="-15.24" y1="12.7" x2="-15.24" y2="-12.7" width="0.2032" layer="94"/>
<text x="-4.7244" y="1.4986" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="VCAP" x="-20.32" y="0" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="GND" x="20.32" y="-7.62" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="EN" x="-20.32" y="-7.62" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="CATHODE" x="20.32" y="0" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="GATE" x="20.32" y="7.62" visible="both" length="middle" direction="out" swaplevel="0" rot="R180"/>
<pin name="ANODE" x="-20.32" y="7.62" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
</symbol>
<symbol name="NMOS">
<wire x1="-2.54" y1="0" x2="-0.762" y2="0" width="0.1524" layer="94"/>
<wire x1="-0.762" y1="-1.778" x2="-0.762" y2="1.778" width="0.254" layer="94"/>
<wire x1="0" y1="-2.032" x2="0" y2="2.032" width="0.254" layer="94"/>
<wire x1="0" y1="1.524" x2="2.54" y2="1.524" width="0.1524" layer="94"/>
<wire x1="2.54" y1="1.524" x2="2.54" y2="2.54" width="0.1524" layer="94"/>
<wire x1="0" y1="-1.524" x2="2.54" y2="-1.524" width="0.1524" layer="94"/>
<wire x1="2.54" y1="-1.524" x2="2.54" y2="-2.54" width="0.1524" layer="94"/>
<text x="3.81" y="0.508" size="1.27" layer="95">&gt;NAME</text>
<text x="3.81" y="-1.778" size="1.27" layer="96">&gt;VALUE</text>
<pin name="G" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="D" x="2.54" y="5.08" visible="off" length="short" direction="pas" rot="R270"/>
<pin name="S" x="2.54" y="-5.08" visible="off" length="short" direction="pas" rot="R90"/>
</symbol>
<symbol name="R">
<wire x1="-2.54" y1="-1.016" x2="2.54" y2="-1.016" width="0.254" layer="94"/>
<wire x1="2.54" y1="-1.016" x2="2.54" y2="1.016" width="0.254" layer="94"/>
<wire x1="2.54" y1="1.016" x2="-2.54" y2="1.016" width="0.254" layer="94"/>
<wire x1="-2.54" y1="1.016" x2="-2.54" y2="-1.016" width="0.254" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="2" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="TCA9534">
<wire x1="5.08" y1="-27.94" x2="30.48" y2="-27.94" width="0.2032" layer="94"/>
<wire x1="30.48" y1="-27.94" x2="30.48" y2="5.08" width="0.2032" layer="94"/>
<wire x1="30.48" y1="5.08" x2="5.08" y2="5.08" width="0.2032" layer="94"/>
<wire x1="5.08" y1="5.08" x2="5.08" y2="-27.94" width="0.2032" layer="94"/>
<text x="-4.7244" y="1.4986" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="A0" x="0" y="-20.32" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="A1" x="0" y="-22.86" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="A2" x="0" y="-25.4" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="P0" x="35.56" y="0" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P1" x="35.56" y="-2.54" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P2" x="35.56" y="-5.08" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P3" x="35.56" y="-7.62" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="GND" x="35.56" y="-25.4" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="P4" x="35.56" y="-10.16" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P5" x="35.56" y="-12.7" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P6" x="35.56" y="-15.24" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="P7" x="35.56" y="-17.78" visible="both" length="middle" direction="io" swaplevel="0" rot="R180"/>
<pin name="INT_N" x="0" y="-5.08" visible="both" length="middle" direction="oc" swaplevel="0" rot="R0"/>
<pin name="SCL" x="0" y="-10.16" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="SDA" x="0" y="-15.24" visible="both" length="middle" direction="io" swaplevel="0" rot="R0"/>
<pin name="VCC" x="0" y="0" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
</symbol>
<symbol name="TPS25982">
<wire x1="7.62" y1="5.08" x2="7.62" y2="-35.56" width="0.1524" layer="94"/>
<wire x1="7.62" y1="-35.56" x2="43.18" y2="-35.56" width="0.1524" layer="94"/>
<wire x1="43.18" y1="-35.56" x2="43.18" y2="5.08" width="0.1524" layer="94"/>
<wire x1="43.18" y1="5.08" x2="7.62" y2="5.08" width="0.1524" layer="94"/>
<text x="20.6756" y="9.1186" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="20.0406" y="6.5786" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="IN_2" x="0" y="0" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="IN_3" x="0" y="-2.54" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="IN_4" x="0" y="-5.08" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="GND_2" x="0" y="-7.62" visible="both" length="long" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="GND_3" x="0" y="-10.16" visible="both" length="long" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="EN/UVLO" x="0" y="-12.7" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="ITIMER" x="0" y="-15.24" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="ILIM" x="0" y="-17.78" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="IMON" x="0" y="-20.32" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="RETRY_DLY" x="0" y="-22.86" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="NRETRY" x="0" y="-25.4" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="LDSTRT" x="0" y="-27.94" visible="both" length="long" direction="in" swaplevel="0" rot="R0"/>
<pin name="PG" x="0" y="-30.48" visible="both" length="long" direction="out" swaplevel="0" rot="R0"/>
<pin name="GND" x="50.8" y="-30.48" visible="both" length="long" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="DVDT" x="50.8" y="-27.94" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="IN" x="50.8" y="-25.4" visible="both" length="long" direction="in" swaplevel="0" rot="R180"/>
<pin name="OUT_2" x="50.8" y="-22.86" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_3" x="50.8" y="-20.32" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_4" x="50.8" y="-17.78" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_5" x="50.8" y="-15.24" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_6" x="50.8" y="-12.7" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_7" x="50.8" y="-10.16" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT_8" x="50.8" y="-7.62" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="OUT" x="50.8" y="-5.08" visible="both" length="long" direction="out" swaplevel="0" rot="R180"/>
<pin name="EPAD_2" x="50.8" y="-2.54" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
<pin name="EPAD" x="50.8" y="0" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
</symbol>
<symbol name="TPS62933">
<wire x1="7.62" y1="5.08" x2="7.62" y2="-12.7" width="0.1524" layer="94"/>
<wire x1="7.62" y1="-12.7" x2="33.02" y2="-12.7" width="0.1524" layer="94"/>
<wire x1="33.02" y1="-12.7" x2="33.02" y2="5.08" width="0.1524" layer="94"/>
<wire x1="33.02" y1="5.08" x2="7.62" y2="5.08" width="0.1524" layer="94"/>
<text x="15.5956" y="9.1186" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="14.9606" y="6.5786" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="RT" x="0" y="0" visible="both" length="long" direction="pas" swaplevel="0" rot="R0"/>
<pin name="EN" x="0" y="-2.54" visible="both" length="long" direction="pas" swaplevel="0" rot="R0"/>
<pin name="VIN" x="0" y="-5.08" visible="both" length="long" direction="pas" swaplevel="0" rot="R0"/>
<pin name="GND" x="0" y="-7.62" visible="both" length="long" direction="pas" swaplevel="0" rot="R0"/>
<pin name="SW" x="40.64" y="-7.62" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
<pin name="BST" x="40.64" y="-5.08" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
<pin name="SS" x="40.64" y="-2.54" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
<pin name="FB" x="40.64" y="0" visible="both" length="long" direction="pas" swaplevel="0" rot="R180"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="C" prefix="C" uservalue="yes">
<gates>
<gate name="G$1" symbol="C" x="0" y="0"/>
</gates>
<devices>
<device name="C0603" package="C_0603_1608Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="C0805" package="C_0805_2012Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="C1206" package="C_1206_3216Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="CP6.3x9.9" package="CP_Elec_6.3x9.9">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="CONN2" prefix="J" uservalue="yes">
<gates>
<gate name="G$1" symbol="CONN2" x="0" y="0"/>
</gates>
<devices>
<device name="PH2" package="JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="XT30" package="AMASS_XT30PW-M_1x02_P2.50mm_Horizontal">
<connects>
<connect gate="G$1" pin="1" pad="2"/>
<connect gate="G$1" pin="2" pad="1"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="CONN3" prefix="J" uservalue="yes">
<gates>
<gate name="G$1" symbol="CONN3" x="0" y="0"/>
</gates>
<devices>
<device name="PH3" package="JST_PH_S3B-PH-K_1x03_P2.00mm_Horizontal">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="CONN6" prefix="J" uservalue="yes">
<gates>
<gate name="G$1" symbol="CONN6" x="0" y="0"/>
</gates>
<devices>
<device name="GH6" package="JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
<connect gate="G$1" pin="5" pad="5"/>
<connect gate="G$1" pin="6" pad="6"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="CONN8" prefix="J" uservalue="yes">
<gates>
<gate name="G$1" symbol="CONN8" x="0" y="0"/>
</gates>
<devices>
<device name="GH8" package="JST_GH_SM08B-GHS-TB_1x08-1MP_P1.25mm_Horizontal">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
<connect gate="G$1" pin="5" pad="5"/>
<connect gate="G$1" pin="6" pad="6"/>
<connect gate="G$1" pin="7" pad="7"/>
<connect gate="G$1" pin="8" pad="8"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="D" prefix="D" uservalue="yes">
<gates>
<gate name="G$1" symbol="D" x="0" y="0"/>
</gates>
<devices>
<device name="SMB" package="D_SMB">
<connects>
<connect gate="G$1" pin="A" pad="2"/>
<connect gate="G$1" pin="C" pad="1"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="X1SON2" package="D_0402_1005Metric">
<connects>
<connect gate="G$1" pin="A" pad="2"/>
<connect gate="G$1" pin="C" pad="1"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="FUSE" prefix="F" uservalue="yes">
<gates>
<gate name="G$1" symbol="FUSE" x="0" y="0"/>
</gates>
<devices>
<device name="F1206" package="Fuse_1206_3216Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="INA228" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="INA228" x="0" y="0"/>
</gates>
<devices>
<device name="" package="VSSOP10_DGS_TEX">
<connects>
<connect gate="A" pin="A1" pad="1"/>
<connect gate="A" pin="A0" pad="2"/>
<connect gate="A" pin="ALERT" pad="3"/>
<connect gate="A" pin="SDA" pad="4"/>
<connect gate="A" pin="SCL" pad="5"/>
<connect gate="A" pin="VS" pad="6"/>
<connect gate="A" pin="GND" pad="7"/>
<connect gate="A" pin="VBUS" pad="8"/>
<connect gate="A" pin="IN-" pad="9"/>
<connect gate="A" pin="IN+" pad="10"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="L" prefix="L" uservalue="yes">
<gates>
<gate name="G$1" symbol="L" x="0" y="0"/>
</gates>
<devices>
<device name="MWSA0603S" package="L_Sunlord_MWSA0603S">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="LM74700" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="LM74700" x="0" y="0"/>
</gates>
<devices>
<device name="" package="DBV0006A_N">
<connects>
<connect gate="A" pin="VCAP" pad="1"/>
<connect gate="A" pin="GND" pad="2"/>
<connect gate="A" pin="EN" pad="3"/>
<connect gate="A" pin="CATHODE" pad="4"/>
<connect gate="A" pin="GATE" pad="5"/>
<connect gate="A" pin="ANODE" pad="6"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="NMOS" prefix="Q" uservalue="yes">
<gates>
<gate name="G$1" symbol="NMOS" x="0" y="0"/>
</gates>
<devices>
<device name="SO8FL" package="ONSemi_SO-8FL_488AA">
<connects>
<connect gate="G$1" pin="G" pad="4"/>
<connect gate="G$1" pin="D" pad="5"/>
<connect gate="G$1" pin="S" pad="1 2 3"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="SOT23" package="SOT-23">
<connects>
<connect gate="G$1" pin="G" pad="1"/>
<connect gate="G$1" pin="D" pad="3"/>
<connect gate="G$1" pin="S" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="R" prefix="R" uservalue="yes">
<gates>
<gate name="G$1" symbol="R" x="0" y="0"/>
</gates>
<devices>
<device name="0603" package="R_0603_1608Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
<device name="2512" package="R_2512_6332Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="TCA9534" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="TCA9534" x="0" y="0"/>
</gates>
<devices>
<device name="" package="PW0016A-MFG">
<connects>
<connect gate="A" pin="A0" pad="1"/>
<connect gate="A" pin="A1" pad="2"/>
<connect gate="A" pin="A2" pad="3"/>
<connect gate="A" pin="P0" pad="4"/>
<connect gate="A" pin="P1" pad="5"/>
<connect gate="A" pin="P2" pad="6"/>
<connect gate="A" pin="P3" pad="7"/>
<connect gate="A" pin="GND" pad="8"/>
<connect gate="A" pin="P4" pad="9"/>
<connect gate="A" pin="P5" pad="10"/>
<connect gate="A" pin="P6" pad="11"/>
<connect gate="A" pin="P7" pad="12"/>
<connect gate="A" pin="INT_N" pad="13"/>
<connect gate="A" pin="SCL" pad="14"/>
<connect gate="A" pin="SDA" pad="15"/>
<connect gate="A" pin="VCC" pad="16"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="TPS25982" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="TPS25982" x="0" y="0"/>
</gates>
<devices>
<device name="" package="RGE24_2P5XP85X1P45_TEX">
<connects>
<connect gate="A" pin="IN_2" pad="1"/>
<connect gate="A" pin="IN_3" pad="2"/>
<connect gate="A" pin="IN_4" pad="3"/>
<connect gate="A" pin="GND_2" pad="4"/>
<connect gate="A" pin="GND_3" pad="5"/>
<connect gate="A" pin="EN/UVLO" pad="6"/>
<connect gate="A" pin="ITIMER" pad="7"/>
<connect gate="A" pin="ILIM" pad="8"/>
<connect gate="A" pin="IMON" pad="9"/>
<connect gate="A" pin="RETRY_DLY" pad="10"/>
<connect gate="A" pin="NRETRY" pad="11"/>
<connect gate="A" pin="LDSTRT" pad="12"/>
<connect gate="A" pin="PG" pad="13"/>
<connect gate="A" pin="GND" pad="14"/>
<connect gate="A" pin="DVDT" pad="15"/>
<connect gate="A" pin="IN" pad="16"/>
<connect gate="A" pin="OUT_2" pad="17"/>
<connect gate="A" pin="OUT_3" pad="18"/>
<connect gate="A" pin="OUT_4" pad="19"/>
<connect gate="A" pin="OUT_5" pad="20"/>
<connect gate="A" pin="OUT_6" pad="21"/>
<connect gate="A" pin="OUT_7" pad="22"/>
<connect gate="A" pin="OUT_8" pad="23"/>
<connect gate="A" pin="OUT" pad="24"/>
<connect gate="A" pin="EPAD_2" pad="25"/>
<connect gate="A" pin="EPAD" pad="26"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="TPS62933" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="TPS62933" x="0" y="0"/>
</gates>
<devices>
<device name="" package="SOT_33DRLR_TEX">
<connects>
<connect gate="A" pin="RT" pad="1"/>
<connect gate="A" pin="EN" pad="2"/>
<connect gate="A" pin="VIN" pad="3"/>
<connect gate="A" pin="GND" pad="4"/>
<connect gate="A" pin="SW" pad="5"/>
<connect gate="A" pin="BST" pad="6"/>
<connect gate="A" pin="SS" pad="7"/>
<connect gate="A" pin="FB" pad="8"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0">
</class>
</classes>
<parts>
<part name="J2" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30PW-M (dock power, from the LEMO)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="J7" library="nibbles_power" deviceset="CONN3" device="PH3" value="S3B-PH-K-S (dock data, from the LEMO)"><attribute name="MPN" value="S3B-PH-K-S"/><attribute name="LCSC" value="C157929"/></part>
<part name="D2" library="nibbles_power" deviceset="D" device="SMB" value="SMBJ18A"><attribute name="MPN" value="SMBJ18A"/><attribute name="LCSC" value="C10212"/></part>
<part name="R3" library="nibbles_power" deviceset="R" device="0603" value="1k"/>
<part name="D3" library="nibbles_power" deviceset="D" device="X1SON2" value="TPD1E10B06DPYR"><attribute name="MPN" value="TPD1E10B06DPYR"/><attribute name="LCSC" value="C48260"/></part>
<part name="R4" library="nibbles_power" deviceset="R" device="0603" value="1k"/>
<part name="D4" library="nibbles_power" deviceset="D" device="X1SON2" value="TPD1E10B06DPYR"><attribute name="MPN" value="TPD1E10B06DPYR"/><attribute name="LCSC" value="C48260"/></part>
<part name="R5" library="nibbles_power" deviceset="R" device="0603" value="100k"/>
<part name="U9" library="nibbles_power" deviceset="LM74700" device="" value="LM74700-Q1 (LEMO)"><attribute name="MPN" value="LM74700QDBVRQ1"/><attribute name="LCSC" value="C2941042"/></part>
<part name="C48" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF"/>
<part name="R35" library="nibbles_power" deviceset="R" device="0603" value="1M"/>
<part name="Q9" library="nibbles_power" deviceset="NMOS" device="SO8FL" value="30V N-FET &gt;= 15 A"><attribute name="MPN" value="NTMFS4C022NT1G"/><attribute name="LCSC" value="C179623"/></part>
<part name="R36" library="nibbles_power" deviceset="R" device="0603" value="7.5k"/>
<part name="J8" library="nibbles_power" deviceset="CONN2" device="PH2" value="S2B-PH-K-S (dock LED, panel)"><attribute name="MPN" value="S2B-PH-K-S"/><attribute name="LCSC" value="C173752"/></part>
<part name="J3" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (pack, from 4S BMS)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="F1" library="nibbles_power" deviceset="FUSE" device="F1206" value="15A"><attribute name="MPN" value="CC12H15A-TR"/><attribute name="LCSC" value="C2443310"/></part>
<part name="U3" library="nibbles_power" deviceset="LM74700" device="" value="LM74700-Q1 (dock)"><attribute name="MPN" value="LM74700QDBVRQ1"/><attribute name="LCSC" value="C2941042"/></part>
<part name="C30" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF"/>
<part name="R20" library="nibbles_power" deviceset="R" device="0603" value="1M"/>
<part name="Q5" library="nibbles_power" deviceset="NMOS" device="SO8FL" value="30V N-FET &gt;= 15 A"><attribute name="MPN" value="NTMFS4C022NT1G"/><attribute name="LCSC" value="C179623"/></part>
<part name="U4" library="nibbles_power" deviceset="LM74700" device="" value="LM74700-Q1 (pack)"><attribute name="MPN" value="LM74700QDBVRQ1"/><attribute name="LCSC" value="C2941042"/></part>
<part name="C31" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF"/>
<part name="R21" library="nibbles_power" deviceset="R" device="0603" value="1M"/>
<part name="Q6" library="nibbles_power" deviceset="NMOS" device="SO8FL" value="30V N-FET &gt;= 15 A"><attribute name="MPN" value="NTMFS4C022NT1G"/><attribute name="LCSC" value="C179623"/></part>
<part name="Q7" library="nibbles_power" deviceset="NMOS" device="SOT23" value="2N7002"><attribute name="MPN" value="2N7002"/><attribute name="LCSC" value="C8545"/></part>
<part name="R22" library="nibbles_power" deviceset="R" device="0603" value="100k"/>
<part name="C32" library="nibbles_power" deviceset="C" device="CP6.3x9.9" value="220uF 25V polymer"><attribute name="MPN" value="KNM2220UF149EC0051"/><attribute name="LCSC" value="C2981459"/></part>
<part name="C33" library="nibbles_power" deviceset="C" device="CP6.3x9.9" value="220uF 25V polymer"><attribute name="MPN" value="KNM2220UF149EC0051"/><attribute name="LCSC" value="C2981459"/></part>
<part name="C34" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="U5" library="nibbles_power" deviceset="TPS25982" device="" value="TPS259827ONRGET"><attribute name="MPN" value="TPS259827ONRGET"/><attribute name="LCSC" value="C2155765"/></part>
<part name="R23" library="nibbles_power" deviceset="R" device="0603" value="86.6k 1%"/>
<part name="R24" library="nibbles_power" deviceset="R" device="0603" value="10k 1%"/>
<part name="Q8" library="nibbles_power" deviceset="NMOS" device="SOT23" value="2N7002"><attribute name="MPN" value="2N7002"/><attribute name="LCSC" value="C8545"/></part>
<part name="R25" library="nibbles_power" deviceset="R" device="0603" value="100k"/>
<part name="R26" library="nibbles_power" deviceset="R" device="0603" value="130 1%"/>
<part name="C35" library="nibbles_power" deviceset="C" device="C0603" value="10nF (TBD)"/>
<part name="C36" library="nibbles_power" deviceset="C" device="C0603" value="100nF (TBD)"/>
<part name="C37" library="nibbles_power" deviceset="C" device="C0603" value="22nF (TBD)"/>
<part name="R27" library="nibbles_power" deviceset="R" device="0603" value="TBD (IMON)"/>
<part name="R28" library="nibbles_power" deviceset="R" device="0603" value="10k"/>
<part name="R29" library="nibbles_power" deviceset="R" device="2512" value="2 mOhm 2512 3W"><attribute name="MPN" value="LR2512-23R002F4"/><attribute name="LCSC" value="C154685"/></part>
<part name="U6" library="nibbles_power" deviceset="INA228" device="" value="INA226AIDGSR"><attribute name="MPN" value="INA226AIDGSR"/><attribute name="LCSC" value="C49851"/></part>
<part name="C38" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF"/>
<part name="J5" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (to the 5 V lighting converter)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="U7" library="nibbles_power" deviceset="TPS62933" device="" value="TPS62933DRLR"><attribute name="MPN" value="TPS62933DRLR"/><attribute name="LCSC" value="C3200405"/></part>
<part name="C40" library="nibbles_power" deviceset="C" device="C1206" value="10uF 35V"/>
<part name="C41" library="nibbles_power" deviceset="C" device="C1206" value="10uF 35V"/>
<part name="C42" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF 35V"/>
<part name="C43" library="nibbles_power" deviceset="C" device="C0603" value="100nF"/>
<part name="L2" library="nibbles_power" deviceset="L" device="MWSA0603S" value="6.8uH Isat&gt;=4A"><attribute name="MPN" value="MWSA0603S-6R8MT"/><attribute name="LCSC" value="C408449"/></part>
<part name="C44" library="nibbles_power" deviceset="C" device="C0805" value="22uF 10V"/>
<part name="C45" library="nibbles_power" deviceset="C" device="C0805" value="22uF 10V"/>
<part name="R30" library="nibbles_power" deviceset="R" device="0603" value="52.3k 1%"/>
<part name="R31" library="nibbles_power" deviceset="R" device="0603" value="10k 1%"/>
<part name="C46" library="nibbles_power" deviceset="C" device="C0603" value="10nF"/>
<part name="U8" library="nibbles_power" deviceset="TCA9534" device="" value="TCA9534PWR"><attribute name="MPN" value="TCA9534PWR"/><attribute name="LCSC" value="C783615"/></part>
<part name="C47" library="nibbles_power" deviceset="C" device="C0603" value="0.1uF"/>
<part name="R32" library="nibbles_power" deviceset="R" device="0603" value="4.7k"/>
<part name="R33" library="nibbles_power" deviceset="R" device="0603" value="4.7k"/>
<part name="R16" library="nibbles_power" deviceset="R" device="0603" value="10k"/>
<part name="J6" library="nibbles_power" deviceset="CONN8" device="GH8" value="To the base board (JST-GH 8)"><attribute name="MPN" value="SM08B-GHS-TB"/><attribute name="LCSC" value="C265111"/></part>
<part name="J10" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (charger port: input)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="J11" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (charger port: battery)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="J12" library="nibbles_power" deviceset="CONN6" device="GH6" value="JST-GH 6 (charger port: I2C)"><attribute name="MPN" value="SM06B-GHS-TB"/></part>
<part name="J9" library="nibbles_power" deviceset="CONN6" device="GH6" value="JST-GH 6 (keypad port: I2C)"><attribute name="MPN" value="SM06B-GHS-TB"/></part>
</parts>
<sheets>
<sheet>
<plain>
<text x="0" y="12.7" size="3.81" layer="97">1 Input: dock (LEMO)</text>
<text x="0" y="5.08" size="2.1844" layer="97">The LEMO is the only power input: the dock, or a separate USB-C PD to LEMO charging dongle at home.</text>
<text x="0" y="1.27" size="2.1844" layer="97">Unused refs: J1, U1, D1, R1, C1, Q1-Q4, R34 (an earlier USB-C input), J4 and everything on U2 (the charger board).</text>
<text x="0" y="-2.54" size="2.1844" layer="97">Dock: the LEMO 1K.308 sits in the enclosure wall and is wired to this board: J2 (XT30) takes its battery</text>
<text x="0" y="-6.35" size="2.1844" layer="97">pins (LEMO 1-3 +, 4-6 ground), J7 (JST-PH 3) its data (LEMO 7, UART to the base), dock detect (8) and a ground.</text>
<text x="0" y="-10.16" size="2.1844" layer="97">TVS: SMBJ18A on the dock input (16.8 V max), clamping under the 30 V FETs.</text>
<text x="0" y="-13.97" size="2.1844" layer="97">U9/Q9: ideal diode right after the LEMO, so no current ever leaves the shark through it (the pack, the charger,</text>
<text x="0" y="-17.78" size="2.1844" layer="97">or plan B's USB-C modules feeding back into the dock). U3/Q5 on sheet 2 is the dock's separate OR diode into</text>
<text x="0" y="-21.59" size="2.1844" layer="97">the lights: without it, the pack would feed the charger's input through LIGHT_BUS.</text>
<text x="0" y="-25.4" size="2.1844" layer="97">J8: a panel LED (with leads, no resistor) lit whenever the dock supplies power, firmware or not (~1-2 mA).</text>
</plain>
<instances>
<instance part="U9" gate="A" x="160.02" y="-60.96"/>
<instance part="R35" gate="G$1" x="114.3" y="-83.82" rot="R90"/>
<instance part="C48" gate="G$1" x="91.44" y="-83.82" rot="R270"/>
<instance part="D2" gate="G$1" x="63.5" y="-83.82" rot="R90"/>
<instance part="J2" gate="G$1" x="256.54" y="-55.88"/>
<instance part="J7" gate="G$1" x="444.5" y="-55.88"/>
<instance part="R4" gate="G$1" x="414.02" y="-73.66" rot="R270"/>
<instance part="D4" gate="G$1" x="401.32" y="-73.66" rot="R90"/>
<instance part="R3" gate="G$1" x="368.3" y="-73.66" rot="R270"/>
<instance part="D3" gate="G$1" x="355.6" y="-73.66" rot="R90"/>
<instance part="J8" gate="G$1" x="553.72" y="-55.88"/>
<instance part="R36" gate="G$1" x="523.24" y="-71.12" rot="R90"/>
<instance part="Q9" gate="G$1" x="25.4" y="-142.24"/>
<instance part="R5" gate="G$1" x="91.44" y="-142.24"/>
</instances>
<busses>
</busses>
<nets>
<net name="DOCK_DATA_RAW" class="0">
<segment>
<pinref part="J7" gate="G$1" pin="1"/>
<pinref part="R3" gate="G$1" pin="1"/>
<pinref part="D3" gate="G$1" pin="C"/>
<wire x1="439.42" y1="-53.34" x2="368.3" y2="-53.34" width="0.1524" layer="91"/>
<wire x1="368.3" y1="-53.34" x2="368.3" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="368.3" y1="-68.58" x2="355.6" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="355.6" y1="-68.58" x2="353.06" y2="-68.58" width="0.1524" layer="91"/>
<junction x="368.3" y="-68.58"/>
<junction x="355.6" y="-68.58"/>
<label x="353.06" y="-68.58" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_DET" class="0">
<segment>
<pinref part="R4" gate="G$1" pin="2"/>
<wire x1="414.02" y1="-78.74" x2="414.02" y2="-81.28" width="0.1524" layer="91"/>
<label x="414.02" y="-81.28" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R5" gate="G$1" pin="1"/>
<wire x1="86.36" y1="-142.24" x2="81.28" y2="-142.24" width="0.1524" layer="91"/>
<label x="81.28" y="-142.24" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_DET_RAW" class="0">
<segment>
<pinref part="J7" gate="G$1" pin="2"/>
<pinref part="R4" gate="G$1" pin="1"/>
<pinref part="D4" gate="G$1" pin="C"/>
<wire x1="439.42" y1="-55.88" x2="414.02" y2="-55.88" width="0.1524" layer="91"/>
<wire x1="414.02" y1="-55.88" x2="414.02" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="414.02" y1="-68.58" x2="401.32" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="401.32" y1="-68.58" x2="398.78" y2="-68.58" width="0.1524" layer="91"/>
<junction x="414.02" y="-68.58"/>
<junction x="401.32" y="-68.58"/>
<label x="398.78" y="-68.58" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_LED" class="0">
<segment>
<pinref part="J8" gate="G$1" pin="1"/>
<pinref part="R36" gate="G$1" pin="2"/>
<wire x1="548.64" y1="-54.61" x2="523.24" y2="-54.61" width="0.1524" layer="91"/>
<wire x1="523.24" y1="-54.61" x2="523.24" y2="-66.04" width="0.1524" layer="91"/>
<wire x1="523.24" y1="-66.04" x2="520.7" y2="-66.04" width="0.1524" layer="91"/>
<junction x="523.24" y="-66.04"/>
<label x="520.7" y="-66.04" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_RAW" class="0">
<segment>
<pinref part="R35" gate="G$1" pin="1"/>
<wire x1="114.3" y1="-88.9" x2="114.3" y2="-91.44" width="0.1524" layer="91"/>
<label x="114.3" y="-91.44" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C48" gate="G$1" pin="2"/>
<wire x1="91.44" y1="-88.9" x2="91.44" y2="-91.44" width="0.1524" layer="91"/>
<label x="91.44" y="-91.44" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U9" gate="A" pin="ANODE"/>
<pinref part="D2" gate="G$1" pin="C"/>
<wire x1="139.7" y1="-53.34" x2="63.5" y2="-53.34" width="0.1524" layer="91"/>
<wire x1="63.5" y1="-53.34" x2="63.5" y2="-78.74" width="0.1524" layer="91"/>
<wire x1="63.5" y1="-78.74" x2="60.96" y2="-78.74" width="0.1524" layer="91"/>
<junction x="63.5" y="-78.74"/>
<label x="60.96" y="-78.74" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J2" gate="G$1" pin="1"/>
<wire x1="251.46" y1="-54.61" x2="246.38" y2="-54.61" width="0.1524" layer="91"/>
<label x="246.38" y="-54.61" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="Q9" gate="G$1" pin="S"/>
<wire x1="27.94" y1="-147.32" x2="27.94" y2="-152.4" width="0.1524" layer="91"/>
<label x="27.94" y="-152.4" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="DOCK_UART" class="0">
<segment>
<pinref part="R3" gate="G$1" pin="2"/>
<wire x1="368.3" y1="-78.74" x2="368.3" y2="-81.28" width="0.1524" layer="91"/>
<label x="368.3" y="-81.28" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="DOCK_VIN" class="0">
<segment>
<pinref part="U9" gate="A" pin="CATHODE"/>
<wire x1="180.34" y1="-60.96" x2="185.42" y2="-60.96" width="0.1524" layer="91"/>
<label x="185.42" y="-60.96" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="R36" gate="G$1" pin="1"/>
<wire x1="523.24" y1="-76.2" x2="523.24" y2="-78.74" width="0.1524" layer="91"/>
<label x="523.24" y="-78.74" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="Q9" gate="G$1" pin="D"/>
<wire x1="27.94" y1="-137.16" x2="27.94" y2="-132.08" width="0.1524" layer="91"/>
<label x="27.94" y="-132.08" size="1.778" layer="95" rot="R90"/>
</segment>
</net>
<net name="EN_LEMO" class="0">
<segment>
<pinref part="U9" gate="A" pin="EN"/>
<pinref part="R35" gate="G$1" pin="2"/>
<wire x1="139.7" y1="-68.58" x2="114.3" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="114.3" y1="-68.58" x2="114.3" y2="-78.74" width="0.1524" layer="91"/>
<wire x1="114.3" y1="-78.74" x2="111.76" y2="-78.74" width="0.1524" layer="91"/>
<junction x="114.3" y="-78.74"/>
<label x="111.76" y="-78.74" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="GATE_LEMO" class="0">
<segment>
<pinref part="U9" gate="A" pin="GATE"/>
<wire x1="180.34" y1="-53.34" x2="185.42" y2="-53.34" width="0.1524" layer="91"/>
<label x="185.42" y="-53.34" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="Q9" gate="G$1" pin="G"/>
<wire x1="20.32" y1="-142.24" x2="15.24" y2="-142.24" width="0.1524" layer="91"/>
<label x="15.24" y="-142.24" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="D2" gate="G$1" pin="A"/>
<wire x1="63.5" y1="-88.9" x2="63.5" y2="-91.44" width="0.1524" layer="91"/>
<label x="63.5" y="-91.44" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U9" gate="A" pin="GND"/>
<wire x1="180.34" y1="-68.58" x2="185.42" y2="-68.58" width="0.1524" layer="91"/>
<label x="185.42" y="-68.58" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J2" gate="G$1" pin="2"/>
<wire x1="251.46" y1="-57.15" x2="246.38" y2="-57.15" width="0.1524" layer="91"/>
<label x="246.38" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="D4" gate="G$1" pin="A"/>
<wire x1="401.32" y1="-78.74" x2="401.32" y2="-81.28" width="0.1524" layer="91"/>
<label x="401.32" y="-81.28" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="D3" gate="G$1" pin="A"/>
<wire x1="355.6" y1="-78.74" x2="355.6" y2="-81.28" width="0.1524" layer="91"/>
<label x="355.6" y="-81.28" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="J7" gate="G$1" pin="3"/>
<wire x1="439.42" y1="-58.42" x2="434.34" y2="-58.42" width="0.1524" layer="91"/>
<label x="434.34" y="-58.42" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J8" gate="G$1" pin="2"/>
<wire x1="548.64" y1="-57.15" x2="543.56" y2="-57.15" width="0.1524" layer="91"/>
<label x="543.56" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="R5" gate="G$1" pin="2"/>
<wire x1="96.52" y1="-142.24" x2="101.6" y2="-142.24" width="0.1524" layer="91"/>
<label x="101.6" y="-142.24" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="VCAP_LEMO" class="0">
<segment>
<pinref part="U9" gate="A" pin="VCAP"/>
<pinref part="C48" gate="G$1" pin="1"/>
<wire x1="139.7" y1="-60.96" x2="91.44" y2="-60.96" width="0.1524" layer="91"/>
<wire x1="91.44" y1="-60.96" x2="91.44" y2="-78.74" width="0.1524" layer="91"/>
<wire x1="91.44" y1="-78.74" x2="88.9" y2="-78.74" width="0.1524" layer="91"/>
<junction x="91.44" y="-78.74"/>
<label x="88.9" y="-78.74" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
</nets>
</sheet>
<sheet>
<plain>
<text x="0" y="12.7" size="3.81" layer="97">2 Lighting path: ideal diodes, eFuse, INA228</text>
<text x="0" y="5.08" size="2.1844" layer="97">Two LM74700 ideal diodes OR the dock and the internal pack onto LIGHT_BUS. The dock's is always on.</text>
<text x="0" y="1.27" size="2.1844" layer="97">LIGHT_BUS has only small ceramics, so plugging a live dock into a switched-off shark charges ~30 uF through the</text>
<text x="0" y="-2.54" size="2.1844" layer="97">LEMO, not the bulk caps: C32/C33 sit behind the eFuse (EFUSE_OUT), which ramps them up (dVdt), and hold up the</text>
<text x="0" y="-6.35" size="2.1844" layer="97">converter's input while the ideal diodes switch between dock and pack.</text>
<text x="0" y="-10.16" size="2.1844" layer="97">The pack's can be switched off (PACK_OFF) while the dock runs the lights; its FET's body diode still holds the bus</text>
<text x="0" y="-13.97" size="2.1844" layer="97">if the dock is pulled, until the base turns it back on. Defaults to on.</text>
<text x="0" y="-17.78" size="2.1844" layer="97">Pack: off-the-shelf 4S BMS on the cells, the main power switch in its lead, XT30 to J3; F1 protects lights and charging.</text>
<text x="0" y="-21.59" size="2.1844" layer="97">eFuse TPS259827ONRGE: no overvoltage lockout (the pack and dock reach 16.8 V), circuit breaker.</text>
<text x="0" y="-25.4" size="2.1844" layer="97">ILIM 130R = about 11.3 A (datasheet: 300R = 5 A, 182R = 8.1 A, 100R = 14.7 A). UVLO 86.6k/10k: on at 11.6 V, off at 10.6 V.</text>
<text x="0" y="-29.21" size="2.1844" layer="97">LIGHT_OFF from the expander turns the lights off; defaults to on, so the lights work without the base.</text>
<text x="0" y="-33.02" size="2.1844" layer="97">ITIMER, RETRY_DLY and dVdt values are starting points: check against the datasheet for the converter's input capacitance.</text>
<text x="0" y="-36.83" size="2.1844" layer="97">INA228 on the output: 2 mOhm shunt, address 0x40 (A0 = A1 = GND); its ALERT shares PWR_INT.</text>
</plain>
<instances>
<instance part="U3" gate="A" x="121.92" y="-73.66"/>
<instance part="R20" gate="G$1" x="76.2" y="-96.52" rot="R90"/>
<instance part="C30" gate="G$1" x="53.34" y="-96.52" rot="R270"/>
<instance part="C34" gate="G$1" x="167.64" y="-96.52" rot="R270"/>
<instance part="U4" gate="A" x="350.52" y="-73.66"/>
<instance part="R21" gate="G$1" x="304.8" y="-96.52" rot="R90"/>
<instance part="C31" gate="G$1" x="281.94" y="-96.52" rot="R270"/>
<instance part="U5" gate="A" x="701.04" y="-66.04"/>
<instance part="R28" gate="G$1" x="675.64" y="-111.76" rot="R270"/>
<instance part="C36" gate="G$1" x="650.24" y="-111.76" rot="R270"/>
<instance part="R27" gate="G$1" x="619.76" y="-111.76" rot="R270"/>
<instance part="R26" gate="G$1" x="591.82" y="-111.76" rot="R270"/>
<instance part="C35" gate="G$1" x="563.88" y="-111.76" rot="R270"/>
<instance part="R23" gate="G$1" x="530.86" y="-111.76" rot="R90"/>
<instance part="R24" gate="G$1" x="518.16" y="-111.76" rot="R270"/>
<instance part="C37" gate="G$1" x="777.24" y="-111.76" rot="R270"/>
<instance part="C32" gate="G$1" x="805.18" y="-111.76" rot="R270"/>
<instance part="C33" gate="G$1" x="817.88" y="-111.76" rot="R270"/>
<instance part="U6" gate="A" x="916.94" y="-66.04"/>
<instance part="C38" gate="G$1" x="982.98" y="-91.44" rot="R270"/>
<instance part="R29" gate="G$1" x="1003.3" y="-91.44" rot="R90"/>
<instance part="J3" gate="G$1" x="1145.54" y="-66.04"/>
<instance part="F1" gate="G$1" x="1115.06" y="-81.28" rot="R270"/>
<instance part="J5" gate="G$1" x="1216.66" y="-66.04"/>
<instance part="Q5" gate="G$1" x="25.4" y="-170.18"/>
<instance part="Q6" gate="G$1" x="91.44" y="-170.18"/>
<instance part="Q7" gate="G$1" x="157.48" y="-170.18"/>
<instance part="Q8" gate="G$1" x="223.52" y="-170.18"/>
<instance part="R22" gate="G$1" x="289.56" y="-170.18"/>
<instance part="R25" gate="G$1" x="355.6" y="-170.18"/>
</instances>
<busses>
</busses>
<nets>
<net name="+3V3" class="0">
<segment>
<pinref part="R28" gate="G$1" pin="2"/>
<wire x1="675.64" y1="-116.84" x2="675.64" y2="-119.38" width="0.1524" layer="91"/>
<label x="675.64" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="VS"/>
<pinref part="C38" gate="G$1" pin="1"/>
<wire x1="957.58" y1="-76.2" x2="982.98" y2="-76.2" width="0.1524" layer="91"/>
<wire x1="982.98" y1="-76.2" x2="982.98" y2="-86.36" width="0.1524" layer="91"/>
<wire x1="982.98" y1="-86.36" x2="985.52" y2="-86.36" width="0.1524" layer="91"/>
<junction x="982.98" y="-86.36"/>
<label x="985.52" y="-86.36" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="DOCK_VIN" class="0">
<segment>
<pinref part="R20" gate="G$1" pin="1"/>
<wire x1="76.2" y1="-101.6" x2="76.2" y2="-104.14" width="0.1524" layer="91"/>
<label x="76.2" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C30" gate="G$1" pin="2"/>
<wire x1="53.34" y1="-101.6" x2="53.34" y2="-104.14" width="0.1524" layer="91"/>
<label x="53.34" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U3" gate="A" pin="ANODE"/>
<wire x1="101.6" y1="-66.04" x2="96.52" y2="-66.04" width="0.1524" layer="91"/>
<label x="96.52" y="-66.04" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="Q5" gate="G$1" pin="S"/>
<wire x1="27.94" y1="-175.26" x2="27.94" y2="-180.34" width="0.1524" layer="91"/>
<label x="27.94" y="-180.34" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="EFUSE_DVDT" class="0">
<segment>
<pinref part="U5" gate="A" pin="DVDT"/>
<pinref part="C37" gate="G$1" pin="1"/>
<wire x1="751.84" y1="-93.98" x2="777.24" y2="-93.98" width="0.1524" layer="91"/>
<wire x1="777.24" y1="-93.98" x2="777.24" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="777.24" y1="-106.68" x2="779.78" y2="-106.68" width="0.1524" layer="91"/>
<junction x="777.24" y="-106.68"/>
<label x="779.78" y="-106.68" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="EFUSE_EN" class="0">
<segment>
<pinref part="U5" gate="A" pin="EN/UVLO"/>
<pinref part="R23" gate="G$1" pin="2"/>
<pinref part="R24" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-78.74" x2="530.86" y2="-78.74" width="0.1524" layer="91"/>
<wire x1="530.86" y1="-78.74" x2="530.86" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="530.86" y1="-106.68" x2="518.16" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="518.16" y1="-106.68" x2="515.62" y2="-106.68" width="0.1524" layer="91"/>
<junction x="530.86" y="-106.68"/>
<junction x="518.16" y="-106.68"/>
<label x="515.62" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="Q8" gate="G$1" pin="D"/>
<wire x1="226.06" y1="-165.1" x2="226.06" y2="-160.02" width="0.1524" layer="91"/>
<label x="226.06" y="-160.02" size="1.778" layer="95" rot="R90"/>
</segment>
</net>
<net name="EFUSE_ILIM" class="0">
<segment>
<pinref part="U5" gate="A" pin="ILIM"/>
<pinref part="R26" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-83.82" x2="591.82" y2="-83.82" width="0.1524" layer="91"/>
<wire x1="591.82" y1="-83.82" x2="591.82" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="591.82" y1="-106.68" x2="589.28" y2="-106.68" width="0.1524" layer="91"/>
<junction x="591.82" y="-106.68"/>
<label x="589.28" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EFUSE_IMON" class="0">
<segment>
<pinref part="U5" gate="A" pin="IMON"/>
<pinref part="R27" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-86.36" x2="619.76" y2="-86.36" width="0.1524" layer="91"/>
<wire x1="619.76" y1="-86.36" x2="619.76" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="619.76" y1="-106.68" x2="617.22" y2="-106.68" width="0.1524" layer="91"/>
<junction x="619.76" y="-106.68"/>
<label x="617.22" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EFUSE_ITIMER" class="0">
<segment>
<pinref part="U5" gate="A" pin="ITIMER"/>
<pinref part="C35" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-81.28" x2="563.88" y2="-81.28" width="0.1524" layer="91"/>
<wire x1="563.88" y1="-81.28" x2="563.88" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="563.88" y1="-106.68" x2="561.34" y2="-106.68" width="0.1524" layer="91"/>
<junction x="563.88" y="-106.68"/>
<label x="561.34" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EFUSE_OUT" class="0">
<segment>
<pinref part="U5" gate="A" pin="OUT_2"/>
<pinref part="C32" gate="G$1" pin="1"/>
<pinref part="C33" gate="G$1" pin="1"/>
<wire x1="751.84" y1="-88.9" x2="805.18" y2="-88.9" width="0.1524" layer="91"/>
<wire x1="805.18" y1="-88.9" x2="805.18" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="805.18" y1="-106.68" x2="817.88" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="817.88" y1="-106.68" x2="820.42" y2="-106.68" width="0.1524" layer="91"/>
<junction x="805.18" y="-106.68"/>
<junction x="817.88" y="-106.68"/>
<label x="820.42" y="-106.68" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_3"/>
<wire x1="751.84" y1="-86.36" x2="756.92" y2="-86.36" width="0.1524" layer="91"/>
<label x="756.92" y="-86.36" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_4"/>
<wire x1="751.84" y1="-83.82" x2="756.92" y2="-83.82" width="0.1524" layer="91"/>
<label x="756.92" y="-83.82" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_5"/>
<wire x1="751.84" y1="-81.28" x2="756.92" y2="-81.28" width="0.1524" layer="91"/>
<label x="756.92" y="-81.28" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_6"/>
<wire x1="751.84" y1="-78.74" x2="756.92" y2="-78.74" width="0.1524" layer="91"/>
<label x="756.92" y="-78.74" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_7"/>
<wire x1="751.84" y1="-76.2" x2="756.92" y2="-76.2" width="0.1524" layer="91"/>
<label x="756.92" y="-76.2" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT_8"/>
<wire x1="751.84" y1="-73.66" x2="756.92" y2="-73.66" width="0.1524" layer="91"/>
<label x="756.92" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="OUT"/>
<wire x1="751.84" y1="-71.12" x2="756.92" y2="-71.12" width="0.1524" layer="91"/>
<label x="756.92" y="-71.12" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="R29" gate="G$1" pin="1"/>
<wire x1="1003.3" y1="-96.52" x2="1003.3" y2="-99.06" width="0.1524" layer="91"/>
<label x="1003.3" y="-99.06" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="IN+"/>
<wire x1="957.58" y1="-66.04" x2="962.66" y2="-66.04" width="0.1524" layer="91"/>
<label x="962.66" y="-66.04" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="EFUSE_PG" class="0">
<segment>
<pinref part="U5" gate="A" pin="PG"/>
<pinref part="R28" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-96.52" x2="675.64" y2="-96.52" width="0.1524" layer="91"/>
<wire x1="675.64" y1="-96.52" x2="675.64" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="675.64" y1="-106.68" x2="673.1" y2="-106.68" width="0.1524" layer="91"/>
<junction x="675.64" y="-106.68"/>
<label x="673.1" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EFUSE_RETRY" class="0">
<segment>
<pinref part="U5" gate="A" pin="RETRY_DLY"/>
<pinref part="C36" gate="G$1" pin="1"/>
<wire x1="701.04" y1="-88.9" x2="650.24" y2="-88.9" width="0.1524" layer="91"/>
<wire x1="650.24" y1="-88.9" x2="650.24" y2="-106.68" width="0.1524" layer="91"/>
<wire x1="650.24" y1="-106.68" x2="647.7" y2="-106.68" width="0.1524" layer="91"/>
<junction x="650.24" y="-106.68"/>
<label x="647.7" y="-106.68" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EN_DOCK" class="0">
<segment>
<pinref part="U3" gate="A" pin="EN"/>
<pinref part="R20" gate="G$1" pin="2"/>
<wire x1="101.6" y1="-81.28" x2="76.2" y2="-81.28" width="0.1524" layer="91"/>
<wire x1="76.2" y1="-81.28" x2="76.2" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="76.2" y1="-91.44" x2="73.66" y2="-91.44" width="0.1524" layer="91"/>
<junction x="76.2" y="-91.44"/>
<label x="73.66" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EN_PACK" class="0">
<segment>
<pinref part="U4" gate="A" pin="EN"/>
<pinref part="R21" gate="G$1" pin="2"/>
<wire x1="330.2" y1="-81.28" x2="304.8" y2="-81.28" width="0.1524" layer="91"/>
<wire x1="304.8" y1="-81.28" x2="304.8" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="304.8" y1="-91.44" x2="302.26" y2="-91.44" width="0.1524" layer="91"/>
<junction x="304.8" y="-91.44"/>
<label x="302.26" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="Q7" gate="G$1" pin="D"/>
<wire x1="160.02" y1="-165.1" x2="160.02" y2="-160.02" width="0.1524" layer="91"/>
<label x="160.02" y="-160.02" size="1.778" layer="95" rot="R90"/>
</segment>
</net>
<net name="GATE_DOCK" class="0">
<segment>
<pinref part="U3" gate="A" pin="GATE"/>
<wire x1="142.24" y1="-66.04" x2="147.32" y2="-66.04" width="0.1524" layer="91"/>
<label x="147.32" y="-66.04" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="Q5" gate="G$1" pin="G"/>
<wire x1="20.32" y1="-170.18" x2="15.24" y2="-170.18" width="0.1524" layer="91"/>
<label x="15.24" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="GATE_PACK" class="0">
<segment>
<pinref part="U4" gate="A" pin="GATE"/>
<wire x1="370.84" y1="-66.04" x2="375.92" y2="-66.04" width="0.1524" layer="91"/>
<label x="375.92" y="-66.04" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="Q6" gate="G$1" pin="G"/>
<wire x1="86.36" y1="-170.18" x2="81.28" y2="-170.18" width="0.1524" layer="91"/>
<label x="81.28" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="C34" gate="G$1" pin="2"/>
<wire x1="167.64" y1="-101.6" x2="167.64" y2="-104.14" width="0.1524" layer="91"/>
<label x="167.64" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U3" gate="A" pin="GND"/>
<wire x1="142.24" y1="-81.28" x2="147.32" y2="-81.28" width="0.1524" layer="91"/>
<label x="147.32" y="-81.28" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U4" gate="A" pin="GND"/>
<wire x1="370.84" y1="-81.28" x2="375.92" y2="-81.28" width="0.1524" layer="91"/>
<label x="375.92" y="-81.28" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="C36" gate="G$1" pin="2"/>
<wire x1="650.24" y1="-116.84" x2="650.24" y2="-119.38" width="0.1524" layer="91"/>
<label x="650.24" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R27" gate="G$1" pin="2"/>
<wire x1="619.76" y1="-116.84" x2="619.76" y2="-119.38" width="0.1524" layer="91"/>
<label x="619.76" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R26" gate="G$1" pin="2"/>
<wire x1="591.82" y1="-116.84" x2="591.82" y2="-119.38" width="0.1524" layer="91"/>
<label x="591.82" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C35" gate="G$1" pin="2"/>
<wire x1="563.88" y1="-116.84" x2="563.88" y2="-119.38" width="0.1524" layer="91"/>
<label x="563.88" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R24" gate="G$1" pin="2"/>
<wire x1="518.16" y1="-116.84" x2="518.16" y2="-119.38" width="0.1524" layer="91"/>
<label x="518.16" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C37" gate="G$1" pin="2"/>
<wire x1="777.24" y1="-116.84" x2="777.24" y2="-119.38" width="0.1524" layer="91"/>
<label x="777.24" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C32" gate="G$1" pin="2"/>
<wire x1="805.18" y1="-116.84" x2="805.18" y2="-119.38" width="0.1524" layer="91"/>
<label x="805.18" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C33" gate="G$1" pin="2"/>
<wire x1="817.88" y1="-116.84" x2="817.88" y2="-119.38" width="0.1524" layer="91"/>
<label x="817.88" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="GND_2"/>
<wire x1="701.04" y1="-73.66" x2="695.96" y2="-73.66" width="0.1524" layer="91"/>
<label x="695.96" y="-73.66" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="GND_3"/>
<wire x1="701.04" y1="-76.2" x2="695.96" y2="-76.2" width="0.1524" layer="91"/>
<label x="695.96" y="-76.2" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="NRETRY"/>
<wire x1="701.04" y1="-91.44" x2="695.96" y2="-91.44" width="0.1524" layer="91"/>
<label x="695.96" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="LDSTRT"/>
<wire x1="701.04" y1="-93.98" x2="695.96" y2="-93.98" width="0.1524" layer="91"/>
<label x="695.96" y="-93.98" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="GND"/>
<wire x1="751.84" y1="-96.52" x2="756.92" y2="-96.52" width="0.1524" layer="91"/>
<label x="756.92" y="-96.52" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="EPAD"/>
<wire x1="751.84" y1="-66.04" x2="756.92" y2="-66.04" width="0.1524" layer="91"/>
<label x="756.92" y="-66.04" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="C38" gate="G$1" pin="2"/>
<wire x1="982.98" y1="-96.52" x2="982.98" y2="-99.06" width="0.1524" layer="91"/>
<label x="982.98" y="-99.06" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="A1"/>
<wire x1="916.94" y1="-66.04" x2="911.86" y2="-66.04" width="0.1524" layer="91"/>
<label x="911.86" y="-66.04" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="A0"/>
<wire x1="916.94" y1="-68.58" x2="911.86" y2="-68.58" width="0.1524" layer="91"/>
<label x="911.86" y="-68.58" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="GND"/>
<wire x1="957.58" y1="-73.66" x2="962.66" y2="-73.66" width="0.1524" layer="91"/>
<label x="962.66" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J3" gate="G$1" pin="2"/>
<wire x1="1140.46" y1="-67.31" x2="1135.38" y2="-67.31" width="0.1524" layer="91"/>
<label x="1135.38" y="-67.31" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J5" gate="G$1" pin="2"/>
<wire x1="1211.58" y1="-67.31" x2="1206.5" y2="-67.31" width="0.1524" layer="91"/>
<label x="1206.5" y="-67.31" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="Q7" gate="G$1" pin="S"/>
<wire x1="160.02" y1="-175.26" x2="160.02" y2="-180.34" width="0.1524" layer="91"/>
<label x="160.02" y="-180.34" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="Q8" gate="G$1" pin="S"/>
<wire x1="226.06" y1="-175.26" x2="226.06" y2="-180.34" width="0.1524" layer="91"/>
<label x="226.06" y="-180.34" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R22" gate="G$1" pin="2"/>
<wire x1="294.64" y1="-170.18" x2="299.72" y2="-170.18" width="0.1524" layer="91"/>
<label x="299.72" y="-170.18" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="R25" gate="G$1" pin="2"/>
<wire x1="360.68" y1="-170.18" x2="365.76" y2="-170.18" width="0.1524" layer="91"/>
<label x="365.76" y="-170.18" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="LIGHT_BUS" class="0">
<segment>
<pinref part="U3" gate="A" pin="CATHODE"/>
<pinref part="C34" gate="G$1" pin="1"/>
<wire x1="142.24" y1="-73.66" x2="167.64" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="167.64" y1="-73.66" x2="167.64" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="167.64" y1="-91.44" x2="170.18" y2="-91.44" width="0.1524" layer="91"/>
<junction x="167.64" y="-91.44"/>
<label x="170.18" y="-91.44" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U4" gate="A" pin="CATHODE"/>
<wire x1="370.84" y1="-73.66" x2="375.92" y2="-73.66" width="0.1524" layer="91"/>
<label x="375.92" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="R23" gate="G$1" pin="1"/>
<wire x1="530.86" y1="-116.84" x2="530.86" y2="-119.38" width="0.1524" layer="91"/>
<label x="530.86" y="-119.38" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="IN_2"/>
<wire x1="701.04" y1="-66.04" x2="695.96" y2="-66.04" width="0.1524" layer="91"/>
<label x="695.96" y="-66.04" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="IN_3"/>
<wire x1="701.04" y1="-68.58" x2="695.96" y2="-68.58" width="0.1524" layer="91"/>
<label x="695.96" y="-68.58" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="IN_4"/>
<wire x1="701.04" y1="-71.12" x2="695.96" y2="-71.12" width="0.1524" layer="91"/>
<label x="695.96" y="-71.12" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="IN"/>
<wire x1="751.84" y1="-91.44" x2="756.92" y2="-91.44" width="0.1524" layer="91"/>
<label x="756.92" y="-91.44" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U5" gate="A" pin="EPAD_2"/>
<wire x1="751.84" y1="-68.58" x2="756.92" y2="-68.58" width="0.1524" layer="91"/>
<label x="756.92" y="-68.58" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="Q5" gate="G$1" pin="D"/>
<wire x1="27.94" y1="-165.1" x2="27.94" y2="-160.02" width="0.1524" layer="91"/>
<label x="27.94" y="-160.02" size="1.778" layer="95" rot="R90"/>
</segment>
<segment>
<pinref part="Q6" gate="G$1" pin="D"/>
<wire x1="93.98" y1="-165.1" x2="93.98" y2="-160.02" width="0.1524" layer="91"/>
<label x="93.98" y="-160.02" size="1.778" layer="95" rot="R90"/>
</segment>
</net>
<net name="LIGHT_OFF" class="0">
<segment>
<pinref part="Q8" gate="G$1" pin="G"/>
<wire x1="218.44" y1="-170.18" x2="213.36" y2="-170.18" width="0.1524" layer="91"/>
<label x="213.36" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="R25" gate="G$1" pin="1"/>
<wire x1="350.52" y1="-170.18" x2="345.44" y2="-170.18" width="0.1524" layer="91"/>
<label x="345.44" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="LIGHT_OUT" class="0">
<segment>
<pinref part="U6" gate="A" pin="VBUS"/>
<pinref part="R29" gate="G$1" pin="2"/>
<wire x1="957.58" y1="-71.12" x2="1003.3" y2="-71.12" width="0.1524" layer="91"/>
<wire x1="1003.3" y1="-71.12" x2="1003.3" y2="-86.36" width="0.1524" layer="91"/>
<wire x1="1003.3" y1="-86.36" x2="1005.84" y2="-86.36" width="0.1524" layer="91"/>
<junction x="1003.3" y="-86.36"/>
<label x="1005.84" y="-86.36" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="U6" gate="A" pin="IN-"/>
<wire x1="957.58" y1="-68.58" x2="962.66" y2="-68.58" width="0.1524" layer="91"/>
<label x="962.66" y="-68.58" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J5" gate="G$1" pin="1"/>
<wire x1="1211.58" y1="-64.77" x2="1206.5" y2="-64.77" width="0.1524" layer="91"/>
<label x="1206.5" y="-64.77" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PACK_F" class="0">
<segment>
<pinref part="R21" gate="G$1" pin="1"/>
<wire x1="304.8" y1="-101.6" x2="304.8" y2="-104.14" width="0.1524" layer="91"/>
<label x="304.8" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C31" gate="G$1" pin="2"/>
<wire x1="281.94" y1="-101.6" x2="281.94" y2="-104.14" width="0.1524" layer="91"/>
<label x="281.94" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U4" gate="A" pin="ANODE"/>
<wire x1="330.2" y1="-66.04" x2="325.12" y2="-66.04" width="0.1524" layer="91"/>
<label x="325.12" y="-66.04" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="F1" gate="G$1" pin="2"/>
<wire x1="1115.06" y1="-86.36" x2="1115.06" y2="-88.9" width="0.1524" layer="91"/>
<label x="1115.06" y="-88.9" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="Q6" gate="G$1" pin="S"/>
<wire x1="93.98" y1="-175.26" x2="93.98" y2="-180.34" width="0.1524" layer="91"/>
<label x="93.98" y="-180.34" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="PACK_OFF" class="0">
<segment>
<pinref part="Q7" gate="G$1" pin="G"/>
<wire x1="152.4" y1="-170.18" x2="147.32" y2="-170.18" width="0.1524" layer="91"/>
<label x="147.32" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="R22" gate="G$1" pin="1"/>
<wire x1="284.48" y1="-170.18" x2="279.4" y2="-170.18" width="0.1524" layer="91"/>
<label x="279.4" y="-170.18" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PACK_RAW" class="0">
<segment>
<pinref part="J3" gate="G$1" pin="1"/>
<pinref part="F1" gate="G$1" pin="1"/>
<wire x1="1140.46" y1="-64.77" x2="1115.06" y2="-64.77" width="0.1524" layer="91"/>
<wire x1="1115.06" y1="-64.77" x2="1115.06" y2="-76.2" width="0.1524" layer="91"/>
<wire x1="1115.06" y1="-76.2" x2="1112.52" y2="-76.2" width="0.1524" layer="91"/>
<junction x="1115.06" y="-76.2"/>
<label x="1112.52" y="-76.2" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_INT" class="0">
<segment>
<pinref part="U6" gate="A" pin="ALERT"/>
<wire x1="916.94" y1="-71.12" x2="911.86" y2="-71.12" width="0.1524" layer="91"/>
<label x="911.86" y="-71.12" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SCL" class="0">
<segment>
<pinref part="U6" gate="A" pin="SCL"/>
<wire x1="916.94" y1="-76.2" x2="911.86" y2="-76.2" width="0.1524" layer="91"/>
<label x="911.86" y="-76.2" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SDA" class="0">
<segment>
<pinref part="U6" gate="A" pin="SDA"/>
<wire x1="916.94" y1="-73.66" x2="911.86" y2="-73.66" width="0.1524" layer="91"/>
<label x="911.86" y="-73.66" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="VCAP_DOCK" class="0">
<segment>
<pinref part="U3" gate="A" pin="VCAP"/>
<pinref part="C30" gate="G$1" pin="1"/>
<wire x1="101.6" y1="-73.66" x2="53.34" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="53.34" y1="-73.66" x2="53.34" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="53.34" y1="-91.44" x2="50.8" y2="-91.44" width="0.1524" layer="91"/>
<junction x="53.34" y="-91.44"/>
<label x="50.8" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="VCAP_PACK" class="0">
<segment>
<pinref part="U4" gate="A" pin="VCAP"/>
<pinref part="C31" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-73.66" x2="281.94" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="281.94" y1="-73.66" x2="281.94" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="281.94" y1="-91.44" x2="279.4" y2="-91.44" width="0.1524" layer="91"/>
<junction x="281.94" y="-91.44"/>
<label x="279.4" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
</nets>
</sheet>
<sheet>
<plain>
<text x="0" y="12.7" size="3.81" layer="97">3 Logic: 5 V for the base, I/O expander, ports</text>
<text x="0" y="5.08" size="2.1844" layer="97">TPS62933 from LIGHT_BUS (dock or pack, 11-16.8 V; not the charger, so the base runs without a charger board):</text>
<text x="0" y="1.27" size="2.1844" layer="97">5 V, 3 A max, 500 kHz (RT open), FB 52.3k/10k = 4.98 V. L2 6.8 uH is a starting point (datasheet's inductor table).</text>
<text x="0" y="-2.54" size="2.1844" layer="97">TCA9534 at 0x20 (A0-A2 = GND): P0 LIGHT_OFF, P1 PACK_OFF (outputs); P3 EFUSE_PG (input); P2, P4-P7 spare.</text>
<text x="0" y="-6.35" size="2.1844" layer="97">Power I2C (BQ25798 0x6B, INA228 0x40, TCA9534 0x20, keypad 0x21-0x27) on its own bus from the Waveshare board;</text>
<text x="0" y="-10.16" size="2.1844" layer="97">pull-ups here. J6 to the Waveshare ESP32-S3-Touch-LCD-3.49 V2: 1 +5V out, 2 GND, 3 +3V3 in (pull-ups and logic),</text>
<text x="0" y="-13.97" size="2.1844" layer="97">4 SDA, 5 SCL, 6 INT (open-drain, shared by everything on the bus), 7 dock UART, 8 dock detect.</text>
<text x="0" y="-17.78" size="2.1844" layer="97">Charger port: J10 (XT30, DOCK_VIN out to the charger), J11 (XT30, the charger's battery output into PACK_F),</text>
<text x="0" y="-21.59" size="2.1844" layer="97">J12 (JST-GH 6: 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V). Plan B: last year's 60 W + 140 W USB-C modules on J10/J11.</text>
<text x="0" y="-25.4" size="2.1844" layer="97">Keypad port: J9, the same JST-GH 6 pinout (the keypad's expander shares INT). Leave the Waveshare battery header empty.</text>
</plain>
<instances>
<instance part="U7" gate="A" x="91.44" y="-55.88"/>
<instance part="C40" gate="G$1" x="66.04" y="-78.74" rot="R270"/>
<instance part="C41" gate="G$1" x="53.34" y="-78.74" rot="R270"/>
<instance part="C42" gate="G$1" x="40.64" y="-78.74" rot="R270"/>
<instance part="L2" gate="G$1" x="157.48" y="-78.74" rot="R270"/>
<instance part="C43" gate="G$1" x="180.34" y="-78.74" rot="R270"/>
<instance part="C46" gate="G$1" x="205.74" y="-78.74" rot="R270"/>
<instance part="R30" gate="G$1" x="228.6" y="-78.74" rot="R90"/>
<instance part="R31" gate="G$1" x="241.3" y="-78.74" rot="R270"/>
<instance part="U8" gate="A" x="500.38" y="-55.88"/>
<instance part="R32" gate="G$1" x="474.98" y="-96.52" rot="R270"/>
<instance part="R33" gate="G$1" x="452.12" y="-96.52" rot="R270"/>
<instance part="R16" gate="G$1" x="429.26" y="-96.52" rot="R270"/>
<instance part="C47" gate="G$1" x="406.4" y="-96.52" rot="R270"/>
<instance part="J6" gate="G$1" x="655.32" y="-63.5"/>
<instance part="C44" gate="G$1" x="624.84" y="-86.36" rot="R270"/>
<instance part="C45" gate="G$1" x="612.14" y="-86.36" rot="R270"/>
<instance part="J10" gate="G$1" x="726.44" y="-55.88"/>
<instance part="J11" gate="G$1" x="797.56" y="-55.88"/>
<instance part="J12" gate="G$1" x="868.68" y="-60.96"/>
<instance part="J9" gate="G$1" x="939.8" y="-60.96"/>
</instances>
<busses>
</busses>
<nets>
<net name="+3V3" class="0">
<segment>
<pinref part="R32" gate="G$1" pin="2"/>
<wire x1="474.98" y1="-101.6" x2="474.98" y2="-104.14" width="0.1524" layer="91"/>
<label x="474.98" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R33" gate="G$1" pin="2"/>
<wire x1="452.12" y1="-101.6" x2="452.12" y2="-104.14" width="0.1524" layer="91"/>
<label x="452.12" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R16" gate="G$1" pin="2"/>
<wire x1="429.26" y1="-101.6" x2="429.26" y2="-104.14" width="0.1524" layer="91"/>
<label x="429.26" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U8" gate="A" pin="VCC"/>
<pinref part="C47" gate="G$1" pin="1"/>
<wire x1="500.38" y1="-55.88" x2="406.4" y2="-55.88" width="0.1524" layer="91"/>
<wire x1="406.4" y1="-55.88" x2="406.4" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="406.4" y1="-91.44" x2="403.86" y2="-91.44" width="0.1524" layer="91"/>
<junction x="406.4" y="-91.44"/>
<label x="403.86" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="3"/>
<wire x1="650.24" y1="-59.69" x2="645.16" y2="-59.69" width="0.1524" layer="91"/>
<label x="645.16" y="-59.69" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="2"/>
<wire x1="863.6" y1="-57.15" x2="858.52" y2="-57.15" width="0.1524" layer="91"/>
<label x="858.52" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="2"/>
<wire x1="934.72" y1="-57.15" x2="929.64" y2="-57.15" width="0.1524" layer="91"/>
<label x="929.64" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="+5V" class="0">
<segment>
<pinref part="L2" gate="G$1" pin="2"/>
<wire x1="157.48" y1="-83.82" x2="157.48" y2="-86.36" width="0.1524" layer="91"/>
<label x="157.48" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R30" gate="G$1" pin="1"/>
<wire x1="228.6" y1="-83.82" x2="228.6" y2="-86.36" width="0.1524" layer="91"/>
<label x="228.6" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="1"/>
<pinref part="C44" gate="G$1" pin="1"/>
<pinref part="C45" gate="G$1" pin="1"/>
<wire x1="650.24" y1="-54.61" x2="624.84" y2="-54.61" width="0.1524" layer="91"/>
<wire x1="624.84" y1="-54.61" x2="624.84" y2="-81.28" width="0.1524" layer="91"/>
<wire x1="624.84" y1="-81.28" x2="612.14" y2="-81.28" width="0.1524" layer="91"/>
<wire x1="612.14" y1="-81.28" x2="609.6" y2="-81.28" width="0.1524" layer="91"/>
<junction x="624.84" y="-81.28"/>
<junction x="612.14" y="-81.28"/>
<label x="609.6" y="-81.28" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="6"/>
<wire x1="863.6" y1="-67.31" x2="858.52" y2="-67.31" width="0.1524" layer="91"/>
<label x="858.52" y="-67.31" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="6"/>
<wire x1="934.72" y1="-67.31" x2="929.64" y2="-67.31" width="0.1524" layer="91"/>
<label x="929.64" y="-67.31" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="BUCK_BST" class="0">
<segment>
<pinref part="U7" gate="A" pin="BST"/>
<pinref part="C43" gate="G$1" pin="1"/>
<wire x1="132.08" y1="-60.96" x2="180.34" y2="-60.96" width="0.1524" layer="91"/>
<wire x1="180.34" y1="-60.96" x2="180.34" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="180.34" y1="-73.66" x2="182.88" y2="-73.66" width="0.1524" layer="91"/>
<junction x="180.34" y="-73.66"/>
<label x="182.88" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="BUCK_FB" class="0">
<segment>
<pinref part="U7" gate="A" pin="FB"/>
<pinref part="R30" gate="G$1" pin="2"/>
<pinref part="R31" gate="G$1" pin="1"/>
<wire x1="132.08" y1="-55.88" x2="228.6" y2="-55.88" width="0.1524" layer="91"/>
<wire x1="228.6" y1="-55.88" x2="228.6" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="228.6" y1="-73.66" x2="241.3" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="241.3" y1="-73.66" x2="243.84" y2="-73.66" width="0.1524" layer="91"/>
<junction x="228.6" y="-73.66"/>
<junction x="241.3" y="-73.66"/>
<label x="243.84" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="BUCK_SS" class="0">
<segment>
<pinref part="U7" gate="A" pin="SS"/>
<pinref part="C46" gate="G$1" pin="1"/>
<wire x1="132.08" y1="-58.42" x2="205.74" y2="-58.42" width="0.1524" layer="91"/>
<wire x1="205.74" y1="-58.42" x2="205.74" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="205.74" y1="-73.66" x2="208.28" y2="-73.66" width="0.1524" layer="91"/>
<junction x="205.74" y="-73.66"/>
<label x="208.28" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="BUCK_SW" class="0">
<segment>
<pinref part="U7" gate="A" pin="SW"/>
<pinref part="L2" gate="G$1" pin="1"/>
<wire x1="132.08" y1="-63.5" x2="157.48" y2="-63.5" width="0.1524" layer="91"/>
<wire x1="157.48" y1="-63.5" x2="157.48" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="157.48" y1="-73.66" x2="160.02" y2="-73.66" width="0.1524" layer="91"/>
<junction x="157.48" y="-73.66"/>
<label x="160.02" y="-73.66" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="C43" gate="G$1" pin="2"/>
<wire x1="180.34" y1="-83.82" x2="180.34" y2="-86.36" width="0.1524" layer="91"/>
<label x="180.34" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="DOCK_DET" class="0">
<segment>
<pinref part="J6" gate="G$1" pin="8"/>
<wire x1="650.24" y1="-72.39" x2="645.16" y2="-72.39" width="0.1524" layer="91"/>
<label x="645.16" y="-72.39" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_UART" class="0">
<segment>
<pinref part="J6" gate="G$1" pin="7"/>
<wire x1="650.24" y1="-69.85" x2="645.16" y2="-69.85" width="0.1524" layer="91"/>
<label x="645.16" y="-69.85" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_VIN" class="0">
<segment>
<pinref part="J10" gate="G$1" pin="1"/>
<wire x1="721.36" y1="-54.61" x2="716.28" y2="-54.61" width="0.1524" layer="91"/>
<label x="716.28" y="-54.61" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="EFUSE_PG" class="0">
<segment>
<pinref part="U8" gate="A" pin="P3"/>
<wire x1="535.94" y1="-63.5" x2="541.02" y2="-63.5" width="0.1524" layer="91"/>
<label x="541.02" y="-63.5" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="C40" gate="G$1" pin="2"/>
<wire x1="66.04" y1="-83.82" x2="66.04" y2="-86.36" width="0.1524" layer="91"/>
<label x="66.04" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C41" gate="G$1" pin="2"/>
<wire x1="53.34" y1="-83.82" x2="53.34" y2="-86.36" width="0.1524" layer="91"/>
<label x="53.34" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C42" gate="G$1" pin="2"/>
<wire x1="40.64" y1="-83.82" x2="40.64" y2="-86.36" width="0.1524" layer="91"/>
<label x="40.64" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C46" gate="G$1" pin="2"/>
<wire x1="205.74" y1="-83.82" x2="205.74" y2="-86.36" width="0.1524" layer="91"/>
<label x="205.74" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R31" gate="G$1" pin="2"/>
<wire x1="241.3" y1="-83.82" x2="241.3" y2="-86.36" width="0.1524" layer="91"/>
<label x="241.3" y="-86.36" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U7" gate="A" pin="GND"/>
<wire x1="91.44" y1="-63.5" x2="86.36" y2="-63.5" width="0.1524" layer="91"/>
<label x="86.36" y="-63.5" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="C47" gate="G$1" pin="2"/>
<wire x1="406.4" y1="-101.6" x2="406.4" y2="-104.14" width="0.1524" layer="91"/>
<label x="406.4" y="-104.14" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U8" gate="A" pin="A0"/>
<wire x1="500.38" y1="-76.2" x2="495.3" y2="-76.2" width="0.1524" layer="91"/>
<label x="495.3" y="-76.2" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U8" gate="A" pin="A1"/>
<wire x1="500.38" y1="-78.74" x2="495.3" y2="-78.74" width="0.1524" layer="91"/>
<label x="495.3" y="-78.74" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U8" gate="A" pin="A2"/>
<wire x1="500.38" y1="-81.28" x2="495.3" y2="-81.28" width="0.1524" layer="91"/>
<label x="495.3" y="-81.28" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U8" gate="A" pin="GND"/>
<wire x1="535.94" y1="-81.28" x2="541.02" y2="-81.28" width="0.1524" layer="91"/>
<label x="541.02" y="-81.28" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="C44" gate="G$1" pin="2"/>
<wire x1="624.84" y1="-91.44" x2="624.84" y2="-93.98" width="0.1524" layer="91"/>
<label x="624.84" y="-93.98" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C45" gate="G$1" pin="2"/>
<wire x1="612.14" y1="-91.44" x2="612.14" y2="-93.98" width="0.1524" layer="91"/>
<label x="612.14" y="-93.98" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="2"/>
<wire x1="650.24" y1="-57.15" x2="645.16" y2="-57.15" width="0.1524" layer="91"/>
<label x="645.16" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J10" gate="G$1" pin="2"/>
<wire x1="721.36" y1="-57.15" x2="716.28" y2="-57.15" width="0.1524" layer="91"/>
<label x="716.28" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J11" gate="G$1" pin="2"/>
<wire x1="792.48" y1="-57.15" x2="787.4" y2="-57.15" width="0.1524" layer="91"/>
<label x="787.4" y="-57.15" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="1"/>
<wire x1="863.6" y1="-54.61" x2="858.52" y2="-54.61" width="0.1524" layer="91"/>
<label x="858.52" y="-54.61" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="1"/>
<wire x1="934.72" y1="-54.61" x2="929.64" y2="-54.61" width="0.1524" layer="91"/>
<label x="929.64" y="-54.61" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="LIGHT_BUS" class="0">
<segment>
<pinref part="U7" gate="A" pin="VIN"/>
<pinref part="C40" gate="G$1" pin="1"/>
<pinref part="C41" gate="G$1" pin="1"/>
<pinref part="C42" gate="G$1" pin="1"/>
<wire x1="91.44" y1="-60.96" x2="66.04" y2="-60.96" width="0.1524" layer="91"/>
<wire x1="66.04" y1="-60.96" x2="66.04" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="66.04" y1="-73.66" x2="53.34" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="53.34" y1="-73.66" x2="40.64" y2="-73.66" width="0.1524" layer="91"/>
<wire x1="40.64" y1="-73.66" x2="38.1" y2="-73.66" width="0.1524" layer="91"/>
<junction x="66.04" y="-73.66"/>
<junction x="53.34" y="-73.66"/>
<junction x="40.64" y="-73.66"/>
<label x="38.1" y="-73.66" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="LIGHT_OFF" class="0">
<segment>
<pinref part="U8" gate="A" pin="P0"/>
<wire x1="535.94" y1="-55.88" x2="541.02" y2="-55.88" width="0.1524" layer="91"/>
<label x="541.02" y="-55.88" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="PACK_F" class="0">
<segment>
<pinref part="J11" gate="G$1" pin="1"/>
<wire x1="792.48" y1="-54.61" x2="787.4" y2="-54.61" width="0.1524" layer="91"/>
<label x="787.4" y="-54.61" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PACK_OFF" class="0">
<segment>
<pinref part="U8" gate="A" pin="P1"/>
<wire x1="535.94" y1="-58.42" x2="541.02" y2="-58.42" width="0.1524" layer="91"/>
<label x="541.02" y="-58.42" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="PWR_INT" class="0">
<segment>
<pinref part="U8" gate="A" pin="INT_N"/>
<pinref part="R16" gate="G$1" pin="1"/>
<wire x1="500.38" y1="-60.96" x2="429.26" y2="-60.96" width="0.1524" layer="91"/>
<wire x1="429.26" y1="-60.96" x2="429.26" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="429.26" y1="-91.44" x2="426.72" y2="-91.44" width="0.1524" layer="91"/>
<junction x="429.26" y="-91.44"/>
<label x="426.72" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="6"/>
<wire x1="650.24" y1="-67.31" x2="645.16" y2="-67.31" width="0.1524" layer="91"/>
<label x="645.16" y="-67.31" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="5"/>
<wire x1="863.6" y1="-64.77" x2="858.52" y2="-64.77" width="0.1524" layer="91"/>
<label x="858.52" y="-64.77" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="5"/>
<wire x1="934.72" y1="-64.77" x2="929.64" y2="-64.77" width="0.1524" layer="91"/>
<label x="929.64" y="-64.77" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SCL" class="0">
<segment>
<pinref part="U8" gate="A" pin="SCL"/>
<pinref part="R33" gate="G$1" pin="1"/>
<wire x1="500.38" y1="-66.04" x2="452.12" y2="-66.04" width="0.1524" layer="91"/>
<wire x1="452.12" y1="-66.04" x2="452.12" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="452.12" y1="-91.44" x2="449.58" y2="-91.44" width="0.1524" layer="91"/>
<junction x="452.12" y="-91.44"/>
<label x="449.58" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="5"/>
<wire x1="650.24" y1="-64.77" x2="645.16" y2="-64.77" width="0.1524" layer="91"/>
<label x="645.16" y="-64.77" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="4"/>
<wire x1="863.6" y1="-62.23" x2="858.52" y2="-62.23" width="0.1524" layer="91"/>
<label x="858.52" y="-62.23" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="4"/>
<wire x1="934.72" y1="-62.23" x2="929.64" y2="-62.23" width="0.1524" layer="91"/>
<label x="929.64" y="-62.23" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SDA" class="0">
<segment>
<pinref part="U8" gate="A" pin="SDA"/>
<pinref part="R32" gate="G$1" pin="1"/>
<wire x1="500.38" y1="-71.12" x2="474.98" y2="-71.12" width="0.1524" layer="91"/>
<wire x1="474.98" y1="-71.12" x2="474.98" y2="-91.44" width="0.1524" layer="91"/>
<wire x1="474.98" y1="-91.44" x2="472.44" y2="-91.44" width="0.1524" layer="91"/>
<junction x="474.98" y="-91.44"/>
<label x="472.44" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J6" gate="G$1" pin="4"/>
<wire x1="650.24" y1="-62.23" x2="645.16" y2="-62.23" width="0.1524" layer="91"/>
<label x="645.16" y="-62.23" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J12" gate="G$1" pin="3"/>
<wire x1="863.6" y1="-59.69" x2="858.52" y2="-59.69" width="0.1524" layer="91"/>
<label x="858.52" y="-59.69" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J9" gate="G$1" pin="3"/>
<wire x1="934.72" y1="-59.69" x2="929.64" y2="-59.69" width="0.1524" layer="91"/>
<label x="929.64" y="-59.69" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>
