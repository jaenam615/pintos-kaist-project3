Krafton Jungle 
Week 7 Team 6 WIL

Project 2 User Programs:

---

Argument Passing

---

시스템 콜을 만들기에 앞서 Argument Passing을 구현해야 했다. 
Argument Passing이란, main함수로 들어오는 명령어를 구분하여 인자로 전달하는 것이다.  
우선, init.c main()함수의 read_command_line()함수가 커맨드라인으로 들어오는 인자를 받아와 저장한다.  
(예: -q -f put args-single run 'args-single onearg')
이후 일차적으로 파싱을 통해 -q와 -f등의 플래그들을 제거한 후 남은 부분을 run_actions로 전달해준다.  
run_actions -> run taks 등을 거쳐 args-single onearg를 process_create_initd로 전달한다.  
process_create_initd에서는 미리 fn_copy를 만들어 전체 인자(args-single onearg)를 복사해두고 파일의 이름을 구하기 위해 strktok_r 함수로 파싱한다. 
thread_create로 file_name을 전달해 쓰레드를 만들고, process_exec에는 fn_copy를 전달하여 프로세스(쓰레드)를 실행한다. 

Load함수에서 다시 한 번 strtok_r함수로 들어온 인자를 delimiter(여기의 경우에서는 스페이스)를 기준으로 파싱하여 배열(스택)에 저장한다.  
argument_stack이라는 함수를 만들어 배열에 깃북에서 설명한 방식으로 인자를 쌓는다. 
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

```c
void argument_stack (char **argv, int argc, struct intr_frame *if_){
	
    //포인터를 이동시킬 단위
	int minus_addr;

    //포인터
	int address = if_->rsp;
	
    //인자를 쌓는 반복문 (이 때, 스택은 위에서 아래로 자라기 떄문에 i--로 이동시켜준다)
    for (int i = argc-1; i >= 0;i-- ){
		minus_addr = strlen(argv[i]) + 1; 
		address -= minus_addr;
		memcpy(address, argv[i], minus_addr);
		argv[i] = (char *)address;
	}

    //패딩(word-align)을 통해 8의 배수로 맞춰준다 
    //정렬된 접근은 정렬 안된 접근보다 빠르기 때문이다
	if (address % 8){
		int word_align = address % 8;
		address -= word_align;
		memset(address, 0, word_align);
	}

    //주소의 끝을 알려주는 부분 (위 표에서는 argv[4] / 0 부분에 해당)
	address -= 8;
	memset(address, 0, sizeof(char*));

    //인자의 개수만큼 포인터를 앞으로 당긴 후 모든 주소를 한 번에 넣어준다
	address -= (sizeof(char*) * argc);
	memcpy(address, argv, sizeof(char*) * argc);

    //리턴포인트
	address -= 8;
	memset(address, 0, 8);
	if_->rsp = address;
}
```