# mdostool / mdos extract
series of tools to convert mdos disk image files / list directory / insert or export mdos file

mdosextract.c is working find and extract the content of the imd mdos image file
binary files are saved as they are
text files are saved twice  once as they are and and one with the texts decoded

example xtrek.sa is saved as xtrek.sa (not decoded) and xtrek.sa.txt (decoded and readable on a PC)


mdostool was created  to perform a series of operations

MDOS Filesystem Utility v1.1\n");
Usage: mdostool <mdos-disk-image> [command] [args...]
Commands:
  ls                    - List directory contents\n");
  cat <filename>        - Display file contents (with ASCII conversion)\n");
  rawcat <filename>     - Display raw file contents (no conversion)\n");
  get <filename> [out]  - Export file from MDOS to local filesystem\n");
  put <local> [mdos]    - Import file from local to MDOS filesystem\n");
  mkfs <sides>          - Create new MDOS filesystem (1=single, 2=double sided)\n");
  seek <filename>       - Test seek operations on file\n");
  info <filename>       - Show detailed file information\n");
  free                  - Show free space information\n");
  rm <filename>         - Delete file from MDOS filesystem\n");
Image Conversion Commands:
  imd2dsk <input.imd> <output.dsk> - Convert IMD to DSK format\n");
  dsk2imd <input.dsk> <output.imd> - Convert DSK to IMD format\n");
Examples:
  mdostool disk.dsk ls\n", program_name);
  mdostool disk.dsk cat readme.txt\n", program_name);
  mdostool disk.dsk put myfile.txt\n", program_name);
  mdostool disk.dsk get data.bin exported.bin\n", program_name);
  mdostool newdisk.dsk mkfs 2\n", program_name);
  mdostool - imd2dsk disk.imd disk.dsk\n", program_name);
  mdostool - dsk2imd disk.dsk disk.imd\n", program_name);


beware that it's work in progress some functions are not fully tested

the conversion tools works fine
the extraction and cat works fine



known bugs:
  - put is saving with the wrong type and attributes
  - rm not tested

if you have problems emails me at:

didier@aida.org
