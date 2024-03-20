Krafton Jungle 
Week 7 Team 6 WIL

Project 2 User Programs:

---

Argument Passing

---
| Address	 |  Name	      |  Data	   | Type       |  
|---|---|---|---|
|0x4747fffc	 |  argv[3][...]  | 'bar\0'	   |char[4]     |     
|0x4747fff8	 |   argv[2][...] | 'foo\0'	   |char[4]     |     
|0x4747fff5	 |   argv[1][...] | '-l\0'	   |char[3]     |      
|0x4747ffed	 |   argv[0][..|.]|	'/bin/ls\0'|char[8]     |    
|0x4747ffe8	 |   word-align	  | 0	       |uint8_t[]   |      
|0x4747ffe0	 |   argv[4]	  | 0	       |char *      |     
|0x4747ffd8	 |   argv[3]	  | 0x4747fffc |char *      |      
|0x4747ffd0	 |   argv[2]	  | 0x4747fff8 |char *      |    
|0x4747ffc8	 |   argv[1]	  | 0x4747fff5 |char *      |     
|0x4747ffc0	 |   argv[0]	  | 0x4747ffed |char *      |     
|0x4747ffb8	 |  return address|	0	       |void (*) () |     
---