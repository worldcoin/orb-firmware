# Copyright(c) 2018 STMicroelectronics International N.V.
# Copyright 2017 Linaro Limited
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
#
import keys
import sys
import argparse
import os
import hashlib
import numpy
import elftools
#import string
from elftools.elf.elffile import ELFFile
from struct import pack

def gen_ecdsa_p256(args):
    keys.ECDSA256P1.generate().export_private(args.key)

def gen_aes_gcm(args):
    keys.AES_GCM.generate().export_private(args.key)

def gen_aes_cbc(args):
    keys.AES_CBC.generate().export_private(args.key)

def gen_aes_ctr(args):
    keys.AES_CTR.generate().export_private(args.key)


keygens = {
        'aes-gcm': gen_aes_gcm,
        'aes-cbc': gen_aes_cbc,
        'aes-ctr': gen_aes_ctr,
        'ecdsa-p256': gen_ecdsa_p256,
        }

def do_keygen(args):
    if args.type not in keygens:
        msg = "Unexpected key type: {}".format(args.type)
        raise argparse.ArgumentTypeError(msg)
    keygens[args.type](args)

def do_trans(args):
    key = keys.load(args.key)
    end = False
    if args.end:
        end = True
    if args.assembly=="ARM" or args.assembly=="IAR" or args.assembly=="GNU":
        if args.version=="V6M" or args.version=="V7M": 
          out = key.trans(args.section, args.function, end, args.assembly, args.version)
          print (str(out))
        else:
          print ("-v option : Cortex M architecture not supported")
          exit(1)
    else:
        print ("-a option : assembly option not supported")
        exit(1)

def do_getpub(args):
    key = keys.load(args.key)
    key.emit_c()

def do_sign(args):
    payload = []
    with open(args.infile, 'rb') as f:
        payload = f.read()
    # Removing the padding because the hashlib pads properly (512 bit alignment) for SHA256
    # payload = pad(payload)
    key = keys.load(args.key) if args.key else None
    if key.has_sign():
        if key.has_nonce():
            nonce = []
            try:
                with open(args.nonce, 'rb') as f:
                    nonce = f.read()
            except:
                nonce= []
            encrytpted, signature , nonce_used = key.encrypt( payload, nonce)
            if nonce !=nonce_used:
                try:
                    f = open(args.nonce, 'wb')
                    f.write(nonce_used)
                    f.close()
                except:
                    print("nonce filename required")
                    exit(1)
        else:
            signature  = key.sign(payload)
        f = open(args.outfile,"wb")
        f.write(signature)
        f.close()
    else:
        print("Provided Key is not usable to sign ")
        exit(1)


def do_sha(args):
    payload = []
    with open(args.infile, 'rb') as f:
        payload = f.read()
    m = hashlib.sha256()
    buffer=payload
    m.update(buffer)
    signature = m.digest()
    f = open(args.outfile, "wb")
    f.write(signature)
    f.close()

def do_encrypt(args):
    payload = []
    with open(args.infile, 'rb') as f:
        payload = f.read()
        f.close()
    key = keys.load(args.key) if args.key else None
    if key.has_encrypt():
        if key.has_nonce():
            nonce = []
            if args.nonce and args.iv:
                print("either IV or Nonce Required for this key!!!")
                exit(1)
            if args.nonce:
                iv_nonce=args.nonce
            elif args.iv:
                iv_nonce=args.iv
            else:
                print("either IV or Nonce Required for this key!!!")
                exit(1)
            if os.path.isfile(iv_nonce):
                with open(iv_nonce, 'rb') as f:
                    nonce = f.read()
            if args.poffset:
                with open(args.poffset, 'r') as f:
                  offset = int(f.read())
                # AES CTR : remove 4 LSB bits to the address
                offset = int(offset / 16)
            else:
                offset = 0
            if args.address:
                address = args.address + offset
                encrypted , signature , nonce_used = key.encrypt(payload, address, nonce)
            else:
                encrypted , signature , nonce_used = key.encrypt(payload, nonce)           
            if nonce !=nonce_used:
                f = open(iv_nonce, 'wb')
                f.write(nonce_used)
                f.close()
        else:
            encrypted ,signature = key.encrypt(payload)

        f=open(args.outfile,"wb")
        f.write(encrypted)
        f.close()
    else:
        print("Key does not support encrypt")
        exit(1)

