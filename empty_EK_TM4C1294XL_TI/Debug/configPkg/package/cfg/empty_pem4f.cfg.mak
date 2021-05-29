# invoke SourceDir generated makefile for empty.pem4f
empty.pem4f: .libraries,empty.pem4f
.libraries,empty.pem4f: package/cfg/empty_pem4f.xdl
	$(MAKE) -f D:\Working\empty_EK_TM4C1294XL_TI/src/makefile.libs

clean::
	$(MAKE) -f D:\Working\empty_EK_TM4C1294XL_TI/src/makefile.libs clean

