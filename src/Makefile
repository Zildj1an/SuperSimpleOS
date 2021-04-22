all:
	# COMMENT NEXT LINE FOR WRITER
	#cat make2 > make.sh
	# UNCOMMENT NEXT LINE FOR WRITER
	cat make1 > make.sh 
	./make.sh
	# COMMENT NEXT LINE FOR WRITER
	#sudo xl create /etc/code-other.cfg
	# UNCOMMENT NEXT LINE FOR WRITER
	#sudo xl create /etc/xen/code-hvm.cfg
clean:
	rm *.img *.o *.efi *.lib *.dll kernel &> /dev/null
	rm -rf uefi_iso_image &> /dev/null
	#sudo xl list