def do_header_lib(args):
    if (os.path.isfile(args.firmware)):
        size = os.path.getsize(args.firmware)
    else:
        print("no fw file")
        exit(1)
    if os.path.isfile(args.tag):
        f=open(args.tag, 'rb')
        tag = f.read()
    if args.cert_fw_leaf :
      if os.path.isfile(args.cert_fw_leaf):
          print("Loading cert: "+args.cert_fw_leaf)
          f=open(args.cert_fw_leaf, 'rb')
          cert_fw_leaf = f.read()
    if args.cert_fw_inter :
      if os.path.isfile(args.cert_fw_inter):
          print("Loading cert: "+args.cert_fw_inter)
          f=open(args.cert_fw_inter, 'rb')
          cert_fw_inter = f.read()
    
    key = keys.load(args.key) if args.key else None
    #empty nonce
    nonce = b''
    protocol = args.protocol
    magic = args.magic.encode()
    version = args.version
    reserved = b'\0'*args.reserved
    if args.nonce and args.iv:
        print("either IV or Nonce Required !!!")
        exit(1)
    iv_nonce=""
    if args.nonce:
        iv_nonce=args.nonce
    elif args.iv:
        iv_nonce=args.iv
    if iv_nonce:
        with open(iv_nonce, 'rb') as f:
            nonce = f.read()
    if args.pfw:
        pfwsize = os.path.getsize(args.pfw)
    else:
        pfwsize = size
    if args.poffset:
        with open(args.poffset, 'r') as f:
            pfwoffset = int(f.read())
    else:
        pfwoffset = 0
    if args.ptag:
        f=open(args.ptag, 'rb')
        pfwtag = f.read()
        f.close()
    else:
        pfwtag=tag
    print("Magic: "+str(magic)+"!!")
    header = pack('<'+str(len(magic))+'sHHIII'+str(len(tag))+'s'+str(len(pfwtag))+'s'+str(len(nonce))+'s'+str(args.reserved)+'s',
                    magic, protocol, version, size, pfwoffset, pfwsize, tag, pfwtag, nonce, reserved)
    #add certs
    # Add certificates
    if args.cert_fw_leaf :
        print("adding leaf cert of length "+str(str(len(cert_fw_leaf))))
        header +=pack(str(len(cert_fw_leaf))+'s',cert_fw_leaf)
    if args.cert_fw_inter :
        print("adding intermediate cert of length "+str(str(len(cert_fw_inter))))
        header +=pack(str(len(cert_fw_inter))+'s',cert_fw_inter)
    # Pad, leaving 64 bytes for signature + 3*32 bytes for image state + 32 Fingerprint
    if (args.cert_fw_leaf or args.cert_fw_inter):
        if 'pack' in args.subcmd:
            padding = args.offset - (len(header) + 3*32 + 32 + 64)
        else:
            padding = args.offset - (len(header) + 3*32 + 32 + 64)
        if (padding<0):
            print("Invalid padding: "+str(padding))
            exit(1)
        else:
            print("Adding padding: "+str(padding))
        header += b'\0'*padding
    #GCM needs Nonce to sign
    #AES cannot sign
    if key.has_sign():
        if key.has_nonce() and iv_nonce=="":
            print("sign key required nonce, provide nonce")
            exit(1)
        if key.has_nonce():
            if nonce != b'':
                signature , nonce_used = key.sign(header, nonce)
                if nonce_used !=nonce:
                    print("error nonce used differs")
                    exit(1)
            else:
                print("nonce required for this key")
                exit(1)
        else:
            signature = key.sign(header)
        header +=pack(str(len(signature))+'s',signature)
    else:
        print("Provided Key is not usable to sign header")
        exit(1)

    if 'pack' in args.subcmd:
        # image state = new
        header += b'\xFF'*96
    else:
        # image state = valid
        header += b'\xFF'*32
        header += b'\x00'*64
    # UpdateSourceFingerprint - init to 0x00
    header += b'\x00'*32
    return header, signature

#header is used only to build install header for merge .elf tool
def do_header(args):
    header ,signature  = do_header_lib(args)
    f=open(args.outfile,"wb")
    if len(signature) == 16:
        signature = 2*signature
    elif len(signature) == 64:
        signature = signature[0:32]
    else:
        print("Unexpected signature size : "+str(len(signature))+"!!")
    f.write(header)
    padding = args.offset - (len(header))
    while (padding != 0):
        f.write(b'\xff')
        padding = padding - 1
    f.close()

def do_conf(args):
    if (os.path.isfile(args.infile)):
        f = open(args.infile)
        for myline in f:
            if args.define in myline:
                myword = myline.split()
                if myword[1] == args.define:
                    print(myword[2])
                    f.close()
                    return
        f.close()
        print("#DEFINE "+args.define+" not found")
        exit(1)

def do_extract(args):
    if (os.path.isfile(args.infile)):
        f = open(args.infile)
        for myline in f:
            if args.define in myline:
                myword = myline.split('0x')
                print('0x'+ myword[1][:8])
                f.close()
                return
        f.close()
        print(args.define+" not found")
        exit(1)

def do_pack(args):
    header,signature = do_header_lib(args)
    f=open(args.outfile,"wb")
    f.write(header)
    if len(header) > args.offset:
        print("error header is larger than offset before binary")
        sys.exit(1)

    #create the string to padd
    tmp = (args.offset-len(header))*b'\xff'
    #write to file
    f.write(tmp)
    if args.pfw:
        #recopy encrypted partial file
        binary=open(args.pfw,'rb')
    else:
        #recopy encrypted complete file
        binary=open(args.firmware,'rb')
    tmp=binary.read()
    f.write(tmp)
    binary.close()
#find lowest sction to fix base address not matching
def find_lowest_section(elffile):
     lowest_addr = 0
     lowest_size = 0
     for s in elffile.iter_sections():
        if (s.header['sh_flags'] & elftools.elf.constants.SH_FLAGS.SHF_ALLOC) and (s.header.sh_type != 'SHT_NOBITS'):
            sh_addr =  s.header['sh_addr'];
            if lowest_addr == 0:
                lowest_addr = sh_addr
            elif sh_addr < lowest_addr:
                lowest_addr = sh_addr
                lowest_size = s.header['sh_size']
     return lowest_addr, lowest_size

