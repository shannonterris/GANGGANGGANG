# invoke SourceDir generated makefile for empty.pem4f
empty.pem4f: .libraries,empty.pem4f
.libraries,empty.pem4f: package/cfg/empty_pem4f.xdl
	$(MAKE) -f C:\Users\shann\Documents\2021\GANGGANGGANG\empty_EK_TM4C1294XL_TI/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\shann\Documents\2021\GANGGANGGANG\empty_EK_TM4C1294XL_TI/src/makefile.libs clean

