# Turns an html file into a String that can easily be pasted into the arduino IDE
file = open("Firmware-rev-2-board/homepage.html",'r')
for line in file:
    print('output += "'+ str(line.rstrip("\n").replace('"','\\"')+'\\n";'))