#return base address of segment, and a binary array,
#add padding pattern
def get_binary(elffile,pad=0xff, elftype=0):
    num = elffile.num_segments()
    print("number of segment :"+str(num))
    for i in range(0,num):
        segment= elffile.get_segment(i)
        if segment['p_type'] == 'PT_LOAD':
          if i!=0:
            print(hex(nextaddress))
            if (len(segment.data())):
                padd_size=segment.__getitem__("p_paddr")- nextaddress
                binary+=padd_size*pack("B",pad) 
                binary+=segment.data()
                nextaddress = segment.__getitem__("p_paddr") + len(segment.data())
          else:
            binary=segment.data()
            if elftype == 0:
              base_address =  segment.__getitem__("p_paddr")
            else:
              base_address , lowest_size =  find_lowest_section(elffile)
              offset = base_address - segment.__getitem__("p_paddr")
              binary = binary[offset:]
            nextaddress = base_address + len(binary)
    return binary, base_address

def do_merge(args):
    #get the different element to compute the big binary
    if args.userapp:
      with open(args.userapp, 'rb') as f:
          # get the data
          my_elffile = ELFFile(f)
          appli_binary, appli_base = get_binary(my_elffile, args.value,args.elf)
    with open(args.sbsfu, 'rb') as f:
        my_elffile = ELFFile(f)
        sbsfu_binary, sbsfu_base = get_binary(my_elffile, args.value, args.elf)
    with open(args.install, 'rb') as f:
        header_binary = f.read()
    if args.loader:
        with open(args.loader, 'rb') as f:
            # get the data
            my_elffile = ELFFile(f)
            loader_binary, loader_base = get_binary(my_elffile, args.value,args.elf)
    #merge the three or four elements and padd in between , add some extra byte to
    #appli for aes cbc support
    address_just_after_sbsfu = len(sbsfu_binary)+sbsfu_base
    if args.loader:
        beginaddress_loader = loader_base
        address_just_after_loader = len(loader_binary)+loader_base
    if args.header:
      beginaddress_header = args.header
    else:
      beginaddress_header = appli_base - len(header_binary)
    if args.loader and (beginaddress_loader < address_just_after_sbsfu):
        print("sbsfu is too large to merge with loader !!")
        exit(2)
    elif args.loader and (beginaddress_header < address_just_after_loader):
        print("loader is too large to merge with appli !!")
        exit(3)
    else:
        print("Merging")
        print("SBSFU Base = "+hex(sbsfu_base))
        if args.loader:
            print("Loader Base = "+hex(loader_base))
        print("Writing header = "+hex(beginaddress_header))
        if args.userapp:
            print("APPLI Base = "+hex(appli_base))
        if args.loader:
            padd_before_loader = beginaddress_loader - address_just_after_sbsfu
            padd_before_header =   beginaddress_header - address_just_after_loader
            big_binary = sbsfu_binary + padd_before_loader * pack("B",args.value)+ loader_binary + padd_before_header * pack("B",args.value) + header_binary + appli_binary
        elif args.header:
            if args.userapp:
                # header not contiguous : SBSFU + pad + header + pad + header(fake) + UserApp
                padd_before_header = beginaddress_header - address_just_after_sbsfu
                padd_before_appli = appli_base - len(header_binary) - padd_before_header - len(header_binary) - address_just_after_sbsfu
                big_binary = sbsfu_binary + padd_before_header * pack("B",args.value) + header_binary + padd_before_appli * pack("B",args.value) + header_binary + appli_binary
            else:
                # header not contiguous : SBSFU + header + SBSFU (loader part)
                offset_header = beginaddress_header - sbsfu_base   
                big_binary = sbsfu_binary[:offset_header] + header_binary + sbsfu_binary[offset_header + len(header_binary):]
        else:
            padd_before_header =   beginaddress_header - address_just_after_sbsfu
            big_binary = sbsfu_binary + padd_before_header * pack("B",args.value) + header_binary + appli_binary
    
    print("Writing to "+str(args.outfile)+" "+str(len(big_binary)))
    with open(args.outfile, 'wb') as f:
        f.write(big_binary)

