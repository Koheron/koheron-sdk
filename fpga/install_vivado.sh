#!/usr/bin/env bash

vivado_release=2016.4
vivado_version=${vivado_release}_1215_1

tar -xvzf Xilinx_Vivado_SDK_${vivado_version}.tar.gz

cat <<- EOF_CAT > install_config.txt

## Vivado HL WebPACK Install Configuration ##

Edition=Vivado HL WebPACK
Destination=/opt/Xilinx
Modules=Software Development Kit (SDK):1,DocNav:0,Kintex UltraScale:0,Zynq-7000:1,System Generator for DSP:0,Artix-7:1,Kintex-7:0
InstallOptions=Acquire or Manage a License Key:0,Enable WebTalk for SDK to send usage statistics to Xilinx:0

## Shortcuts and File associations ##

CreateProgramGroupShortcuts=0
ProgramGroupFolder=Xilinx Design Tools
CreateShortcutsForAllUsers=0
CreateDesktopShortcuts=0
CreateFileAssociation=0
EOF_CAT

bash Xilinx_Vivado_SDK_${vivado_version}/xsetup --agree 3rdPartyEULA,WebTalkTerms,XilinxEULA --batch Install --config install_config.txt
rm install_config.txt
rm -r Xilinx_Vivado_SDK_${vivado_version}

## System version of glibc ##

for folder in Vivado SDK
do
  path=/opt/Xilinx/${folder}/${vivado_release}/lib/lnx64.o
  mv $path/libstdc++.so.6 $path/libstdc++.so.6.orig
  ln -s /usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.21 $path/libstdc++.so.6
done

## awk issue ##

path=/opt/Xilinx/Vivado/${vivado_release}/lib/lnx64.o
mv $path/libmpfr.so.4 $path/libmpfr.so.4.orig
ln -s /usr/lib/x86_64-linux-gnu/libmpfr.so.4 $path/libmpfr.so.4
