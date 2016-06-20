tar -xvzf Xilinx_Vivado_SDK_2016.1_0409_1.tar.gz

cat <<- EOF_CAT > install_config.txt
#### Vivado HL WebPACK Install Configuration ####
Edition=Vivado HL WebPACK

# Path where Xilinx software will be installed.
Destination=/opt/Xilinx

# Choose the Products/Devices the you would like to install.
Modules=Software Development Kit (SDK):1,DocNav:0,Kintex UltraScale:0,Zynq-7000:1,System Generator for DSP:0,Artix-7:1,Kintex-7:0

# Choose the post install scripts you'd like to run as part of the finalization step. Please note that some of these scripts may require user interaction during runtime.
InstallOptions=Acquire or Manage a License Key:0,Enable WebTalk for SDK to send usage statistics to Xilinx:0

## Shortcuts and File associations ##
# Choose whether Start menu/Application menu shortcuts will be created or not.
CreateProgramGroupShortcuts=0

# Choose the name of the Start menu/Application menu shortcut. This setting will be ignored if you choose NOT to create shortcuts.
ProgramGroupFolder=Xilinx Design Tools

# Choose whether shortcuts will be created for All users or just the Current user. Shortcuts can be created for all users only if you run the installer as administrator.
CreateShortcutsForAllUsers=0

# Choose whether shortcuts will be created on the desktop or not.
CreateDesktopShortcuts=0

# Choose whether file associations will be created or not.
CreateFileAssociation=0
EOF_CAT

bash Xilinx_Vivado_SDK_2016.1_0409_1/xsetup --agree 3rdPartyEULA,WebTalkTerms,XilinxEULA --batch Install --config install_config.txt
rm install_config.txt
rm -r Xilinx_Vivado_SDK_2016.1_0409_1
cp Xilinx.lic /opt/Xilinx
