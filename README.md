# window-title-spoofer

`Usage: window_title_spoofer [mandatory switches] [options]`

Mandatory Switches:

---

**You must specify either '-t' or '-p' followed by a string of semicolon separated strings.**

search for windows associated with a list of PIDs:

-p "list of semicolon seperated PIDs"

search for windows that have a title including the list of strings:

-t "list of semicolon separated strings"

string to spoof windows to:

-s "string to spoof window title with"

Options

---

-v | only search for visible windows

-x | randomize the window title of the tool


Examples

---
Randomize the tool's window, search **visible-only** windows associated with the PIDs: **1390, 101 and 203** and replace them with **123**

---

`window_title_spoofer -p 1390;101;203 -s "123" -v -x`

Search all windows containing the strings **IDA, HxD or x64** and replace their window title to **spoof**

`window_title_spoofer -t "IDA;HxD;x64" -s "spoof"`
