#!/bin/bash - 
#prebuild script 
echo prebuild.sh : started > $1/output.txt
asmfile=$1/se_key.s
# comment this line to force python
# python is used if  executable not found
current_directory=`pwd`
cd $1/components/Middlewares/STM32_Secure_Engine/Utilities/KeysAndImages
basedir=`pwd`
cd $current_directory

# test if window executable usable
prepareimage=$basedir/win/prepareimage/prepareimage.exe
uname  | grep -i -e windows -e mingw > /dev/null 2>&1

if [ $? -eq 0 ] && [  -e "$prepareimage" ]; then
  echo "prepareimage with windows executable" >> $1/output.txt
  echo "prepareimage with windows executable"
  cmd=""
else
  # line for python
  echo "prepareimage with python script" >> $1/output.txt
  echo "prepareimage with python script"
  prepareimage=$basedir/prepareimage.py
  cmd=python
fi

echo "$cmd $prepareimage" >> $1/output.txt
crypto_h=$1/../Inc/se_crypto_config.h

#clean
if [ -e "$1/crypto.txt" ]; then
  rm $1"/crypto.txt"
fi

if [ -e "$asmfile" ]; then
  rm $asmfile
fi

if [ -e "$1"/postbuild.sh ]; then
  rm $1"/postbuild.sh"
fi

#get crypto name
command="$cmd $prepareimage conf "$crypto_h""
echo $command
crypto=`$command`
echo $crypto > $1"/crypto.txt"
echo "$crypto selected">> $1"/output.txt"
echo $crypto selected
ret=$?

cortex="V7M"
echo "	.section .SE_Key_Data,\"a\",%progbits" > $asmfile
echo "	.syntax unified" >> $asmfile
echo "	.thumb " >> $asmfile

# AES keys part
if [ $ret -eq 0 ]; then
  type="vide"
  if [ $crypto = "SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM" ]; then
    type="GCM"
  fi  
  if [ $crypto = "SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256" ]; then
    type="CBC"
  fi

  if [ $type != "vide" ]; then
    oemkey="$1/../Binary/OEM_KEY_COMPANY1_key_AES_$type.bin"
    command=$cmd" "$prepareimage" trans -a GNU -k "$oemkey" -f SE_ReadKey_1 -v "$cortex
    echo $command
    $command >> $asmfile
    ret=$?
        
    if [ $ret -eq 0 ]; then
      oemkey="$1/../Binary/OEM_KEY_COMPANY2_key_AES_$type.bin"
      if [ -e "$oemkey" ]; then
        command=$cmd" "$prepareimage" trans -a GNU -k "$oemkey" -f SE_ReadKey_2 -v "$cortex
        echo $command
        $command >> $asmfile
        ret=$?
      fi
    fi

    if [ $ret -eq 0 ]; then
        oemkey="$1/../Binary/OEM_KEY_COMPANY3_key_AES_$type.bin"
        if [ -e "$oemkey" ]; then
            command=$cmd" "$prepareimage" trans -a GNU -k "$oemkey" -f SE_ReadKey_3 -v "$cortex
            echo $command
            $command >> $asmfile
            ret=$?
        fi
    fi
  fi
fi

# ECC keys part
if [ $ret -eq 0 ]; then
  type="vide"
  if [ $crypto = "SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256" ]; then
    type="ECC"
  fi  
  if [ $crypto = "SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256" ]; then
    type="ECC"
  fi

  if [ $type != "vide" ]; then
    ecckey=$1"/../Binary/ECCKEY1.txt"
    command=$cmd" "$prepareimage" trans  -a GNU -k "$ecckey" -f SE_ReadKey_1_Pub -v "$cortex
    echo $command
    $command >> $asmfile
    ret=$?

    if [ $ret -eq 0 ]; then
      ecckey=$1"/../Binary/ECCKEY2.txt"
      if [ -e "$ecckey" ]; then
        command=$cmd" "$prepareimage" trans  -a GNU -k "$ecckey" -f SE_ReadKey_2_Pub -v "$cortex
        echo $command
        $command >> $asmfile
        ret=$?
      fi
    fi

    if [ $ret -eq 0 ]; then
      ecckey=$1"/../Binary/ECCKEY3.txt"
      if [ -e "$ecckey" ]; then
        command=$cmd" "$prepareimage" trans  -a GNU -k "$ecckey" -f SE_ReadKey_3_Pub -v "$cortex
        echo $command
        $command >> $asmfile
        ret=$?
      fi
    fi
  fi
fi
echo "    .end" >> $asmfile

if [ $ret -eq 0 ]; then
#no error recopy post build script
    uname  | grep -i -e windows -e mingw > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "recopy postbuild.sh script with "$crypto".sh script"
        command="cat "$1"/"$crypto".sh"
        $command > "$1"/"postbuild.sh"
        ret=$?
    else
        echo "create symbolic link postbuild.sh to "$crypto".sh"
        command="ln -s ./"$crypto".sh "$1"/postbuild.sh"
        $command
        ret=$?
    fi
fi

if [ $ret != 0 ]; then
#an error
echo $command : failed >> $1/output.txt 
echo $command : failed
read -n 1 -s
exit 1
fi
exit 0

