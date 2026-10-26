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
<package name="RQM0029A-MFG">
<smd name="1" x="-1.3687" y="1.6312" dx="0.1916" dy="0.1916" layer="1" roundness="100" rot="R0"/>
<smd name="2" x="-1.9" y="1.2" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="3" x="-1.9" y="0.8" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="4" x="-1.9" y="0.4" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="5" x="-1.9" y="0" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="6" x="-1.9" y="-0.4" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="7" x="-1.9" y="-0.8" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="8" x="-1.9" y="-1.2" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="9" x="-1.4125" y="-1.7625" dx="0.1575" dy="0.1575" layer="1" roundness="100" rot="R0"/>
<smd name="10" x="-1" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="11" x="-0.6" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="12" x="-0.2" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="13" x="0.2" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="14" x="0.6" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="15" x="1" y="-1.9" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R90"/>
<smd name="16" x="1.4312" y="-1.6312" dx="0.175" dy="0.175" layer="1" roundness="100" rot="R0"/>
<smd name="17" x="1.85" y="-1.2" dx="0.7" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="18" x="1.875" y="-0.8" dx="0.65" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="19" x="1.86" y="-0.4" dx="0.68" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="20" x="1.9" y="0" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="21" x="1.9" y="0.4" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="22" x="1.9" y="0.8" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="23" x="1.9" y="1.2" dx="0.6" dy="0.2" layer="1" roundness="0" rot="R0"/>
<smd name="24" x="1.3812" y="1.6312" dx="0.175" dy="0.175" layer="1" roundness="100" rot="R0"/>
<smd name="25" x="0.9" y="1.7" dx="0.2" dy="1" layer="1" roundness="0" rot="R0"/>
<smd name="26" x="0.45" y="1.7" dx="0.2" dy="1" layer="1" roundness="0" rot="R0"/>
<smd name="27" x="0" y="1.7" dx="0.2" dy="1" layer="1" roundness="0" rot="R0"/>
<smd name="28" x="-0.45" y="1.7" dx="0.2" dy="1" layer="1" roundness="0" rot="R0"/>
<smd name="29" x="-0.9" y="1.725" dx="0.2" dy="0.95" layer="1" roundness="0" rot="R0"/>
<wire x1="-2.0574" y1="-2.0574" x2="-2.0574" y2="2.0574" width="0.1524" layer="51"/>
<wire x1="-2.0574" y1="-2.0574" x2="2.0574" y2="-2.0574" width="0.1524" layer="51"/>
<wire x1="2.0574" y1="-2.0574" x2="2.0574" y2="2.0574" width="0.1524" layer="51"/>
<wire x1="-2.0574" y1="2.0574" x2="2.0574" y2="2.0574" width="0.1524" layer="51"/>
<wire x1="2.0574" y1="-2.3114" x2="2.3114" y2="-2.3114" width="0.2032" layer="21"/>
<wire x1="2.3114" y1="-2.3114" x2="2.3114" y2="-2.0574" width="0.2032" layer="21"/>
<wire x1="-2.3114" y1="-2.3114" x2="-2.3114" y2="-2.0574" width="0.2032" layer="21"/>
<wire x1="-2.3114" y1="-2.3114" x2="-2.0574" y2="-2.3114" width="0.2032" layer="21"/>
<wire x1="2.3114" y1="2.0574" x2="2.3114" y2="2.3114" width="0.2032" layer="21"/>
<wire x1="2.0574" y1="2.3114" x2="2.3114" y2="2.3114" width="0.2032" layer="21"/>
<wire x1="-2.3114" y1="2.0574" x2="-2.3114" y2="2.3114" width="0.2032" layer="21"/>
<wire x1="-2.3114" y1="2.3114" x2="-2.0574" y2="2.3114" width="0.2032" layer="21"/>
<wire x1="-2.6162" y1="1.6002" x2="-2.8702" y2="1.6002" width="0.254" layer="21" curve="-180"/>
<wire x1="-2.8702" y1="1.6002" x2="-2.6162" y2="1.6002" width="0.254" layer="21" curve="-180"/>
<polygon width="0.0254" layer="1"><vertex x="2.2" y="1.65"/><vertex x="2.15" y="1.7"/><vertex x="1.45" y="1.7"/><vertex x="1.45" y="2.15"/><vertex x="1.4" y="2.2"/><vertex x="1.3" y="2.2"/><vertex x="1.25" y="2.15"/><vertex x="1.25" y="1.55"/><vertex x="1.3" y="1.5"/><vertex x="2.15" y="1.5"/><vertex x="2.2" y="1.55"/></polygon>
<polygon width="0.0254" layer="1"><vertex x="2.2" y="-1.65"/><vertex x="2.15" y="-1.7"/><vertex x="1.5" y="-1.7"/><vertex x="1.5" y="-2.15"/><vertex x="1.45" y="-2.2"/><vertex x="1.35" y="-2.2"/><vertex x="1.3" y="-2.15"/><vertex x="1.3" y="-1.55"/><vertex x="1.35" y="-1.5"/><vertex x="2.15" y="-1.5"/><vertex x="2.2" y="-1.55"/></polygon>
<polygon width="0.0254" layer="1"><vertex x="-2.2" y="-1.65"/><vertex x="-2.15" y="-1.7"/><vertex x="-1.5" y="-1.7"/><vertex x="-1.5" y="-2.15"/><vertex x="-1.45" y="-2.2"/><vertex x="-1.35" y="-2.2"/><vertex x="-1.3" y="-2.15"/><vertex x="-1.3" y="-1.55"/><vertex x="-1.35" y="-1.5"/><vertex x="-2.15" y="-1.5"/><vertex x="-2.2" y="-1.55"/></polygon>
<polygon width="0.0254" layer="1"><vertex x="-2.2" y="1.65"/><vertex x="-2.15" y="1.7"/><vertex x="-1.45" y="1.7"/><vertex x="-1.45" y="2.15"/><vertex x="-1.4" y="2.2"/><vertex x="-1.3" y="2.2"/><vertex x="-1.25" y="2.15"/><vertex x="-1.25" y="1.55"/><vertex x="-1.3" y="1.5"/><vertex x="-2.15" y="1.5"/><vertex x="-2.2" y="1.55"/></polygon>
<polygon width="0.0254" layer="29"><vertex x="2.2" y="1.65"/><vertex x="2.2" y="1.55"/><vertex x="2.15" y="1.5"/><vertex x="1.3" y="1.5"/><vertex x="1.25" y="1.55"/><vertex x="1.25" y="2.15"/><vertex x="1.3" y="2.2"/><vertex x="1.4" y="2.2"/><vertex x="1.45" y="2.15"/><vertex x="1.45" y="1.7"/><vertex x="2.15" y="1.7"/></polygon>
<polygon width="0.0254" layer="29"><vertex x="2.2" y="-1.65"/><vertex x="2.15" y="-1.7"/><vertex x="1.5" y="-1.7"/><vertex x="1.5" y="-2.15"/><vertex x="1.45" y="-2.2"/><vertex x="1.35" y="-2.2"/><vertex x="1.3" y="-2.15"/><vertex x="1.3" y="-1.55"/><vertex x="1.35" y="-1.5"/><vertex x="2.15" y="-1.5"/><vertex x="2.2" y="-1.55"/></polygon>
<polygon width="0.0254" layer="29"><vertex x="-2.2" y="-1.65"/><vertex x="-2.2" y="-1.55"/><vertex x="-2.15" y="-1.5"/><vertex x="-1.35" y="-1.5"/><vertex x="-1.3" y="-1.55"/><vertex x="-1.3" y="-2.15"/><vertex x="-1.35" y="-2.2"/><vertex x="-1.45" y="-2.2"/><vertex x="-1.5" y="-2.15"/><vertex x="-1.5" y="-1.7"/><vertex x="-2.15" y="-1.7"/></polygon>
<polygon width="0.0254" layer="29"><vertex x="-2.2" y="1.65"/><vertex x="-2.15" y="1.7"/><vertex x="-1.45" y="1.7"/><vertex x="-1.45" y="2.15"/><vertex x="-1.4" y="2.2"/><vertex x="-1.3" y="2.2"/><vertex x="-1.25" y="2.15"/><vertex x="-1.25" y="1.55"/><vertex x="-1.3" y="1.5"/><vertex x="-2.15" y="1.5"/><vertex x="-2.2" y="1.55"/></polygon>
<polygon width="0.0254" layer="31"><vertex x="2.2" y="1.65"/><vertex x="2.2" y="1.55"/><vertex x="2.15" y="1.5"/><vertex x="1.3" y="1.5"/><vertex x="1.25" y="1.55"/><vertex x="1.25" y="2.15"/><vertex x="1.3" y="2.2"/><vertex x="1.4" y="2.2"/><vertex x="1.45" y="2.15"/><vertex x="1.45" y="1.7"/><vertex x="2.15" y="1.7"/></polygon>
<polygon width="0.0254" layer="31"><vertex x="2.2" y="-1.65"/><vertex x="2.15" y="-1.7"/><vertex x="1.5" y="-1.7"/><vertex x="1.5" y="-2.15"/><vertex x="1.45" y="-2.2"/><vertex x="1.35" y="-2.2"/><vertex x="1.3" y="-2.15"/><vertex x="1.3" y="-1.55"/><vertex x="1.35" y="-1.5"/><vertex x="2.15" y="-1.5"/><vertex x="2.2" y="-1.55"/></polygon>
<polygon width="0.0254" layer="31"><vertex x="-2.2" y="-1.65"/><vertex x="-2.2" y="-1.55"/><vertex x="-2.15" y="-1.5"/><vertex x="-1.35" y="-1.5"/><vertex x="-1.3" y="-1.55"/><vertex x="-1.3" y="-2.15"/><vertex x="-1.35" y="-2.2"/><vertex x="-1.45" y="-2.2"/><vertex x="-1.5" y="-2.15"/><vertex x="-1.5" y="-1.7"/><vertex x="-2.15" y="-1.7"/></polygon>
<polygon width="0.0254" layer="31"><vertex x="-2.2" y="1.65"/><vertex x="-2.15" y="1.7"/><vertex x="-1.45" y="1.7"/><vertex x="-1.45" y="2.15"/><vertex x="-1.4" y="2.2"/><vertex x="-1.3" y="2.2"/><vertex x="-1.25" y="2.15"/><vertex x="-1.25" y="1.55"/><vertex x="-1.3" y="1.5"/><vertex x="-2.15" y="1.5"/><vertex x="-2.2" y="1.55"/></polygon>
<text x="-1.7272" y="-0.635" size="1.27" layer="27" ratio="6">&gt;VALUE</text>
<text x="-3.2766" y="-0.635" size="1.27" layer="27" ratio="6">&gt;NAME</text>
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
<package name="C_0402_1005Metric">
<smd name="1" x="-0.48" y="-0" dx="0.56" dy="0.62" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.48" y="-0" dx="0.56" dy="0.62" layer="1" roundness="50" rot="R0"/>
<wire x1="-0.1078" y1="0.36" x2="0.1078" y2="0.36" width="0.12" layer="21"/>
<wire x1="-0.1078" y1="-0.36" x2="0.1078" y2="-0.36" width="0.12" layer="21"/>
<wire x1="-0.5" y1="0.25" x2="0.5" y2="0.25" width="0.1" layer="51"/>
<wire x1="0.5" y1="0.25" x2="0.5" y2="-0.25" width="0.1" layer="51"/>
<wire x1="0.5" y1="-0.25" x2="-0.5" y2="-0.25" width="0.1" layer="51"/>
<wire x1="-0.5" y1="-0.25" x2="-0.5" y2="0.25" width="0.1" layer="51"/>
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
<package name="LED_0603_1608Metric">
<smd name="1" x="-0.7875" y="-0" dx="0.875" dy="0.95" layer="1" roundness="50" rot="R0"/>
<smd name="2" x="0.7875" y="-0" dx="0.875" dy="0.95" layer="1" roundness="50" rot="R0"/>
<wire x1="-1.485" y1="0.735" x2="-1.485" y2="-0.735" width="0.12" layer="21"/>
<wire x1="-1.485" y1="-0.735" x2="0.8" y2="-0.735" width="0.12" layer="21"/>
<wire x1="0.8" y1="0.735" x2="-1.485" y2="0.735" width="0.12" layer="21"/>
<wire x1="-0.8" y1="0.1" x2="-0.8" y2="-0.4" width="0.1" layer="51"/>
<wire x1="-0.8" y1="-0.4" x2="0.8" y2="-0.4" width="0.1" layer="51"/>
<wire x1="-0.5" y1="0.4" x2="-0.8" y2="0.1" width="0.1" layer="51"/>
<wire x1="0.8" y1="0.4" x2="-0.5" y2="0.4" width="0.1" layer="51"/>
<wire x1="0.8" y1="-0.4" x2="0.8" y2="0.4" width="0.1" layer="51"/>
<text x="0" y="1.5" size="1" layer="25" align="bottom-center">&gt;NAME</text>
<text x="0" y="-1.5" size="1" layer="27" align="top-center">&gt;VALUE</text>
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
<symbol name="BQ25798">
<wire x1="5.08" y1="-81.28" x2="40.64" y2="-81.28" width="0.2032" layer="94"/>
<wire x1="40.64" y1="-81.28" x2="40.64" y2="5.08" width="0.2032" layer="94"/>
<wire x1="40.64" y1="5.08" x2="5.08" y2="5.08" width="0.2032" layer="94"/>
<wire x1="5.08" y1="5.08" x2="5.08" y2="-81.28" width="0.2032" layer="94"/>
<text x="-4.7244" y="1.4986" size="2.0828" layer="95" ratio="6">&gt;NAME</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<text x="-5.3594" y="-1.0414" size="2.0828" layer="96" ratio="6">&gt;VALUE</text>
<pin name="STAT" x="45.72" y="-63.5" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="VBUS_2" x="0" y="0" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="VBUS" x="0" y="-2.54" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="BTST1" x="45.72" y="0" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="REGN" x="0" y="-40.64" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="D+" x="0" y="-50.8" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="D-" x="0" y="-48.26" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="VAC2" x="0" y="-12.7" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="VAC1" x="0" y="-10.16" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="ACDRV2" x="0" y="-22.86" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="ACDRV1" x="0" y="-20.32" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R0"/>
<pin name="QON_N" x="0" y="-78.74" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="CE_N" x="0" y="-73.66" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="SCL" x="0" y="-55.88" visible="both" length="middle" direction="in" swaplevel="0" rot="R0"/>
<pin name="SDA" x="0" y="-58.42" visible="both" length="middle" direction="io" swaplevel="0" rot="R0"/>
<pin name="TS" x="45.72" y="-73.66" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="ILIM_HIZ" x="0" y="-68.58" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="BATP" x="45.72" y="-40.64" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="BTST2" x="45.72" y="-15.24" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="PROG" x="0" y="-63.5" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
<pin name="INT_N" x="45.72" y="-68.58" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="BAT_2" x="45.72" y="-48.26" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="BAT" x="45.72" y="-50.8" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="SDRV" x="45.72" y="-58.42" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="SYS" x="45.72" y="-30.48" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="SW2" x="45.72" y="-22.86" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="GND" x="45.72" y="-78.74" visible="both" length="middle" direction="pwr" swaplevel="0" rot="R180"/>
<pin name="SW1" x="45.72" y="-7.62" visible="both" length="middle" direction="pas" swaplevel="0" rot="R180"/>
<pin name="PMID" x="0" y="-30.48" visible="both" length="middle" direction="pas" swaplevel="0" rot="R0"/>
</symbol>
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
<symbol name="D">
<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.1524" layer="94"/>
<polygon width="0.1524" layer="94"><vertex x="-1.27" y="1.27"/><vertex x="1.27" y="0"/><vertex x="-1.27" y="-1.27"/></polygon>
<wire x1="1.27" y1="1.27" x2="1.27" y2="-1.27" width="0.254" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="A" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="C" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
</symbol>
<symbol name="L">
<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.6096" layer="94"/>
<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>
<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>
<pin name="1" x="-5.08" y="0" visible="off" length="short" direction="pas" rot="R0"/>
<pin name="2" x="5.08" y="0" visible="off" length="short" direction="pas" rot="R180"/>
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
</symbols>
<devicesets>
<deviceset name="BQ25798" prefix="U" uservalue="yes">
<gates>
<gate name="A" symbol="BQ25798" x="0" y="0"/>
</gates>
<devices>
<device name="" package="RQM0029A-MFG">
<connects>
<connect gate="A" pin="STAT" pad="1"/>
<connect gate="A" pin="VBUS_2" pad="2"/>
<connect gate="A" pin="VBUS" pad="3"/>
<connect gate="A" pin="BTST1" pad="4"/>
<connect gate="A" pin="REGN" pad="5"/>
<connect gate="A" pin="D+" pad="6"/>
<connect gate="A" pin="D-" pad="7"/>
<connect gate="A" pin="VAC2" pad="8"/>
<connect gate="A" pin="VAC1" pad="9"/>
<connect gate="A" pin="ACDRV2" pad="10"/>
<connect gate="A" pin="ACDRV1" pad="11"/>
<connect gate="A" pin="QON_N" pad="12"/>
<connect gate="A" pin="CE_N" pad="13"/>
<connect gate="A" pin="SCL" pad="14"/>
<connect gate="A" pin="SDA" pad="15"/>
<connect gate="A" pin="TS" pad="16"/>
<connect gate="A" pin="ILIM_HIZ" pad="17"/>
<connect gate="A" pin="BATP" pad="18"/>
<connect gate="A" pin="BTST2" pad="19"/>
<connect gate="A" pin="PROG" pad="20"/>
<connect gate="A" pin="INT_N" pad="21"/>
<connect gate="A" pin="BAT_2" pad="22"/>
<connect gate="A" pin="BAT" pad="23"/>
<connect gate="A" pin="SDRV" pad="24"/>
<connect gate="A" pin="SYS" pad="25"/>
<connect gate="A" pin="SW2" pad="26"/>
<connect gate="A" pin="GND" pad="27"/>
<connect gate="A" pin="SW1" pad="28"/>
<connect gate="A" pin="PMID" pad="29"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="C" prefix="C" uservalue="yes">
<gates>
<gate name="G$1" symbol="C" x="0" y="0"/>
</gates>
<devices>
<device name="C0402" package="C_0402_1005Metric">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
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
<deviceset name="D" prefix="D" uservalue="yes">
<gates>
<gate name="G$1" symbol="D" x="0" y="0"/>
</gates>
<devices>
<device name="LED0603" package="LED_0603_1608Metric">
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
<part name="U2" library="nibbles_power" deviceset="BQ25798" device="" value="BQ25798RQMR"><attribute name="MPN" value="BQ25798RQMR"/><attribute name="LCSC" value="C2876593"/></part>
<part name="L1" library="nibbles_power" deviceset="L" device="MWSA0603S" value="2.2uH Isat&gt;=7A"><attribute name="MPN" value="MWSA0603S-2R2MT"/><attribute name="LCSC" value="C408445"/></part>
<part name="C10" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C11" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C12" library="nibbles_power" deviceset="C" device="C0402" value="0.1uF 25V"/>
<part name="C13" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C14" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C15" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C16" library="nibbles_power" deviceset="C" device="C0402" value="0.1uF 25V"/>
<part name="C17" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C18" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C19" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C20" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C21" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C22" library="nibbles_power" deviceset="C" device="C0402" value="0.1uF 25V"/>
<part name="C23" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C24" library="nibbles_power" deviceset="C" device="C0805" value="10uF 25V"><attribute name="MPN" value="CL21A106KAYNNNE"/><attribute name="LCSC" value="C15850"/></part>
<part name="C25" library="nibbles_power" deviceset="C" device="C0603" value="4.7uF 10V"/>
<part name="C26" library="nibbles_power" deviceset="C" device="C0603" value="47nF 25V"/>
<part name="C27" library="nibbles_power" deviceset="C" device="C0603" value="47nF 25V"/>
<part name="C28" library="nibbles_power" deviceset="C" device="C0603" value="1nF 50V"/>
<part name="R10" library="nibbles_power" deviceset="R" device="0603" value="27.0k 1%"/>
<part name="R11" library="nibbles_power" deviceset="R" device="0603" value="100"/>
<part name="R12" library="nibbles_power" deviceset="R" device="0603" value="5.23k 1%"/>
<part name="R13" library="nibbles_power" deviceset="R" device="0603" value="30.1k 1%"/>
<part name="R14" library="nibbles_power" deviceset="R" device="0603" value="10k"/>
<part name="R37" library="nibbles_power" deviceset="R" device="0603" value="10k 1%"/>
<part name="R38" library="nibbles_power" deviceset="R" device="0603" value="10.7k 1%"/>
<part name="R15" library="nibbles_power" deviceset="R" device="0603" value="2.2k"/>
<part name="LED1" library="nibbles_power" deviceset="D" device="LED0603" value="LED (charging)"><attribute name="MPN" value="KT-0603R"/><attribute name="LCSC" value="C2286"/></part>
<part name="J4" library="nibbles_power" deviceset="CONN2" device="PH2" value="NTC 103AT on the cells"><attribute name="MPN" value="S2B-PH-K-S"/><attribute name="LCSC" value="C173752"/></part>
<part name="J21" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (from the power board: input)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="J22" library="nibbles_power" deviceset="CONN2" device="XT30" value="XT30 (to the power board: battery)"><attribute name="MPN" value="XT30PW-M"/><attribute name="LCSC" value="C431092"/></part>
<part name="J23" library="nibbles_power" deviceset="CONN6" device="GH6" value="JST-GH 6 (to the power board: I2C)"><attribute name="MPN" value="SM06B-GHS-TB"/></part>
</parts>
<sheets>
<sheet>
<plain>
<text x="0" y="12.7" size="3.81" layer="97">1 Charger: BQ25798, internal pack</text>
<text x="0" y="5.08" size="2.1844" layer="97">A separate board on the power board's charger port: J21 (XT30) DOCK_VIN in, J22 (XT30) to the pack (PACK_F,</text>
<text x="0" y="1.27" size="2.1844" layer="97">after the pack fuse on the power board), J23 (JST-GH 6) I2C, INT and supplies. Pull-ups are on the power board.</text>
<text x="0" y="-2.54" size="2.1844" layer="97">BQ25798RQM: 750 kHz, 4S defaults (PROG 27.0k), so L1 = 2.2 uH (saturation &gt;= 7 A). Datasheet SLUSDV2C.</text>
<text x="0" y="-6.35" size="2.1844" layer="97">One input straight to VBUS with no ACFET-RBFETs (datasheet 7.3.5.2, Figure 8-1): VAC1 and VAC2 tied to VBUS,</text>
<text x="0" y="-10.16" size="2.1844" layer="97">ACDRV1 and ACDRV2 to GND. Compare with Figure 8-1 before layout, including its optional input snubber</text>
<text x="0" y="-13.97" size="2.1844" layer="97">(recommended when hot plugging adapters above 15 V). Never enable OTG: it would drive VBUS.</text>
<text x="0" y="-17.78" size="2.1844" layer="97">SYS powers nothing (the base's 5 V comes from the power board's LIGHT_BUS); it keeps its capacitors.</text>
<text x="0" y="-21.59" size="2.1844" layer="97">ILIM_HIZ divider 10k/10.7k from REGN: 2.59 V = a 2.0 A input clamp at power-up (VILIM = 1 V + 0.8 Ohm x ILIM),</text>
<text x="0" y="-25.4" size="2.1844" layer="97">so lights (~7 A) plus charging stay under a VB99 D-Tap's 10 A with no firmware. The base can lift it by clearing</text>
<text x="0" y="-29.21" size="2.1844" layer="97">EN_EXTILIM and writing IINDPM (datasheet 7.3.4.3).</text>
<text x="0" y="-33.02" size="2.1844" layer="97">TS: 5.23k / 30.1k divider from REGN with a 103AT 10k NTC on the cells (J4; 0 to 60 C window).</text>
<text x="0" y="-36.83" size="2.1844" layer="97">No ship FET (the pack's own BMS protects it): SDRV gets 1 nF to GND. QON left to its internal pull-up (test point).</text>
<text x="0" y="-40.64" size="2.1844" layer="97">D+/D- not connected (no USB input); disable AUTO_INDET in firmware.</text>
</plain>
<instances>
<instance part="U2" gate="A" x="330.2" y="-68.58"/>
<instance part="R14" gate="G$1" x="304.8" y="-162.56" rot="R270"/>
<instance part="R37" gate="G$1" x="281.94" y="-162.56" rot="R90"/>
<instance part="R38" gate="G$1" x="269.24" y="-162.56" rot="R270"/>
<instance part="R10" gate="G$1" x="243.84" y="-162.56" rot="R270"/>
<instance part="C25" gate="G$1" x="218.44" y="-162.56" rot="R270"/>
<instance part="R15" gate="G$1" x="205.74" y="-162.56" rot="R270"/>
<instance part="C13" gate="G$1" x="185.42" y="-162.56" rot="R270"/>
<instance part="C14" gate="G$1" x="172.72" y="-162.56" rot="R270"/>
<instance part="C15" gate="G$1" x="160.02" y="-162.56" rot="R270"/>
<instance part="C16" gate="G$1" x="147.32" y="-162.56" rot="R270"/>
<instance part="C10" gate="G$1" x="127" y="-162.56" rot="R270"/>
<instance part="C11" gate="G$1" x="114.3" y="-162.56" rot="R270"/>
<instance part="C12" gate="G$1" x="101.6" y="-162.56" rot="R270"/>
<instance part="R12" gate="G$1" x="401.32" y="-162.56" rot="R90"/>
<instance part="R13" gate="G$1" x="414.02" y="-162.56" rot="R270"/>
<instance part="LED1" gate="G$1" x="436.88" y="-162.56" rot="R90"/>
<instance part="C28" gate="G$1" x="462.28" y="-162.56" rot="R270"/>
<instance part="C23" gate="G$1" x="482.6" y="-162.56" rot="R270"/>
<instance part="C24" gate="G$1" x="495.3" y="-162.56" rot="R270"/>
<instance part="R11" gate="G$1" x="518.16" y="-162.56" rot="R270"/>
<instance part="C17" gate="G$1" x="538.48" y="-162.56" rot="R270"/>
<instance part="C18" gate="G$1" x="551.18" y="-162.56" rot="R270"/>
<instance part="C19" gate="G$1" x="563.88" y="-162.56" rot="R270"/>
<instance part="C20" gate="G$1" x="576.58" y="-162.56" rot="R270"/>
<instance part="C21" gate="G$1" x="589.28" y="-162.56" rot="R270"/>
<instance part="C22" gate="G$1" x="601.98" y="-162.56" rot="R270"/>
<instance part="C27" gate="G$1" x="619.76" y="-162.56" rot="R270"/>
<instance part="L1" gate="G$1" x="640.08" y="-162.56" rot="R270"/>
<instance part="C26" gate="G$1" x="657.86" y="-162.56" rot="R270"/>
<instance part="J4" gate="G$1" x="843.28" y="-71.12"/>
<instance part="J21" gate="G$1" x="914.4" y="-71.12"/>
<instance part="J22" gate="G$1" x="985.52" y="-71.12"/>
<instance part="J23" gate="G$1" x="1056.64" y="-76.2"/>
</instances>
<busses>
</busses>
<nets>
<net name="+3V3" class="0">
<segment>
<pinref part="J23" gate="G$1" pin="2"/>
<wire x1="1051.56" y1="-72.39" x2="1046.48" y2="-72.39" width="0.1524" layer="91"/>
<label x="1046.48" y="-72.39" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="+5V" class="0">
<segment>
<pinref part="J23" gate="G$1" pin="6"/>
<wire x1="1051.56" y1="-82.55" x2="1046.48" y2="-82.55" width="0.1524" layer="91"/>
<label x="1046.48" y="-82.55" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="BATP" class="0">
<segment>
<pinref part="U2" gate="A" pin="BATP"/>
<pinref part="R11" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-109.22" x2="518.16" y2="-109.22" width="0.1524" layer="91"/>
<wire x1="518.16" y1="-109.22" x2="518.16" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="518.16" y1="-157.48" x2="520.7" y2="-157.48" width="0.1524" layer="91"/>
<junction x="518.16" y="-157.48"/>
<label x="520.7" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="BTST1" class="0">
<segment>
<pinref part="U2" gate="A" pin="BTST1"/>
<pinref part="C26" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-68.58" x2="657.86" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="657.86" y1="-68.58" x2="657.86" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="657.86" y1="-157.48" x2="660.4" y2="-157.48" width="0.1524" layer="91"/>
<junction x="657.86" y="-157.48"/>
<label x="660.4" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="BTST2" class="0">
<segment>
<pinref part="U2" gate="A" pin="BTST2"/>
<pinref part="C27" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-83.82" x2="619.76" y2="-83.82" width="0.1524" layer="91"/>
<wire x1="619.76" y1="-83.82" x2="619.76" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="619.76" y1="-157.48" x2="622.3" y2="-157.48" width="0.1524" layer="91"/>
<junction x="619.76" y="-157.48"/>
<label x="622.3" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="CHG_CE" class="0">
<segment>
<pinref part="U2" gate="A" pin="CE_N"/>
<pinref part="R14" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-142.24" x2="304.8" y2="-142.24" width="0.1524" layer="91"/>
<wire x1="304.8" y1="-142.24" x2="304.8" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="304.8" y1="-157.48" x2="302.26" y2="-157.48" width="0.1524" layer="91"/>
<junction x="304.8" y="-157.48"/>
<label x="302.26" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="CHG_ILIM" class="0">
<segment>
<pinref part="U2" gate="A" pin="ILIM_HIZ"/>
<pinref part="R37" gate="G$1" pin="2"/>
<pinref part="R38" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-137.16" x2="281.94" y2="-137.16" width="0.1524" layer="91"/>
<wire x1="281.94" y1="-137.16" x2="281.94" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="281.94" y1="-157.48" x2="269.24" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="269.24" y1="-157.48" x2="266.7" y2="-157.48" width="0.1524" layer="91"/>
<junction x="281.94" y="-157.48"/>
<junction x="269.24" y="-157.48"/>
<label x="266.7" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="CHG_PROG" class="0">
<segment>
<pinref part="U2" gate="A" pin="PROG"/>
<pinref part="R10" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-132.08" x2="243.84" y2="-132.08" width="0.1524" layer="91"/>
<wire x1="243.84" y1="-132.08" x2="243.84" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="243.84" y1="-157.48" x2="241.3" y2="-157.48" width="0.1524" layer="91"/>
<junction x="243.84" y="-157.48"/>
<label x="241.3" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="CHG_QON" class="0">
<segment>
<pinref part="U2" gate="A" pin="QON_N"/>
<wire x1="330.2" y1="-147.32" x2="325.12" y2="-147.32" width="0.1524" layer="91"/>
<label x="325.12" y="-147.32" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="CHG_STAT" class="0">
<segment>
<pinref part="U2" gate="A" pin="STAT"/>
<pinref part="LED1" gate="G$1" pin="C"/>
<wire x1="375.92" y1="-132.08" x2="436.88" y2="-132.08" width="0.1524" layer="91"/>
<wire x1="436.88" y1="-132.08" x2="436.88" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="436.88" y1="-157.48" x2="439.42" y2="-157.48" width="0.1524" layer="91"/>
<junction x="436.88" y="-157.48"/>
<label x="439.42" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="CHG_TS" class="0">
<segment>
<pinref part="U2" gate="A" pin="TS"/>
<pinref part="R12" gate="G$1" pin="2"/>
<pinref part="R13" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-142.24" x2="401.32" y2="-142.24" width="0.1524" layer="91"/>
<wire x1="401.32" y1="-142.24" x2="401.32" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="401.32" y1="-157.48" x2="414.02" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="414.02" y1="-157.48" x2="416.56" y2="-157.48" width="0.1524" layer="91"/>
<junction x="401.32" y="-157.48"/>
<junction x="414.02" y="-157.48"/>
<label x="416.56" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J4" gate="G$1" pin="1"/>
<wire x1="838.2" y1="-69.85" x2="833.12" y2="-69.85" width="0.1524" layer="91"/>
<label x="833.12" y="-69.85" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="DOCK_VIN" class="0">
<segment>
<pinref part="U2" gate="A" pin="VBUS_2"/>
<pinref part="C10" gate="G$1" pin="1"/>
<pinref part="C11" gate="G$1" pin="1"/>
<pinref part="C12" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-68.58" x2="127" y2="-68.58" width="0.1524" layer="91"/>
<wire x1="127" y1="-68.58" x2="127" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="127" y1="-157.48" x2="114.3" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="114.3" y1="-157.48" x2="101.6" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="101.6" y1="-157.48" x2="99.06" y2="-157.48" width="0.1524" layer="91"/>
<junction x="127" y="-157.48"/>
<junction x="114.3" y="-157.48"/>
<junction x="101.6" y="-157.48"/>
<label x="99.06" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="VBUS"/>
<wire x1="330.2" y1="-71.12" x2="325.12" y2="-71.12" width="0.1524" layer="91"/>
<label x="325.12" y="-71.12" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="VAC2"/>
<wire x1="330.2" y1="-81.28" x2="325.12" y2="-81.28" width="0.1524" layer="91"/>
<label x="325.12" y="-81.28" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="VAC1"/>
<wire x1="330.2" y1="-78.74" x2="325.12" y2="-78.74" width="0.1524" layer="91"/>
<label x="325.12" y="-78.74" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J21" gate="G$1" pin="1"/>
<wire x1="909.32" y1="-69.85" x2="904.24" y2="-69.85" width="0.1524" layer="91"/>
<label x="904.24" y="-69.85" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="R14" gate="G$1" pin="2"/>
<wire x1="304.8" y1="-167.64" x2="304.8" y2="-170.18" width="0.1524" layer="91"/>
<label x="304.8" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R38" gate="G$1" pin="2"/>
<wire x1="269.24" y1="-167.64" x2="269.24" y2="-170.18" width="0.1524" layer="91"/>
<label x="269.24" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R10" gate="G$1" pin="2"/>
<wire x1="243.84" y1="-167.64" x2="243.84" y2="-170.18" width="0.1524" layer="91"/>
<label x="243.84" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C25" gate="G$1" pin="2"/>
<wire x1="218.44" y1="-167.64" x2="218.44" y2="-170.18" width="0.1524" layer="91"/>
<label x="218.44" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C13" gate="G$1" pin="2"/>
<wire x1="185.42" y1="-167.64" x2="185.42" y2="-170.18" width="0.1524" layer="91"/>
<label x="185.42" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C14" gate="G$1" pin="2"/>
<wire x1="172.72" y1="-167.64" x2="172.72" y2="-170.18" width="0.1524" layer="91"/>
<label x="172.72" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C15" gate="G$1" pin="2"/>
<wire x1="160.02" y1="-167.64" x2="160.02" y2="-170.18" width="0.1524" layer="91"/>
<label x="160.02" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C16" gate="G$1" pin="2"/>
<wire x1="147.32" y1="-167.64" x2="147.32" y2="-170.18" width="0.1524" layer="91"/>
<label x="147.32" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C10" gate="G$1" pin="2"/>
<wire x1="127" y1="-167.64" x2="127" y2="-170.18" width="0.1524" layer="91"/>
<label x="127" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C11" gate="G$1" pin="2"/>
<wire x1="114.3" y1="-167.64" x2="114.3" y2="-170.18" width="0.1524" layer="91"/>
<label x="114.3" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C12" gate="G$1" pin="2"/>
<wire x1="101.6" y1="-167.64" x2="101.6" y2="-170.18" width="0.1524" layer="91"/>
<label x="101.6" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="R13" gate="G$1" pin="2"/>
<wire x1="414.02" y1="-167.64" x2="414.02" y2="-170.18" width="0.1524" layer="91"/>
<label x="414.02" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C28" gate="G$1" pin="2"/>
<wire x1="462.28" y1="-167.64" x2="462.28" y2="-170.18" width="0.1524" layer="91"/>
<label x="462.28" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C23" gate="G$1" pin="2"/>
<wire x1="482.6" y1="-167.64" x2="482.6" y2="-170.18" width="0.1524" layer="91"/>
<label x="482.6" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C24" gate="G$1" pin="2"/>
<wire x1="495.3" y1="-167.64" x2="495.3" y2="-170.18" width="0.1524" layer="91"/>
<label x="495.3" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C17" gate="G$1" pin="2"/>
<wire x1="538.48" y1="-167.64" x2="538.48" y2="-170.18" width="0.1524" layer="91"/>
<label x="538.48" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C18" gate="G$1" pin="2"/>
<wire x1="551.18" y1="-167.64" x2="551.18" y2="-170.18" width="0.1524" layer="91"/>
<label x="551.18" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C19" gate="G$1" pin="2"/>
<wire x1="563.88" y1="-167.64" x2="563.88" y2="-170.18" width="0.1524" layer="91"/>
<label x="563.88" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C20" gate="G$1" pin="2"/>
<wire x1="576.58" y1="-167.64" x2="576.58" y2="-170.18" width="0.1524" layer="91"/>
<label x="576.58" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C21" gate="G$1" pin="2"/>
<wire x1="589.28" y1="-167.64" x2="589.28" y2="-170.18" width="0.1524" layer="91"/>
<label x="589.28" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="C22" gate="G$1" pin="2"/>
<wire x1="601.98" y1="-167.64" x2="601.98" y2="-170.18" width="0.1524" layer="91"/>
<label x="601.98" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="ACDRV2"/>
<wire x1="330.2" y1="-91.44" x2="325.12" y2="-91.44" width="0.1524" layer="91"/>
<label x="325.12" y="-91.44" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="ACDRV1"/>
<wire x1="330.2" y1="-88.9" x2="325.12" y2="-88.9" width="0.1524" layer="91"/>
<label x="325.12" y="-88.9" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="GND"/>
<wire x1="375.92" y1="-147.32" x2="381" y2="-147.32" width="0.1524" layer="91"/>
<label x="381" y="-147.32" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J4" gate="G$1" pin="2"/>
<wire x1="838.2" y1="-72.39" x2="833.12" y2="-72.39" width="0.1524" layer="91"/>
<label x="833.12" y="-72.39" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J21" gate="G$1" pin="2"/>
<wire x1="909.32" y1="-72.39" x2="904.24" y2="-72.39" width="0.1524" layer="91"/>
<label x="904.24" y="-72.39" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J22" gate="G$1" pin="2"/>
<wire x1="980.44" y1="-72.39" x2="975.36" y2="-72.39" width="0.1524" layer="91"/>
<label x="975.36" y="-72.39" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J23" gate="G$1" pin="1"/>
<wire x1="1051.56" y1="-69.85" x2="1046.48" y2="-69.85" width="0.1524" layer="91"/>
<label x="1046.48" y="-69.85" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PACK_F" class="0">
<segment>
<pinref part="U2" gate="A" pin="BAT_2"/>
<pinref part="C23" gate="G$1" pin="1"/>
<pinref part="C24" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-116.84" x2="482.6" y2="-116.84" width="0.1524" layer="91"/>
<wire x1="482.6" y1="-116.84" x2="482.6" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="482.6" y1="-157.48" x2="495.3" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="495.3" y1="-157.48" x2="497.84" y2="-157.48" width="0.1524" layer="91"/>
<junction x="482.6" y="-157.48"/>
<junction x="495.3" y="-157.48"/>
<label x="497.84" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="R11" gate="G$1" pin="2"/>
<wire x1="518.16" y1="-167.64" x2="518.16" y2="-170.18" width="0.1524" layer="91"/>
<label x="518.16" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="BAT"/>
<wire x1="375.92" y1="-119.38" x2="381" y2="-119.38" width="0.1524" layer="91"/>
<label x="381" y="-119.38" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J22" gate="G$1" pin="1"/>
<wire x1="980.44" y1="-69.85" x2="975.36" y2="-69.85" width="0.1524" layer="91"/>
<label x="975.36" y="-69.85" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PMID" class="0">
<segment>
<pinref part="U2" gate="A" pin="PMID"/>
<pinref part="C13" gate="G$1" pin="1"/>
<pinref part="C14" gate="G$1" pin="1"/>
<pinref part="C15" gate="G$1" pin="1"/>
<pinref part="C16" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-99.06" x2="185.42" y2="-99.06" width="0.1524" layer="91"/>
<wire x1="185.42" y1="-99.06" x2="185.42" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="185.42" y1="-157.48" x2="172.72" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="172.72" y1="-157.48" x2="160.02" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="160.02" y1="-157.48" x2="147.32" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="147.32" y1="-157.48" x2="144.78" y2="-157.48" width="0.1524" layer="91"/>
<junction x="185.42" y="-157.48"/>
<junction x="172.72" y="-157.48"/>
<junction x="160.02" y="-157.48"/>
<junction x="147.32" y="-157.48"/>
<label x="144.78" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_INT" class="0">
<segment>
<pinref part="U2" gate="A" pin="INT_N"/>
<wire x1="375.92" y1="-137.16" x2="381" y2="-137.16" width="0.1524" layer="91"/>
<label x="381" y="-137.16" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="J23" gate="G$1" pin="5"/>
<wire x1="1051.56" y1="-80.01" x2="1046.48" y2="-80.01" width="0.1524" layer="91"/>
<label x="1046.48" y="-80.01" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SCL" class="0">
<segment>
<pinref part="U2" gate="A" pin="SCL"/>
<wire x1="330.2" y1="-124.46" x2="325.12" y2="-124.46" width="0.1524" layer="91"/>
<label x="325.12" y="-124.46" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J23" gate="G$1" pin="4"/>
<wire x1="1051.56" y1="-77.47" x2="1046.48" y2="-77.47" width="0.1524" layer="91"/>
<label x="1046.48" y="-77.47" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="PWR_SDA" class="0">
<segment>
<pinref part="U2" gate="A" pin="SDA"/>
<wire x1="330.2" y1="-127" x2="325.12" y2="-127" width="0.1524" layer="91"/>
<label x="325.12" y="-127" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="J23" gate="G$1" pin="3"/>
<wire x1="1051.56" y1="-74.93" x2="1046.48" y2="-74.93" width="0.1524" layer="91"/>
<label x="1046.48" y="-74.93" size="1.778" layer="95" rot="R180"/>
</segment>
</net>
<net name="REGN" class="0">
<segment>
<pinref part="R37" gate="G$1" pin="1"/>
<wire x1="281.94" y1="-167.64" x2="281.94" y2="-170.18" width="0.1524" layer="91"/>
<label x="281.94" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="REGN"/>
<pinref part="C25" gate="G$1" pin="1"/>
<pinref part="R15" gate="G$1" pin="1"/>
<wire x1="330.2" y1="-109.22" x2="218.44" y2="-109.22" width="0.1524" layer="91"/>
<wire x1="218.44" y1="-109.22" x2="218.44" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="218.44" y1="-157.48" x2="205.74" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="205.74" y1="-157.48" x2="203.2" y2="-157.48" width="0.1524" layer="91"/>
<junction x="218.44" y="-157.48"/>
<junction x="205.74" y="-157.48"/>
<label x="203.2" y="-157.48" size="1.778" layer="95" rot="R180"/>
</segment>
<segment>
<pinref part="R12" gate="G$1" pin="1"/>
<wire x1="401.32" y1="-167.64" x2="401.32" y2="-170.18" width="0.1524" layer="91"/>
<label x="401.32" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="SDRV" class="0">
<segment>
<pinref part="U2" gate="A" pin="SDRV"/>
<pinref part="C28" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-127" x2="462.28" y2="-127" width="0.1524" layer="91"/>
<wire x1="462.28" y1="-127" x2="462.28" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="462.28" y1="-157.48" x2="464.82" y2="-157.48" width="0.1524" layer="91"/>
<junction x="462.28" y="-157.48"/>
<label x="464.82" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="STAT_LED" class="0">
<segment>
<pinref part="R15" gate="G$1" pin="2"/>
<wire x1="205.74" y1="-167.64" x2="205.74" y2="-170.18" width="0.1524" layer="91"/>
<label x="205.74" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="LED1" gate="G$1" pin="A"/>
<wire x1="436.88" y1="-167.64" x2="436.88" y2="-170.18" width="0.1524" layer="91"/>
<label x="436.88" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="SW1" class="0">
<segment>
<pinref part="U2" gate="A" pin="SW1"/>
<pinref part="L1" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-76.2" x2="640.08" y2="-76.2" width="0.1524" layer="91"/>
<wire x1="640.08" y1="-76.2" x2="640.08" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="640.08" y1="-157.48" x2="642.62" y2="-157.48" width="0.1524" layer="91"/>
<junction x="640.08" y="-157.48"/>
<label x="642.62" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
<segment>
<pinref part="C26" gate="G$1" pin="2"/>
<wire x1="657.86" y1="-167.64" x2="657.86" y2="-170.18" width="0.1524" layer="91"/>
<label x="657.86" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
</net>
<net name="SW2" class="0">
<segment>
<pinref part="C27" gate="G$1" pin="2"/>
<wire x1="619.76" y1="-167.64" x2="619.76" y2="-170.18" width="0.1524" layer="91"/>
<label x="619.76" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="L1" gate="G$1" pin="2"/>
<wire x1="640.08" y1="-167.64" x2="640.08" y2="-170.18" width="0.1524" layer="91"/>
<label x="640.08" y="-170.18" size="1.778" layer="95" rot="R270"/>
</segment>
<segment>
<pinref part="U2" gate="A" pin="SW2"/>
<wire x1="375.92" y1="-91.44" x2="381" y2="-91.44" width="0.1524" layer="91"/>
<label x="381" y="-91.44" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
<net name="SYS" class="0">
<segment>
<pinref part="U2" gate="A" pin="SYS"/>
<pinref part="C17" gate="G$1" pin="1"/>
<pinref part="C18" gate="G$1" pin="1"/>
<pinref part="C19" gate="G$1" pin="1"/>
<pinref part="C20" gate="G$1" pin="1"/>
<pinref part="C21" gate="G$1" pin="1"/>
<pinref part="C22" gate="G$1" pin="1"/>
<wire x1="375.92" y1="-99.06" x2="538.48" y2="-99.06" width="0.1524" layer="91"/>
<wire x1="538.48" y1="-99.06" x2="538.48" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="538.48" y1="-157.48" x2="551.18" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="551.18" y1="-157.48" x2="563.88" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="563.88" y1="-157.48" x2="576.58" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="576.58" y1="-157.48" x2="589.28" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="589.28" y1="-157.48" x2="601.98" y2="-157.48" width="0.1524" layer="91"/>
<wire x1="601.98" y1="-157.48" x2="604.52" y2="-157.48" width="0.1524" layer="91"/>
<junction x="538.48" y="-157.48"/>
<junction x="551.18" y="-157.48"/>
<junction x="563.88" y="-157.48"/>
<junction x="576.58" y="-157.48"/>
<junction x="589.28" y="-157.48"/>
<junction x="601.98" y="-157.48"/>
<label x="604.52" y="-157.48" size="1.778" layer="95" rot="R0"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>
