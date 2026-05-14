with open("C:/Users/mkg/Downloads/Aquaware-main/Aquaware-main/build/AquaWare/AquaWare.sys", "rb") as f:
    data = f.read()
    
print("static std::vector<uint8_t> image = {")
for i, b in enumerate(data):
    if i % 16 == 0:
        print("\n    ", end="")
    print(f"0x{b:02X}, ", end="")
print("\n};")