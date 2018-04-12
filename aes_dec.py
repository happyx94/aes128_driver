# -*- coding: utf-8 -*-
"""
Created on Tue Apr 10 16:31:36 2018

@author: Larry Lai
"""
from Crypto.Cipher import AES
from binascii import hexlify, unhexlify

def print_plaintext(plaintext):
    print('Raw plaintext: \n')
    print (hexlify(plaintext))
    print('\nPlaintext in ASCII:\n')
    print (plaintext.decode('ascii'))


def reverse_blocks(text, block_len = 16):
    result = bytes()
    for i in range(0, int(len(text)/block_len)):
        result += text[block_len*i:block_len*(i+1)][::-1]
    return result
        
def aes_decrypt(ciphertext, key, iv):
    ciphertext = reverse_blocks(ciphertext, 16)
    decipher = AES.new(key,AES.MODE_CBC,IV)
    plaintext = decipher.decrypt(ciphertext)
    plaintext = reverse_blocks(plaintext)
    return plaintext

def aes_decrypt_file(in_path, out_path, key, iv):
    infile = open(in_path, "rb")
    print('Reading input file...\n')
    ciphertext = infile.read()
    infile.close()
    if len(ciphertext) % 16 != 0:
        print ('[Error] Length of the ciphertext is not right!\n')
        return None
    print('Start decryption...\n')
    plaintext = aes_decrypt(ciphertext, key, iv)
    outfile = open(out_path, "wb")
    print('Writing to the output path...\n')
    outfile.write(plaintext)
    outfile.close()
    return plaintext

key = unhexlify('00000001000000000000000000000000')
IV = unhexlify( '00000000000000000000000000000000')
plaintext = aes_decrypt_file('secretpic.jpg', 'notsecretpic.jpg', key, IV)
#print_plaintext(plaintext)
