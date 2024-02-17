file = open("homepage.html",'r')
for line in file:
    print('output += "'+ str(line.rstrip("\n").replace('"','\\"')+'\\n";'))