def sub_mergev2(args, bb_dict, bin, bin_base, bin_end):
    #print("Current BB : "+hex(bb_dict["bb_base"])+" > "+hex(bb_dict["bb_end"]))
    #print("Add segment: "+hex(bin_base)+" > "+hex(bin_end))
    # injection
    # 6 cases:
    #  Elf is strictly inside BB range  => First check there is no useful data in BB range that will be erased by elf file, then copy elf contents into BB
    if (bin_base > bb_dict["bb_base"]) and (bin_end < bb_dict["bb_end"]):
        # Check overlap data
        overlapzone = bb_dict["big_binary"][bin_base-bb_dict["bb_base"]+1:(bin_base-bb_dict["bb_base"])+len(bin)-1]
        for b in overlapzone:
          if b != args.value:
            print("ERROR: overlapped zone is not empty")
            exit(3)
        # Create new BB
        bb_dict["big_binary"] = bb_dict["big_binary"][:bin_base-bb_dict["bb_base"]] + bin + bb_dict["big_binary"][(bin_base-bb_dict["bb_base"])+len(bin):]
        # bb_dict["bb_base"] and bb_dict["bb_end"] do not change
    #  Elf is before BB range  => No issue, paste both with padding in between
    elif (bin_end <= bb_dict["bb_base"]):
        bb_dict["big_binary"] = bin + (bb_dict["bb_base"] - bin_end) * pack("B",args.value) + bb_dict["big_binary"]
        bb_dict["bb_base"] = bin_base
        # bb_dict["bb_end"] do not change
    #  Elf is after BB range   => No issue, paste both with padding in between
    elif (bin_base >= bb_dict["bb_end"]):
        bb_dict["big_binary"] = bb_dict["big_binary"] + (bin_base - bb_dict["bb_end"]) * pack("B",args.value) + bin
        bb_dict["bb_end"] = bin_end
        # bb_start do not change
    #  Elf starts before BB start and ends inside BB range => !!! abnormal case not supported !!!
    elif (bin_base <= bb_dict["bb_base"]) and (bin_end <= bb_dict["bb_end"]):
        print("ERROR current binary is overlapping with previously aggregated binaries !!")
        exit(2)
    #  Elf starts inside BB range and ends after BB end    => !!! abnormal case not supported !!!
    elif (bin_base >= bb_dict["bb_base"]) and (bin_end >= bb_dict["bb_end"]):
        print("ERROR current binary is overlapping with previously aggregated binaries !!")
        exit(2)
    #  Elf starts before BB start and ends after BB end  => First check there is no useful data in ELF file range that will be erased by BB, then copy BB contents into elf file binary that becomes new BB
    elif (bin_base < bb_dict["bb_base"]) and (bin_end > bb_dict["bb_end"]):
        # Check overlap data
        overlapzone = bin[bb_dict["bb_base"]-bin_base+1:(bb_dict["bb_base"]-bin_base)+len(bb_dict["big_binary"])-1]
        for b in overlapzone:
          if b != args.value:
            print("ERROR: overlapped zone is not empty")
            exit(3)
        # Create new BB
        bb_dict["big_binary"] = bin[:bb_dict["bb_base"]-bin_base] + bb_dict["big_binary"] + bin[(bb_dict["bb_base"]-bin_base)+len(bb_dict["big_binary"]):]
        bb_dict["bb_base"] = bin_base
        bb_dict["bb_end"] = bin_end
    else:
        print("ERROR segment configuration is generating not managed case !!")
        exit(99)

def do_mergev2(args):
    bb_dict=dict()
    # loop on elf files
    if args.files:
      elffiles = args.files.split(';')
      for ef in elffiles:
        with open(ef, 'rb') as f:
            # get the data
            my_elffile = ELFFile(f)
            elf_binary, elf_base = get_binary(my_elffile, args.value,args.elf)
            elf_end = elf_base + len(elf_binary)
            print("Adding ELF file: "+ef+" from "+hex(elf_base)+" to "+hex(elf_end))
            # check if first file or injection needed
            if 'big_binary' in bb_dict:
              sub_mergev2(args, bb_dict, elf_binary, elf_base, elf_end)
            else:
              # first copy
              bb_dict['big_binary'] = elf_binary
              bb_dict['bb_base'] = elf_base
              bb_dict['bb_end'] = elf_end
    # loop on binary files
    binfiles = args.binaries.split(';')
    for bf in binfiles:
      #print(bf)
      _bf, _add = bf.split('@')
      with open(_bf, 'rb') as f:
          # get the data
          bin_binary = f.read()
          bin_base = auto_int(_add) & 0xffffffff
          bin_end = bin_base + len(bin_binary)
          print("Adding BIN file: "+_bf+" from "+hex(bin_base)+" to "+hex(bin_end))
          # check if first file or injection needed
          if 'big_binary' in bb_dict:
            sub_mergev2(args, bb_dict, bin_binary, bin_base, bin_end)
          else:
            # first copy
            bb_dict['big_binary'] = bin_binary
            bb_dict['bb_base'] = bin_base
            bb_dict['bb_end'] = bin_end

    print("Writing to "+str(args.outfile)+" "+str(len(bb_dict['big_binary'])))
    with open(args.outfile, 'wb') as f:
        f.write(bb_dict['big_binary'])
    with open(args.outfile+".baseadd", 'w') as f:
        f.write(hex(bb_dict['bb_base']))


def do_append(args):
    #get the different element to compute the binary
    with open(args.userapp, 'rb') as f:
        my_elffile = ELFFile(f)
        appli_binary, appli_base = get_binary(my_elffile, args.value,args.elf)
    with open(args.binary, 'rb') as f:
        input_binary = f.read()
    with open(args.install, 'rb') as f:
        header_binary = f.read()

    #Add header + user application to the existing binary and padd in between
    input_binary_base = args.address
    address_just_after_input_binary = len(input_binary) + input_binary_base
    if args.header:
      beginaddress_header = args.header
    else:
      beginaddress_header = appli_base - len(header_binary)
    print("Merging")
    print("Binary base = "+hex(input_binary_base))
    print("Writing header = "+hex(beginaddress_header))
    print("APPLI base = "+hex(appli_base))
    if beginaddress_header < address_just_after_input_binary:
        # header not contiguous : input binary + header + input binary (remaining part) + pad + header(fake) + UserApp
         padd_before_appli = appli_base - len(header_binary) - address_just_after_input_binary
         offset_header = beginaddress_header - input_binary_base
         output_binary = input_binary[:offset_header] + header_binary + input_binary[offset_header + len(header_binary):] + padd_before_appli * pack("B",args.value) + header_binary + appli_binary
    else:
        # header not contiguous : input binary + pad + header + UserApp
         padd_before_appli = appli_base - len(header_binary) - address_just_after_input_binary
         output_binary = input_binary + + padd_before_appli * pack("B",args.value) + header_binary + appli_binary
    print("Writing to "+str(args.outfile)+" "+str(len(output_binary)))
    with open(args.outfile, 'wb') as f:
        f.write(output_binary)

