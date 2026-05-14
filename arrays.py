def init_decrypt_buffer():
    buf = []
    for i in range(256):
        buf.append((((i * 0x5D) ^ 0xA5) + (i >> 2)) & 0xFF)
    return buf

def encrypt_data(data, key):
    buf = init_decrypt_buffer()
    state = key & 0xFF
    result = []
    for i, byte in enumerate(data):
        state ^= buf[i & 0xFF]
        result.append((byte ^ state) & 0xFF)
        state = ((state << 3) | (state >> 5)) ^ buf[(i + key) & 0xFF]
    return result

def encrypt_ntoskrnl(name):
    key = 0x5B
    result = []
    for c in name:
        result.append((c ^ key) & 0xFF)
        key = ((key << 3) | (key >> 5)) ^ 0xA5
    return result

device_name = b'\\Device\\AudioBridge\x00\x00'
link_name = b'\\DosDevices\\AudioBridge\x00\x00'
sddl = b'D:P(A;;GA;;;SY)(A;;GA;;;BA)\x00\x00'
section_name = b'\\BaseNamedObjects\\AudioBridgeSharedMemory\x00\x00'
ntoskrnl = b'ntoskrnl.exe'

print("device_name_enc =", encrypt_data(device_name, 0x5B9E2C7A))
print("link_name_enc =", encrypt_data(link_name, 0x3F8A1D4C))
print("sddl_enc =", encrypt_data(sddl, 0x6C1F9E3A))
print("section_name_enc =", encrypt_data(section_name, 0x7F3A9C4E))
print("ntoskrnl_enc =", encrypt_ntoskrnl(ntoskrnl))