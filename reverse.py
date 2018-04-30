
s = input("Enter the string: ")
o = ""
for i in range(len(s), 0, -2):
	o += s[i-2:i]
print(o + "\n")