def do_diff(args):
    # Check args
    if args.align == 0:
        msg = "Wrong alignment value, must be greater than 0"
        raise argparse.ArgumentTypeError(msg)
    if args.begin % args.align > 0:
        msg = "Wrong begin value ({}), must be a modulo of the specified alignment ({})".format(args.begin, args.align)
        raise argparse.ArgumentTypeError(msg)
    if args.end > 0 and (args.end + 1) % args.align > 0:
        msg = "Wrong end value ({}), must be last offset of a block defined by alignment ({})".format(args.end, args.align)
        raise argparse.ArgumentTypeError(msg)

    # Append the file 1 to get a size modulo alignment 
    arr1 = numpy.fromfile(args.file1, numpy.int8)
    size = args.align - (arr1.size % args.align)
    for ite in range(0, size):
        arr1 = numpy.append(arr1, [0])
    arr1 = arr1.reshape(-1, args.align)

    # Append the file 2 to get a size modulo alignment 
    arr2 = numpy.fromfile(args.file2, numpy.int8)
    arr2size = arr2.size
    size = args.align - (arr2.size % args.align)
    for ite in range(0, size):
        arr2 = numpy.append(arr2, [0])
    arr2 = arr2.reshape(-1, args.align)
       
    if args.end > 0 and args.end >= arr2.size:
        msg = "Wrong end set ({}), must be a within secondary binary file range ({})".format(args.end, arr2.size)
        raise argparse.ArgumentTypeError(msg)
    
    if arr1.size == arr2.size:
        # Both files to compare have same size
        if args.end > 0 and args.end < arr1.size:
            # args.end is specified within file size
            end_cmp = args.end
        else:
            # args.end is not specified or is out of file size
            end_cmp = arr1.size - 1
        # No additional data, copy stops at end of comparison
        end_cpy = -1
    elif arr1.size > arr2.size:
        # Primary file bigger than secondary
        if args.end > 0 and args.end < arr2.size:
            # args.end is specified within secondary file size
            end_cmp = args.end
        else:
            # args.end is not specified or is out of secondary file size
            end_cmp = arr2.size - 1
        # No additional data, copy stops at end of comparison
        end_cpy = -1
    else:  # arr2.size > arr1.size
        if args.end > 0 and args.end < arr1.size:
            # args.end is specified within primary file size
            end_cmp = args.end
            # No additional data, copy stops at end of comparison
            end_cpy = -1
        else:
            # args.end is not specified or is out of primary file size
            end_cmp = arr1.size - 1
            if args.end > 0 and args.end < arr2.size:
                # Additional data to copy till specified args.end
                end_cpy = args.end
            else:
                # Additional data to copy till end of secondary file
                end_cpy = arr2.size - 1

    first_diff = -1
    last_diff = -1
    for ite in range(int(args.begin/args.align), int(end_cmp/args.align)+1):
        if not numpy.array_equal(arr1[ite], arr2[ite]):
            if first_diff == -1:
                first_diff = ite
                if end_cpy == -1:
                    last_diff = ite
                else:
                    # Additional data to copy, set last_diff to end of copy and exit loop
                    last_diff = int(end_cpy/args.align)
                    break
            else:
                last_diff = ite
    
    if first_diff == -1:
        if end_cpy == -1:
            sys.stderr.write("Input files are identical within comparison range ({},{})".format(args.begin, end_cmp))
            last_diff = -1
            first_diff = 0
        else:
            # No comparison differences detected, but additional data needs to be copied
            # Set first_diff & last_diff to first & last additional data to copy
            first_diff = int(arr1.size/args.align)
            last_diff = int(end_cpy/args.align)
    
    arrout = numpy.zeros((last_diff - first_diff + 1) * args.align, numpy.int8).reshape(-1, args.align)
    for ite in range(0, last_diff - first_diff + 1):
        arrout[ite] = arr2[first_diff + ite]
    
    # If last block concerned, remove the additional bytes appended at the beginning of the function
    arrout = arrout.reshape(1, -1)
    if ((last_diff + 1)* args.align) > arr2size :
        #last block is partial
        size = (last_diff - first_diff) * args.align + arr2size % args.align
    else:
        # all blocks are complete 
        size = (last_diff - first_diff + 1) * args.align
    # keep only the useful part
    arrout = arrout[0,:size]
    # Store to outfile binary file
    arrout.tofile(args.outfile)
    # Store offset in file if specified
    with open(args.poffset, 'w') as f:
        f.write(str(first_diff*args.align))

