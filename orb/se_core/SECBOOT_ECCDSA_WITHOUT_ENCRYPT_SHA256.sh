#!/bin/bash - 
#Post build for SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
# arg1 is the build directory
# arg2 is the elf file path+name
# arg3 is the bin file path+name
# arg4 is the firmware Id (1/2/3)
# arg5 is the version
# arg6 when present forces "bigelf" generation
projectdir=$1
FileName=${3##*/}
execname=${FileName%.*}
elf=$2
bin=$3
fwid=$4
version=$5

SecureEngine=${0%/*}

userAppBinary=$projectdir"/../Binary"

sfb=$userAppBinary"/"$execname".sfb"
sign=$userAppBinary"/"$execname".sign"
headerbin=$userAppBinary"/"$execname"sfuh.bin"
bigbinary=$userAppBinary"/SBSFU_"$execname".bin"
magic="SFU"$fwid
ecckey=$SecureEngine"/../Binary/ECCKEY"$fwid".txt"
partialbin=$userAppBinary"/Partial"$execname".bin"
partialsfb=$userAppBinary"/Partial"$execname".sfb"
partialsign=$userAppBinary"/Partial"$execname".sign"
partialoffset=$userAppBinary"/Partial"$execname".offset"
ref_userapp=$projectdir"/RefUserApp.bin"
offset=4096
alignment=16

current_directory=`pwd`
cd  $SecureEngine"/../../"
SecureDir=`pwd`
cd $current_directory
sbsfuelf="$SecureDir/2_Images_SBSFU/SW4STM32/NUCLEO-G474RE_2_Images_SBSFU/Debug/SBSFU.elf"

current_directory=`pwd`
cd $1/../../../../../../Middlewares/ST/STM32_Secure_Engine/Utilities/KeysAndImages
basedir=`pwd`
cd $current_directory
# test if window executable usable
prepareimage=$basedir"/win/prepareimage/prepareimage.exe"
uname  | grep -i -e windows -e mingw > /dev/null 2>&1
if [ $? -eq 0 ] && [  -e "$prepareimage" ]; then
  echo "prepareimage with windows executable"
  export PATH=$basedir"\win\prepareimage";$PATH > /dev/null 2>&1
  cmd=""
  prepareimage="prepareimage.exe"
else
  # line for python
  echo "prepareimage with python script"
  prepareimage=$basedir/prepareimage.py
  cmd="python"
fi

echo "$cmd $prepareimage" >> $1/output.txt
# Make sure we have a Binary sub-folder in UserApp folder
if [ ! -e $userAppBinary ]; then
mkdir $userAppBinary
fi



command=$cmd" "$prepareimage" sha256 "$bin" "$sign
$command >> $projectdir"/output.txt"
ret=$?
if [ $ret -eq 0 ]; then 
  command=$cmd" "$prepareimage" pack -m "$magic" -k "$ecckey" -p 1 -r 44 -v "$version" -f "$bin" -t "$sign" "$sfb" -o "$offset 
  $command >> $projectdir"/output.txt"
  ret=$?
  if [ $ret -eq 0 ]; then
    command=$cmd" "$prepareimage" header -m "$magic" -k  "$ecckey"  -p 1 -r 44 -v "$version"  -f "$bin" -t "$sign" -o "$offset" "$headerbin
    $command >> $projectdir"/output.txt"
    ret=$?
    if [ $ret -eq 0 ]; then
      command=$cmd" "$prepareimage" merge -v 0 -e 1 -i "$headerbin" -s "$sbsfuelf" -u "$elf" "$bigbinary
      $command >> $projectdir"/output.txt"
      ret=$?
      #Partial image generation if reference userapp exists
      if [ $ret -eq 0 ] && [ -e "$ref_userapp" ]; then
        echo "Generating the partial image .sfb"
        echo "Generating the partial image .sfb" >> $projectdir"/output.txt"
        command=$cmd" "$prepareimage" diff -1 "$ref_userapp" -2 "$bin" "$partialbin" -a "$alignment" --poffset "$partialoffset
        $command >> $projectdir"/output.txt"
        ret=$?
        if [ $ret -eq 0 ]; then
          command=$cmd" "$prepareimage" sha256 "$partialbin" "$partialsign
          $command >> $projectdir"/output.txt"
          ret=$?
          if [ $ret -eq 0 ]; then
            command=$cmd" "$prepareimage" pack -m "$magic" -k "$ecckey" -p 1 -r 44 -v "$version" -f "$bin" -t "$sign" -o "$offset" --pfw "$partialbin" --ptag "$partialsign" --poffset  "$partialoffset" "$partialsfb
            $command >> $projectdir"/output.txt"
            ret=$?
          fi
        fi
      fi
      if [ $ret -eq 0 ] && [ $# = 6 ]; then
        echo "Generating the global elf file (SBSFU and userApp)"
        echo "Generating the global elf file (SBSFU and userApp)" >> $projectdir"/output.txt"
        uname | grep -i -e windows -e mingw > /dev/null 2>&1
        if [ $? -eq 0 ]; then
          # Set to the default installation path of the Cube Programmer tool
          # If you installed it in another location, please update PATH.
          PATH="C:\\Program Files (x86)\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin":$PATH > /dev/null 2>&1
          programmertool="STM32_Programmer_CLI.exe"
        else
          which STM32_Programmer_CLI > /dev/null
          if [ $? = 0 ]; then
            programmertool="STM32_Programmer_CLI"
          else
            echo "fix access path to STM32_Programmer_CLI"
          fi
        fi
        command=$programmertool" -ms "$elf" "$headerbin" "$sbsfuelf
        $command >> $projectdir"/output.txt"
        ret=$?
      fi
    fi
  fi
fi

if [ $ret -eq 0 ]; then
  rm $sign
  rm $headerbin
  if [ -e "$ref_userapp" ]; then
    rm $partialbin
    rm $partialsign
    rm $partialoffset
  fi  
  exit 0
else 
  echo "$command : failed" >> $projectdir"/output.txt"
  if [ -e  "$elf" ]; then
    rm  $elf
  fi
  if [ -e "$elfbackup" ]; then 
    rm  $elfbackup
  fi
  echo $command : failed
  read -n 1 -s
  exit 1
fi
