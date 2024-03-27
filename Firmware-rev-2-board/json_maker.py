file = open("key_dict","r")

output = []
for line in file:
    # print(line.lstrip("#define"))
    splitL = line.lstrip("#define ").split(" ")
    # output[splitL[0]] = int(splitL[1].rstrip("\n"),16)
    # output.append(splitL[0])
    print(splitL[0])

# print(output)