def do_inject(args):
    if args.key:
      key = keys.load(args.key)
      np_key = numpy.frombuffer(key.get_key(args.type), numpy.uint8)
    elif args.cert:
      np_key = numpy.fromfile(args.cert, numpy.uint8)
    else:
      raise argparse.ArgumentTypeError("Unknown error while parsing arguments")
    # DEBUG______________________________
    #print(key.get_key("public"))
    #l = list()
    #for ite in range(0, np_key.size):
    #  l.append(np_key[ite])
    #print(l)
    # DEBUG______________________________
    with open(args.file, 'r') as f:
      with open(args.outfile, 'w') as o:
        for line in f.readlines():
          # DEBUG______________________________
          #print(line)
          # DEBUG______________________________
          if args.pattern in line:
            # DEBUG______________________________
            #print("Found:{}".format(line))
            # DEBUG______________________________
            if "CKA_VALUE" in line:
              value = "{},".format(np_key.size)
              for ite in range(0, int(np_key.size/4+0.75)):  # int() truncate to integer part. +0,75 so that is reman of the division exist, it will be truncated to next int value
                  if ite*4+3 < np_key.size:
                    value += " 0x{:02x}{:02x}{:02x}{:02x}U,".format(np_key[ite*4],np_key[ite*4+1],np_key[ite*4+2],np_key[ite*4+3])
                  elif ite*4+2 < np_key.size:
                    value += " 0x{:02x}{:02x}{:02x}U,".format(np_key[ite*4],np_key[ite*4+1],np_key[ite*4+2])
                  elif ite*4+1 < np_key.size:
                    value += " 0x{:02x}{:02x}U,".format(np_key[ite*4],np_key[ite*4+1])
                  else:
                    value += " 0x{:02x}U,".format(np_key[ite*4])
              # DEBUG______________________________
              #print("Value:{}".format(value))
              # DEBUG______________________________
              line = line.replace(args.pattern, value)
            elif "CKA_EC_POINT"  in line:
              np_key2 = numpy.zeros(np_key.size+3, numpy.uint8)
              for ite in range(0, np_key.size):
                np_key2[ite+3] = np_key[ite]
              np_key2[0] = 0x04;              # Octet string
              np_key2[1] = np_key.size + 1;   # Octet length
              np_key2[2] = 0x04;              # X9.62 uncompressed format
              value = "{},".format(np_key2.size)
              for ite in range(0, int(np_key2.size/4+0.75)):
                  if ite*4+3 < np_key2.size:
                    value += " 0x{:02x}{:02x}{:02x}{:02x}U,".format(np_key2[ite*4],np_key2[ite*4+1],np_key2[ite*4+2],np_key2[ite*4+3])
                  elif ite*4+2 < np_key2.size:
                    value += " 0x{:02x}{:02x}{:02x}U,".format(np_key2[ite*4],np_key2[ite*4+1],np_key2[ite*4+2])
                  elif ite*4+1 < np_key2.size:
                    value += " 0x{:02x}{:02x}U,".format(np_key2[ite*4],np_key2[ite*4+1])
                  else:
                    value += " 0x{:02x}U,".format(np_key2[ite*4])
              # DEBUG______________________________
              #print("Value:{}".format(value))
              # DEBUG______________________________
              line = line.replace(args.pattern, value)
            # DEBUG______________________________
            #print("Altered line:{}".format(line))
            # DEBUG______________________________
          o.write(line)
        o.close()

#    end = False
#    if args.end:
#        end = True
#    if args.assembly=="ARM" or args.assembly=="IAR" or args.assembly=="GNU":
#        if args.version=="V6M" or args.version=="V7M": 
#          out = key.trans(args.section, args.function, end, args.assembly, args.version)
#          print (str(out))
#        else:
#          print ("-v option : Cortex M architecture not supported")
#          exit(1)
#    else:
#        print ("-a option : assembly option not supported")
#        exit(1)

subcmds = {
        'keygen': do_keygen,
        'trans':do_trans,
        'getpub': do_getpub,
        #sign a binary with a givent key
        # nonce is required for aes gcm  , if nonce not existing the file is
        # created
        # -k keyfilename -n nonce file
        'sign': do_sign,
        #hash a file with sha256 and provide result in in a file
        'sha256': do_sha,
        #return define value from crpto configuration from .h file
        #define to search default SECBOOT_CRYPTO_SCHEME, -d
        'conf': do_conf,
        #define to search , -d
        'extract': do_extract,
        #encrypt binary file with provided key
        # -k -n
        'enc': do_encrypt,
        # give what to put  in header and provide the key to compute hmac
        # magic (4 bytes) required, -m
        # protocol version(2 bytes) required , -p
        # nonce optional , -n
        # fwversion (required) 2 bytes, -ver
        # fw file (to get the size)
        # fw tag  (file)
        # reserved size
        # key
        # offset default 512
        'header':do_header,
        # give what to pack a single file header
        # magic (4 bytes) required, -m
        # protocol version(2 bytes) required , -p
        # nonce optional , -n
        # fwversion (required) 2 bytes, -ver
        # fw file (to get the size)
        # fw tag  (file)
        # reserved size
        # key
        # offset default 512
        #
        'pack':do_pack,
        #
        'diff':do_diff,
        #merge appli.elf , header binary and sbsfu elf in a big binary
        #input file appli.elf
        #-h header file
        #-s sbsfu.elf
        #output file binary to merge
        #-v byte pattern to fill between the different segment default 0xff
        #-p padding length to add to appli binary
        'merge':do_merge,
        'mergev2':do_mergev2,
        'diff':do_diff,
        #append appli.elf and header binary to an existing big binary
        'append':do_append,
        #inject key into file by replacing pattern
        'inject':do_inject,
        }

