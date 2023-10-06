#
# @(#) EnduroX Persistent Queue Configuration
#
@,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n
LTESTA,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo
LTESTB,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo
LTESTC,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo
TEST1
# Where to put bad messages
DEADQ
# Where to put OK replies:
REPLYQ
# Auto Q 1, for OK service
OKQ1,svcnm=SVCOK,autoq=y,tries=5,waitinit=1,waitretry=2,waitretryinc=2,waitretrymax=10,memonly=n
# Bad service
BADQ1,svcnm=SVCFAIL,autoq=y,tries=3,waitinit=1,waitretry=2,waitretryinc=2,waitretrymax=3,memonly=n
# Random Bad, transactional
RFQ,svcnm=FAILRND,autoq=T,tries=10,waitinit=0,waitretry=0,waitretrymax=0,memonly=n

#
# corrid tests with fifo
#
CORFIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n

#
# corrid tests with lifo
#
CORLIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo

#
# corrid tests with autoq + errorq
#
CORAUTO,svcnm=-,autoq=y,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,errorq=CORERR

#
# correlator error q
#
CORERR,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n

#
# future tests
#
FUT_FIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n
FUT_LIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo

#
# future corrid tests with fifo/lifo
#
FUT_CORFIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n
FUT_CORLIFO,svcnm=-,autoq=n,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,mode=lifo

# will send delayed messages to CLTBCAST service. Which in turn broadcast
# message back to the client. One worker to keep the order.
FUT_FIFO_AUTO,svcnm=CLTBCAST,autoq=y,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,workers=1
FUT_LIFO_AUTO,svcnm=CLTBCAST,autoq=y,waitinit=0,waitretry=0,waitretrymax=0,memonly=n,workers=1,mode=lifo
