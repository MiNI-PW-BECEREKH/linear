CC=gcc
CFLAGS= -std=gnu99 -Wall -lpthread -lrt

all: linear.stage1 linear.stage2 linear.stage3 linear.stage4 linear.stage5 linear.stage6

linear.stage1: linear.stage1.c
	${CC} ${CFLAGS} -o linear.stage1 linear.stage1.c 

linear.stage2: linear.stage2.c
	${CC} ${CFLAGS} -o linear.stage2 linear.stage2.c 

linear.stage3: linear.stage3.c
	${CC} ${CFLAGS} -o linear.stage3 linear.stage3.c 

linear.stage4: linear.stage4.c
	${CC} ${CFLAGS} -o linear.stage4 linear.stage4.c 

linear.stage5: linear.stage5.c
	${CC} ${CFLAGS} -o linear.stage5 linear.stage5.c 

linear.stage6: linear.stage6.c
	${CC} ${CFLAGS} -o linear.stage6 linear.stage6.c 

# udpfwd.stage3: udpfwd.stage3.c
# 	${CC} ${CFLAGS} -o udpfwd.stage3 udpfwd.stage3.c

# udpfwd.stage4: udpfwd.stage4.c
# 	${CC} ${CFLAGS} -o udpfwd.stage4 udpfwd.stage4.c   

# udpfwd.stage5: udpfwd.stage5.c
# 	${CC} ${CFLAGS} -o udpfwd.stage5 udpfwd.stage5.c  

# udpfwd.stage6: udpfwd.stage6.c
# 	${CC} ${CFLAGS} -o udpfwd.stage6 udpfwd.stage6.c  



.PHONY: clean all
clean:
	rm linear.stage1 linear.stage2 linear.stage3 linear.stage4 linear.stage5 linear.stage6
    # rm prog1_stage1 prog1_stage2 prog2_stage2 prog1_stage3 prog2_stage3 prog1_stage4 prog2_stage4 prog1_stage5 prog2_stage5 prog1_stage6 prog2_stage6