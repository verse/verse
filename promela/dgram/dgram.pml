/**
 * $Id: dgram.pml 734 2011-01-13 14:28:56Z jiri $
 *
 * This PROMELA program simulates and helps to verify the datagrame
 * connection of Verse protocol.
 *
 * author: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 * This verification model works only with parameter -m, because spin does not
 * block, when channel is full; instead, message is dropped. This parameter
 * has to be used during simulation as well for generating verifier!
 *
 */

/* Queue length */
#define QLEN 3
/* Maximal number of connection/teardown attempts */
#define	MAX_ATTEMPTS 3
/* Number of payload packets sent by client */
#define N 2
/* Consider peer as death after 10 sent payload packets and no response
 * from peer */
#define TIMEOUT 10

proctype Client(chan in; chan out)
{
	/* Receive bit flags */
	bit rPAY=0, rACK=0, rSYN=0, rFIN=0;
	/* Send bit flags */
	bit sPAY=0, sACK=0, sSYN=0, sFIN=0;
	/* Receive PayID, AckID and MSG */
	short rPAY_ID=0, rACK_ID=0, rMSG=0;
	/* Send PayID, AckID and MSG */
	short sPAY_ID=0, sACK_ID=0, sMSG=0;
	
	/* Temporary variables */
	short conn=0;	/* Number of connection attempts */
	short f_pay;	/* Number of first payload packet */
	short timer=0;
	bit want_close=0;

	/* Handshake */
closed:
	printf("Client: CLOSED\n");
	
	/* First phase of handshake */
request:
	printf("Client: REQUEST\n");
	sPAY=1;	sSYN=1;
request_again:
	if
	/* Max number of connection attemps reached. */
	:: (conn >= MAX_ATTEMPTS) -> { goto end; };
	:: else -> atomic {
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
			conn++;
		};
	fi;

	do
	:: (nempty(in)) -> atomic {
		in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
			if
			:: (rPAY==1 && rACK==1 && rSYN==1 && rMSG==sPAY_ID) -> { goto partopen; };
			/* Simulation of packet loss */
			:: (1) -> printf("Packet loss\n");
			fi;
		};
	:: timeout -> atomic { goto request_again; };
	od;

	/* Second phase of handshake */
partopen:
	printf("Client: PARTOPEN\n");
	sSYN=0;	sACK=1;
	sPAY_ID=1;
	sMSG=rPAY_ID;
	conn=0;
partopen_again:
	if
	:: (conn >= MAX_ATTEMPTS) -> atomic {
			printf("Client: max attempts reached\n");
			goto end_failure;
		};
	:: else -> atomic {
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
			conn++;
		};
	fi;

	do
	:: (nempty(in)) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
			if
			:: (rPAY==1 && rACK==1 && rMSG==sPAY_ID) -> { goto open; };
			/* Simulation of packet loss */
			:: (1) -> printf("Packet loss/drop\n");
			fi;
		};
	:: timeout -> goto partopen_again;
	od;

	/* Handshake finished, normal communication */
open:
	printf("Client: OPEN\n");
	sMSG=rPAY_ID;

	do
	:: timer++;
		if
		/* Send payload packet */
		:: (nfull(out) && timer<TIMEOUT) -> atomic {
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG; sPAY_ID++;
			};
		/* Receive packet */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
				timer=0;
				do
				:: (rACK==1) -> { 
						rACK=0;
						if
						:: (rMSG>=N) -> { goto closing; };
						:: else -> skip;
						fi;
					};
				:: (rFIN==1) -> { rFIN=0; sFIN=1; want_close=1; };
				:: else -> {
					if
					:: (want_close==1) -> { sMSG=rPAY_ID; goto closing; };
					:: else -> skip;
					fi;
					if
					:: (rPAY==1) -> {
							sMSG=rPAY_ID;
							sACK=1;
							/* Send acknowledgment packet */
							out!0,sACK,sSYN,sFIN,0,sACK_ID,sMSG;
							sACK_ID++;
						};
					:: else -> skip;
					fi;
					break;
					};
				od;
			};
		/* Simulation of packet loss */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
				in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
				printf("Packet loss\n");
			};
		/* Other cases */
		:: (empty(in) && full(out) && timer<TIMEOUT) -> skip;
		/* No response from server */
		:: (timer>=TIMEOUT) -> goto end_failure;
		fi;
	od;

	/* Teardown */
