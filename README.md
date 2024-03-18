# wp

Is a tool to check the parenthesis in a file and to navigate in it. It checks just the basic rules open-close do not check hierarchy ({}) an do not match sequences like ([)]. I am going to complete the implement these functionalities in a second moment. <br>
The resulting file can be opened and loaded in a SQLite database with my other git,qLite, running the tool WP.<br>
The resulting file is composed so composed:<br>

<title>##<autohr>##<p1><integer1>...<pn><integern>\n
where pn is a signle characther rappresenting the parenthesis, and integer is number composed as follow row*10000+col;


