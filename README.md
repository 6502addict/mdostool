# mdostool / mdos extract
series of tools to convert mdos disk image files / list directory / insert or export mdos file

mdosextract.c is working find and extract the content of the imd mdos image file
binary files are saved as they are
text files are saved twice  once as they are and and one with the texts decoded

example xtrek.sa is saved as xtrek.sa (not decoded) and xtrek.sa.txt (decoded and readable on a PC)


mdostool was created  to perform a series of operations

MDOS Filesystem Utility v1.1\n");<br>
Usage: mdostool <mdos-disk-image> [command] [args...]<br>
Commands:<br>
  ls                    - List directory contents\n");<br>
  cat <filename>        - Display file contents (with ASCII conversion)\n");<br>
  rawcat <filename>     - Display raw file contents (no conversion)\n");<br>
  get <filename> [out]  - Export file from MDOS to local filesystem\n");<br>
  put <local> [mdos]    - Import file from local to MDOS filesystem\n");<br>
  mkfs <sides>          - Create new MDOS filesystem (1=single, 2=double sided)\n");<br>
  seek <filename>       - Test seek operations on file\n");<br>
  info <filename>       - Show detailed file information\n");<br>
  free                  - Show free space information\n");<br>
  rm <filename>         - Delete file from MDOS filesystem\n");<br>
Image Conversion Commands:<br>
  imd2dsk <input.imd> <output.dsk> - Convert IMD to DSK format\n");<br>
  dsk2imd <input.dsk> <output.imd> - Convert DSK to IMD format\n");<br>
Examples:<br>
  mdostool disk.dsk ls\n", program_name);<br>
  mdostool disk.dsk cat readme.txt\n", program_name);<br>
  mdostool disk.dsk put myfile.txt\n", program_name);<br>
  mdostool disk.dsk get data.bin exported.bin\n", program_name);<br>
  mdostool newdisk.dsk mkfs 2\n", program_name);<br>
  mdostool - imd2dsk disk.imd disk.dsk\n", program_name);<br>
  mdostool - dsk2imd disk.dsk disk.imd\n", program_name);<br>


beware that it's work in progress some functions are not fully tested<br>

the conversion tools works fine<br>
the extraction and cat works fine<br>



known bugs:
  - put is saving with the wrong type and attributes
  - rm not tested

if you have problems emails me at:

didier@aida.org