closing:
	printf("Client: CLOSING\n");
	sPAY=1; sACK=1; sSYN=0; sFIN=1;
	conn=0;
closing_again:
	if
	/* Server died during teardown, end. */
	:: (conn >= MAX_ATTEMPTS) -> atomic {
			printf("Client: max attempts reached\n");
			goto end_failure;
		};
	:: else -> atomic {
			conn++;
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
		};
	fi;

	do
	/* Receive packet */
	:: (nempty(in)) -> atomic {
		in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
		if
		:: (rFIN==1 && rACK==1 && rMSG==sPAY_ID) -> { goto end_closed; };
		/* Simulation of packet loss */
		:: (1) -> printf("Packet loss/drop\n");
		fi;
		};
	:: timeout -> atomic { goto closing_again; };
	od;

end_closed:
	printf("Client: CLOSED\n");
	goto end_success;

end_failure:
	printf("Client: failure\n");
	do
	:: in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
	:: timeout -> goto end;
	od;

end_success:
	printf("Client: success\n");
	do
	:: in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
	:: timeout -> goto end;
	od;

end:
	skip;
}

proctype Server(chan in; chan out)
{
	/* Receive bit flags */
	bit rPAY=0, rACK=0, rSYN=0, rFIN=0;
	/* Send bit flags */
	bit sPAY=0, sACK=0, sSYN=0, sFIN=0;
	/* Receive PayID, AckID and MSG */
	short rPAY_ID=0, rACK_ID=0, rMSG=0;
	/* Send PayID, AckID and MSG */
	short sPAY_ID=0, sACK_ID=0, sMSG=0;
	
	/* Temporary variables */
	short fr_PAY_ID=0, fs_PAY_ID=0;
	short timer=0, count=0;
	bit want_close=0;

	/* Handshake */
	
	/* First phase of handshake */
listen:
	printf("Server: LISTEN\n");
	count=0;
	
	do
	/* Receive packet */
	:: (nempty(in)) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
			/* Simulation of 30 second waiting in this state */
			if
			:: (count>=MAX_ATTEMPTS) -> goto end_failure;
			:: else -> count++;
			fi;
			if
			:: (rSYN==1) -> {
					/* Store first received payload ID */
					fr_PAY_ID=rPAY_ID;
					goto respond;
				};
			/* Simulation of packet loss */
			:: (1) -> printf("Packet loss\n");;
			fi;
		};
	:: timeout -> goto end_failure;
	od;

	/* Second phase of handshake */
respond:
	printf("Server: RESPOND\n");
	sPAY=1; sSYN=1; sACK=1;
	sMSG=rPAY_ID;
	count=0;
	
	/* Send response to the first packet */
	out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
	/* Store ID of first sent payload packet */
	fs_PAY_ID=sPAY_ID;

	do
	/* Receive packet */
	:: (nempty(in)) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
			if
			:: (count>=MAX_ATTEMPTS) -> goto end_failure;
			:: else -> count++;
			fi;
			if
			/* Still send response to the packet sent in REQUEST state */
			:: (rSYN==1 && rPAY_ID==fr_PAY_ID) -> {
					out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
				};
			/* If received packet was sent from PARTOPEN state, then switch
			 * to the OPEN state */
			:: (rACK==1 && rMSG==fs_PAY_ID) -> atomic {
					goto open;
				};
			/* Simulation of packet loss */
			:: (1) -> printf("Packet loss\n");
			fi;
		};
	:: timeout -> goto end_failure;
	od;

	/* End of handshake; normal communication */
