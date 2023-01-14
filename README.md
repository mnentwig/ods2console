# ods2console
Extracts the XML body from an open-office spreadsheet (which is a .zip file internally), traverses the hierarchy and dumps contents from all sheets to the console

Actual functionality is extremely basic, e.g. no quoting of commas or multiline strings in the output. Intended as code template, not as command line utility.

The only library dependency is ubiquitous libz (-lz)

Other libraries (tinyxml2, minizip from libz source) are included as source and come under their own license.