def auto_int(x):
    if x.startswith("0x"):
        return int(x,16)
    else:
        return int(x)

def args():
    parser = argparse.ArgumentParser()
    subs = parser.add_subparsers(help='subcommand help', dest='subcmd')
    keygenp = subs.add_parser('keygen', help='Generate pub/private keypair')
    keygenp.add_argument('-k', '--key', metavar='filename', required=True)
    keygenp.add_argument('-t', '--type', metavar='type',
            choices=['aes-gcm', 'ecdsa-p256','aes-cbc','aes-ctr'],
            required=True)
    trans =  subs.add_parser('trans', help='translate key to execute only code')
    trans.add_argument('-k', '--key', metavar='filename', required=True)
    trans.add_argument('-f', '--function', type=str, required = True)
    trans.add_argument('-s', '--section', type=str, default="")
    trans.add_argument('-a', '--assembly',help='fix assembly type IAR or ARM or GNU', type=str,required = False, default="IAR")
    trans.add_argument('-v', '--version',help='fix CORTEX M architecture', type=str,required = False, default="V7M")   
    trans.add_argument('-e', '--end')

    getpub = subs.add_parser('getpub', help='Get public key from keypair')
    getpub.add_argument('-k', '--key', metavar='filename', required=True)

    sign = subs.add_parser('sign', help='Sign an image with a private key')
    sign.add_argument('-k', '--key', metavar='filename', required = True)
    sign.add_argument('-n', '--nonce', metavar='filename', required = False)
    sign.add_argument("infile")
    sign.add_argument("outfile")
    sha = subs.add_parser('sha256', help='hash a file with sha256')
    sha.add_argument("infile")
    sha.add_argument("outfile")
    sha.add_argument('-p', '--padding',type=int, required = False, default=0, help='pad to be a multiple of the given size if needed')
    config = subs.add_parser('conf', help='get crypto config from .h file')
    config.add_argument('-d', '--define', type=str, default='SECBOOT_CRYPTO_SCHEME')
    config.add_argument("infile")
    extract = subs.add_parser('extract', help='get value definition from text file')
    extract.add_argument('-d', '--define', type=str, help='defintion searched in file')
    extract.add_argument("infile")
    enc = subs.add_parser('enc', help='encrypt an image with a private key')
    enc.add_argument('-k', '--key', metavar='filename', required = True)
    enc.add_argument('-n', '--nonce', metavar='filename')
    enc.add_argument('-i', '--iv', metavar='filename')
    enc.add_argument('-a', '--address', type=auto_int, help='part of IV for AES CTR. address = Slot0_FW_Start_Address[31:4]')
    enc.add_argument('--poffset', help ='file that contains offset at which the partial firmware should be applied', type=str, metavar='filename')
    enc.add_argument("infile")
    enc.add_argument("outfile")

    head = subs.add_parser('header', help='build  installed header file and compute mac according to provided key')
    head.add_argument('-k', '--key', metavar='filename', required = True)
    head.add_argument('-n', '--nonce', metavar='filename')
    head.add_argument('-i', '--iv', metavar='filename')
    head.add_argument('-f', '--firmware', metavar='filename', required = True)
    head.add_argument('-t', '--tag', metavar='filename', required = True)
    head.add_argument('-v', '--version',type=int, required = True)
    head.add_argument('-m', '--magic',type=str, default="SFUM")
    head.add_argument('-p', '--protocol',type=int,  default = 0x1)
    head.add_argument('-r', '--reserved',type=int, default=8)
    head.add_argument('-o', '--offset', type=int, default = 512, required = False)
    head.add_argument('--cert_fw_leaf', metavar='filename', required = False)
    head.add_argument('--cert_fw_inter', metavar='filename', required = False)
    head.add_argument('--pfw', help ='partial firmware', metavar='filename', required = ('--poffset' in sys.argv) or ('--ptag' in sys.argv))
    head.add_argument('--poffset', help ='file that contains offset at which the partial firmware should be applied', type=str, metavar='filename', required = ('--pfw' in sys.argv) or ('--ptag' in sys.argv))
    head.add_argument('--ptag', metavar='filename', required = ('--pfw' in sys.argv) or ('--poffset' in sys.argv))
    head.add_argument("outfile")
    pack = subs.add_parser('pack', help='build header file and compute mac according to key provided')
    pack.add_argument('-k', '--key', metavar='filename', required = True)
    pack.add_argument('-n', '--nonce', metavar='filename')
    pack.add_argument('-i', '--iv', metavar='filename')
    pack.add_argument('-f', '--firmware', metavar='filename', required = True)
    pack.add_argument('-t', '--tag', metavar='filename', required = True)
    pack.add_argument('-v', '--version',type=int, required = True)
    pack.add_argument('-m', '--magic',type=str, default="SFUM")
    pack.add_argument('-p', '--protocol',type=int, default = 0x1)
    pack.add_argument('-r', '--reserved',type=int, default=8)
    pack.add_argument('-o', '--offset', help='offset between start of header and binary', type=int, default=512)
    pack.add_argument('-e', '--elf', help='elf type set to 1 for GNU, 0 for other by default', type=int, default=1)
    pack.add_argument('--cert_fw_leaf', metavar='filename', required = False)
    pack.add_argument('--cert_fw_inter', metavar='filename', required = False)
    pack.add_argument('--pfw', help ='partial firmware', metavar='filename', required = ('--poffset' in sys.argv) or ('--ptag' in sys.argv))
    pack.add_argument('--poffset', help ='file that contains offset at which the partial firmware should be applied', type=str, metavar='filename', required = ('--pfw' in sys.argv) or ('--ptag' in sys.argv))
    pack.add_argument('--ptag', metavar='filename', required = ('--pfw' in sys.argv) or ('--poffset' in sys.argv))
    pack.add_argument("outfile")
    
    diff = subs.add_parser('diff', help='compute differences between 2 binary files ')
    diff.add_argument('-1', '--file1', type=str, metavar='filename', required=True, help="first binary file to compare")
    diff.add_argument('-2', '--file2', type=str, metavar='filename', required=True, help="second binary file to compare")
    diff.add_argument('-p', '--poffset', type=str, metavar='filename', required=True, help="file that will contain offset at which the difference binary files should be applied")
    diff.add_argument('-b', '--begin', type=auto_int, metavar='filename', default='0x0', required=False, help="offset from which beginning comparison - in bytes (default: 0)")
    diff.add_argument('-e', '--end',   type=auto_int, metavar='filename', default='0x0', required=False, help="offset at which ending comparison - in bytes. use 0 to specify end of given binary files (default: 0)")
    diff.add_argument('-a', '--align', type=int, metavar='align', default=2, required=False, help="difference binary file alignment in bytes (default: 2)")
    diff.add_argument("outfile")
    
    mrg = subs.add_parser('merge', help='merge elf appli , install header and sbsfu.elf in a contiguous binary')
    mrg.add_argument('-i', '--install', metavar='filename',  help="filename of installed binary header", required = True)
    mrg.add_argument('-s', '--sbsfu', metavar='filename', help="filename of sbsfu elf", required = True)
    mrg.add_argument('-l', '--loader', metavar='filename', help="filename of loader elf", required = False)
    mrg.add_argument('-v', '--value', help= "byte padding pattern", required = False, type=int, default=0xff)
    mrg.add_argument('-p', '--padding', help='pad to add to appli binary, a multiple of the given size if needed',type=int, required = False, default=0)
    mrg.add_argument('-e', '--elf', help='elf type set to 1 for GNU, 0 for other by default', type=int, default=1) 
    mrg.add_argument('-x', '--header', type=auto_int, help='Header address, when header not contiguous with FW',required = False)
    mrg.add_argument('-u', '--userapp', metavar='filename', help="filename of appli elf file", required = False)
    mrg.add_argument("outfile", help = "filename of contiguous binary")
    
    mrgv2 = subs.add_parser('mergev2', help='merge elf files and binaries in a contiguous binary')
    mrgv2.add_argument('-b', '--binaries', type=str,  help="binaries to merge files list (ex:'f1.bin@0x08004500;f1.bin@0x08006000'", required = True)
    mrgv2.add_argument('-f', '--files', type=str,  help="elf files to merge list (ex:'f1.elf;f2.elf'", required = False)
    mrgv2.add_argument('-v', '--value', help= "byte padding pattern", required = False, type=int, default=0xff)
    mrgv2.add_argument('-e', '--elf', help='elf type set to 1 for GNU, 0 for other by default', type=int, default=1) 
    mrgv2.add_argument("outfile", help = "filename of contiguous binary")

    app = subs.add_parser('append', help='append elf appli and install header to an existing binary')
    app.add_argument('-i', '--install', metavar='filename',  help="filename of installed binary header", required = True)
    app.add_argument('-b', '--binary', metavar='filename', help="filename of existing binary", required = True)
    app.add_argument('-v', '--value', help= "byte padding pattern", required = False, type=int, default=0xff)
    app.add_argument('-e', '--elf', help='elf type set to 1 for GNU, 0 for other by default', type=int, default=1) 
    app.add_argument('-x', '--header', type=auto_int, help='Header address',required = False)
    app.add_argument('-a', '--address', type=auto_int, help='Base address of input binary',required = False)
    app.add_argument('-u', '--userapp', metavar='filename', help="filename of user application elf file", required = False)
    app.add_argument("outfile", help = "filename of contiguous binary")

    inject =  subs.add_parser('inject', help='inject key to KMS embedded keys code')
    inject.add_argument('-k', '--key', metavar='filename', required=('--cert' not in sys.argv) and ('-c' not in sys.argv))
    inject.add_argument('-c', '--cert', metavar='filename', help = "Certificate .der file", required=('--key' not in sys.argv) and ('-k' not in sys.argv))
    inject.add_argument('-p', '--pattern', type=str, required=True)
    inject.add_argument('-f', '--file', metavar='filename', required=True)
    inject.add_argument('-t', '--type', type=str, default="public")
    inject.add_argument("outfile", help = "generated output file")

    args = parser.parse_args()
    if args.subcmd is None:
        print('Must specify a subcommand')
        sys.exit(1)
    subcmds[args.subcmd](args)

if __name__ == '__main__':
    args()