open:
	printf("Server: OPEN\n");
	sSYN=0;
	sMSG=rPAY_ID;
	sPAY_ID = fs_PAY_ID+1;
	timer=0;
	
	/* Send response to the second packet */
	out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
	sPAY_ID++;

	do
	:: timer++;
		if
		/* Send payload packet */
		:: (nfull(out) && timer<TIMEOUT) -> atomic {
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
			sPAY_ID++;
			};
		/* Receive packet */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
			timer=0;
			do
			:: (rACK==1) -> {
					rACK=0;
					if
					:: (rMSG>=N) -> { goto closereq; };
					:: else -> skip;
					fi;
				};
			:: (rFIN==1) -> { rFIN=0; want_close=1; };
			:: else -> {
					if
					:: (want_close==1) -> { sMSG=rPAY_ID; goto closed; };
					:: else -> skip;
					fi;
					if
					:: (rPAY==1) -> {
							sMSG=rPAY_ID;
							sACK=1;
							/* Send acknowledgment packet */
							out!0,sACK,sSYN,sFIN,0,sACK_ID,sMSG;
							sACK_ID++;
						};
					:: else -> skip;
					fi;
					break;
					};
			od;
			};
		/* Simulation of packet loss */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
				printf("Packet loss\n");
			};
		/* Other cases */
		:: (empty(in) && full(out) && timer<TIMEOUT) -> skip;
		/* No response from client */
		:: (timer>=TIMEOUT) -> goto end_failure;
		fi;
	od;

	/* Taerdown */
closereq:
	printf("Server: CLOSEREQ\n");
	sFIN=1;
	count=0;
	timer=0;

	do
	:: timer++;
		if
		/* Send payload packet */
		:: (nfull(out) && timer<TIMEOUT) -> atomic {
			out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
			sPAY_ID++;
			};
		/* Receive packet */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
			timer=0;
			do
			:: (rACK==1) -> { rACK=0; };
			:: (rFIN==1) -> { rFIN=0; want_close=1; };
			:: else -> {
					if
					:: (want_close==1) -> { sMSG=rPAY_ID; goto closed; };
					:: else -> skip;
					fi;
					if
					:: (rPAY==1) -> {
							sMSG=rPAY_ID;
							sACK=1;
							/* Send acknowledgment packet */
							out!0,sACK,sSYN,sFIN,0,sACK_ID,sMSG;
							sACK_ID++;
						};
					:: else -> skip;
					fi;
					break;
					};
			od;
			/* If client ignores FIN flag, then terminate connection */
			if
			:: (count>=MAX_ATTEMPTS) -> goto end_failure;
			:: else count++;
			fi;
			};
		/* Simulation of packet loss */
		:: (nempty(in) && timer<TIMEOUT) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG ->
				printf("Packet loss\n");
			};
		/* Other cases */
		:: (empty(in) && full(out) && timer<TIMEOUT) -> skip;
		/* No response from client */
		:: (timer>=TIMEOUT) -> goto end_failure;
		fi;
	od;

closed:
	printf("Server: CLOSED\n");
	sPAY=1; sACK=1; sSYN=0; sFIN=1;
	count=0;
	
	/* Send response to payload packet with FIN flag */
	out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
	
	do
	:: (nempty(in)) -> atomic {
			in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
			if
			:: (count>=MAX_ATTEMPTS) -> goto end_failure;
			:: else -> count++;
			fi;
			if
			:: (rPAY==1 && rFIN==1) -> atomic {
					sMSG=rPAY_ID;
					out!sPAY,sACK,sSYN,sFIN,sPAY_ID,sACK_ID,sMSG;
				}
			/* Simulation of packet loss */
			:: (1) -> printf("Packet loss\n");
			fi;
		};
	:: timeout -> { goto end_success; };
	od;

end_failure:
	printf("Server: failure\n");
	do
	:: in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
	:: timeout -> goto end;
	od;

end_success:
	printf("Server: success\n");
	do
	:: in?rPAY,rACK,rSYN,rFIN,rPAY_ID,rACK_ID,rMSG;
	:: timeout -> goto end;
	od;

end:
	skip;
}

/* Main process */
init
{
	/*                   PAY, ACK, SYN, FIN, PAY_ID ACK_ID MSG */
	chan q1 = [QLEN] of {bit, bit, bit, bit, short, short, short};
	chan q2 = [QLEN] of {bit, bit, bit, bit, short, short, short};

	atomic {run Server(q1, q2); run Client(q2, q1);};
}
