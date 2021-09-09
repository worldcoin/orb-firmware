# Copyright(c) 2018 STMicroelectronics International N.V.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import os
from array import array
#generating execute only code function for ARMV7M
#based on :
# 0xABCD
#    MOVW Rn, #0xBA
#    MOVT Rn  #0xCD
# AREA KEY, CODE
# EXPORT ReadKey
# 16 bytes AEG_GCM key
#PUSH {R4, R7}
# 32 bytes (pub_x, pub_y)
#PUSH {R4, R11}
def build_mov(reg, val, version):
    if version == "V7M":
        #build the 2 16bits to write
        out ="\tMOVW R"+str(reg)+", #"+hex(val[1]*256+val[0])+"\n"
        out +="\tMOVT R"+str(reg)+", #"+hex(val[3]*256+val[2])+"\n"
    elif version == "V6M":    
        #build the 8 8bits to write
        out ="\tMOVS R"+str(reg)+", #"+hex(val[3])+"\n"
        out +="\tLSLS R"+str(reg)+", R"+str(reg)+", #24\n"
        out +="\tMOVS R5, #"+hex(val[2])+"\n"
        out +="\tLSLS R5, R5, #16\n"
        out +="\tADD R"+str(reg)+", R"+str(reg)+", R5\n"
        out +="\tMOVS R5, #"+hex(val[1])+"\n"
        out +="\tLSLS R5, R5, #8\n"
        out +="\tADD R"+str(reg)+", R"+str(reg)+", R5\n"
        out +="\tMOVS R5, #"+hex(val[0])+"\n"
        out +="\tADD R"+str(reg)+", R"+str(reg)+", R5\n"
    else:
        exit(1)    
    return out

def translate(key, end=False, assembly="IAR", version="V7M"):
    if version == "V7M":        
        STMR0 = "\tSTM R0"
        ADDR0 = "\tADD R0"
    elif version == "V6M":    
        STMR0 = "\tSTM R0!"
    else:
        exit(1)

    key=bytearray(key)
    if len(key) <= 16:
        endreg = int((len(key)+3)/4)
        loop = 1
    elif len(key) == 32:
        endreg = int(4)
        loop = 2
    elif len(key) == 64:
        endreg = int(4)
        loop = 4
    else:
        return ""
    output = "\tPUSH {R1-R5}\n"
    for j in range(0,loop):
        if j!=0:
            output+= STMR0 +", {R1-R"+str(endreg)+"}\n"
            if version == "V7M":
                output+= ADDR0 +", R0,#16\n"
        for i in range(0,endreg):     
            output+=build_mov(i+1, key[16*j+i*4:16*j+i*4+4], version)
    output += STMR0 + ", {R1-R"+str(endreg)+"}\n"
    output += "\tPOP {R1-R5}\n"
    output += "\tBX LR\n"
    if end:
        if assembly == "GNU":
            output +="\t.end"
        else:
            output +="\tEND"
    return output

def function(section, name, assembly="IAR" ):
    if assembly == "IAR":
      section_name="section "
      separator=":CODE\n"
    elif assembly == "ARM":
      section_name="AREA |"
      separator="|, CODE\n"
    elif assembly == "GNU":
      section_name=".section "
      separator=""","a",%progbits\n .syntax unified \n .thumb \n
      """
    else:
      exit(1)
    if section !="":
        out = "\t"+str(section_name)+str(section)+str(separator)
    else:
        out=""
    if assembly == "GNU":
        out += "\t.global "+str(name)+"\n"
        out += str(name)+":\n"
    else:
        out += "\tEXPORT "+str(name)+"\n"
        out += str(name)+"\n"
    return out
if __name__ == '__main__':
    assembly = "GNU"
    outcode = function(".SE_Key_Data", "SE_ReadKey")
    #key = os.urandom(32)
    key="OEM_KEY_COMPANY1".encode()
    outcode +=translate(key, version="V6M")
    key = bytearray([0xce, 0x40, 0x14, 0xc6, 0x88, 0x11, 0xf9, 0xa2, 0x1a, 0x1f, 0xdb, 0x2c, 0x0e,
    0x61, 0x13, 0xe0, 0x6d, 0xb7, 0xca, 0x93, 0xb7, 0x40, 0x4e, 0x78, 0xdc, 0x7c,
    0xcd, 0x5c, 0xa8, 0x9a, 0x4c, 0xa9])
    outcode += function("", "SE_ReadKey_PubY")
    outcode += translate(key, version="V6M")
    outcode += function("", "SE_ReadKey_PubX")
    key = bytearray([ 0x1c, 0xcb, 0xe9, 0x1c, 0x07, 0x5f, 0xc7, 0xf4, 0xf0, 0x33, 0xbf, 0xa2, 0x48,
    0xdb, 0x8f, 0xcc, 0xd3, 0x56, 0x5d, 0xe9, 0x4b, 0xbf, 0xb1, 0x2f, 0x3c, 0x59,
    0xff, 0x46, 0xc2, 0x71, 0xbf, 0x83])

    outcode +=translate(key,version="V6M")
    if assembly == "GNU":
      outcode +="\t.end"
    else:
      outcode +="\tEND"
    print(outcode)
#bytearray([0x13, 0x00, 0x00, 0x00, 0x08, 0x00])
#key = bytearray([0xaa, 0xbb, 0xcc, 0xdd])
    print(str(key))



