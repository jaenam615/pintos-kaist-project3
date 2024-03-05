#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* OS가 부팅된 이후 타이머의 틱수. */
static int64_t ticks;

/* 타이머 틱당 루프의 수
   timer_calibrate().에 의해 초기화 된다. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT)     pit = 프로그래밍된 카운트에 도달할 때 출력 신호를 생성하는 카운터
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. 
   초당 PIT_FREQ를 인터럽트하고 해당 인터럽트를 등록시킨다.*/
void
timer_init (void) {
	/* 8254 입력 주파수를 TIMER_FREQ로 나눈 값을 가장 가까운 값으로 반올림합니다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
// 짧은 지연을 구현하는데 사용하는 loops_per_tick을 보정한다.
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	// 적절한 loops_per_tick을 2의 제곱형태로 보여준다. << 여전히 timer tick보다 작다.
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
// OS 부팅 후 타이머 틱 수를 반환한다.
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
// timer_ticks에 의해 반환된 값, 이후 경과한 타이머 틱수를 반환한다.
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
// TICKS timer ticks를 대략적으로 실행을 중지한다.
void
timer_sleep (int64_t ticks) {
	
	int64_t start = timer_ticks ();
<<<<<<< HEAD
=======

	ASSERT(intr_get_level() == INTR_ON);

	thread_sleep(ticks+start);

	// timer_interrupt(thread_current());

	//INTR OFF 
	//현재 실행되고 있는 쓰레드를 반환 thread_current()
	// struct thread *waiting;
	// // enum intr_level 
	// printf("<3>\n");

	// waiting = thread_current();
	// enum intr_level old_level;
	// printf("%d", old_level);
	// printf("\n<4>\n");

	// intr_set_level(old_level);
	// ASSERT (intr_get_level () == INTR_OFF);
	// printf("\n<5>\n");
	// thread_block();
	// printf("\n<6>\n");
	// //시간 확인 방법
	// // while (timer_elapsed (start) < ticks){
	// // 	continue;
	// // }
	// thread_unblock(waiting);
	// printf("<7>\n");	

	//thread_unblock(뭘 언블락할지 명시해줘야함)

>>>>>>> 25b98e7e9efa10b0548a93fe432b06106265e500
	// ASSERT (intr_get_level () == INTR_ON);
	mutex_sleep () ;
}

/* Suspends execution for approximately MS milliseconds. */
// 약 ms 밀리초 동안 실행을 중단한다.
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
// 약 us 마이크로 초 동안 실행을 중지한다.
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
// 약 나노 초 동안 실행을 중지한다.
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
// 타이머의 통계를 프린트한다.
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
// 인터럽트 핸들러
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();

	//if (list_front(&sleep_list)
	thread_wakeup(ticks);
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.
// 짧은 지연을 구현하기 위해서 간단한 루프를 LOOPS 시간 만큼 반복한다.


   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
// 코드 정렬이 타이밍에 상당한 영향을 미칠 수 있기 때문에 NO_INLINE으로 표시합니다. 따라서 이 함수를 다른 위치에 다르게 표시하면 결과를 예측하기가 어렵습니다.

static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
// 약 몇 초 동안 절전
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
