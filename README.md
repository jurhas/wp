# wp

Is a tool to check the parenthesis in a file or in a directory and to navigate the resulting file. It checks just the basic rules open-close do not check hierarchy ({}) an do not match sequences like ([)]. I am going to implement these functionalities in a second moment. <br>
The resulting file can be opened and loaded in a SQLite database with my other git,[qLite](../qLite), running the tool `WP`.<br>
The resulting file is so composed:<br>

`<title>##<autohr>##<p-1><integer-1>...<p-n><integer-n>\n`

where `<p-n>` is a single characther rappresenting the parenthesis, and `<integer-n>` is a number composed as follow `<integer-n>=row*10000+col`
To build it in Linux just run the Makefile.<br>
To build it in Windows you will need an SDK. Create an empty project, include the C files and run it.<br>
The library `mparser`, hier included is not maintened, I just use the stack. If you do not uses other library, you do not need to check for updates<br
The file `par.txt` is a sample file and it the check of the italian wiki dump.<br>






